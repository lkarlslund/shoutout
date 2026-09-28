package service

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"math"
	"net"
	"strconv"
	"sync"
	"time"

	"github.com/lkarlslund/shoutout/internal/audio"
	"github.com/lkarlslund/shoutout/internal/cast"
	"github.com/lkarlslund/shoutout/internal/config"
	"github.com/lkarlslund/shoutout/internal/discovery"
)

var errTakenOver = errors.New("another controller took over; save settings to reconnect")

type Status struct {
	RequestedDelayMS  int       `json:"requested_delay_ms"`
	ReceiverDelayMS   int       `json:"receiver_delay_ms"`
	AudioFrames       uint64    `json:"audio_frames"`
	AcknowledgedFrame int64     `json:"acknowledged_frame"`
	Retransmits       uint64    `json:"retransmits"`
	FeedbackReports   uint64    `json:"feedback_reports"`
	SinkMuted         bool      `json:"sink_muted"`
	SinkVolume        float64   `json:"sink_volume_percent"`
	PlaybackSeconds   float64   `json:"playback_seconds"`
	State             string    `json:"state"`
	Message           string    `json:"message"`
	Sink              string    `json:"sink"`
	Device            string    `json:"device"`
	PlayerState       string    `json:"player_state"`
	ReceiverVolume    float64   `json:"receiver_volume"`
	ReceiverMuted     bool      `json:"receiver_muted"`
	EncodedBytes      uint64    `json:"encoded_bytes"`
	MediaRequests     uint64    `json:"media_requests"`
	Subscribers       int       `json:"subscribers"`
	Peak              float64   `json:"peak"`
	Updated           time.Time `json:"updated"`
}

type Service struct {
	Discovery     discovery.Browser
	mu            sync.Mutex
	config        config.Config
	path          string
	status        Status
	changed       chan struct{}
	liveChanged   chan struct{}
	sessionCancel context.CancelFunc
}

func New(c config.Config, path string) *Service {
	return &Service{config: c, path: path, changed: make(chan struct{}, 1), liveChanged: make(chan struct{}, 1), status: Status{State: "starting", Sink: audio.SinkName, ReceiverMuted: true}}
}
func (s *Service) Config() config.Config { s.mu.Lock(); defer s.mu.Unlock(); return s.config }
func (s *Service) Status() Status        { s.mu.Lock(); defer s.mu.Unlock(); return s.status }
func (s *Service) Update(c config.Config) error {
	if err := c.Validate(); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := config.Save(s.path, c); err != nil {
		return err
	}

	restart := s.config.RestartRequired(c) || (s.status.State == "idle" && c.Enabled && (c.Host != "" || c.DeviceID != ""))
	s.config = c
	if restart {
		s.status.State = "reconfiguring"
		s.status.PlayerState = ""
		s.status.Message = "Applying stream settings."
		s.status.Updated = time.Now()
		if s.sessionCancel != nil {
			s.sessionCancel()
		}
		select {
		case s.changed <- struct{}{}:
		default:
		}
	} else {
		select {
		case s.liveChanged <- struct{}{}:
		default:
		}
	}
	return nil
}
func (s *Service) state(state, message string) {
	s.mu.Lock()
	s.status.State = state
	if state != "streaming" {
		s.status.PlayerState = ""
		s.status.Subscribers = 0
		s.status.Peak = 0
	}
	s.status.Message = message
	s.status.Updated = time.Now()
	s.mu.Unlock()
	slog.Info("session", "state", state, "message", message)
}
func (s *Service) Run(ctx context.Context) error {
	var sink *audio.Sink

	for {
		if ctx.Err() != nil {
			return nil
		}
		if sink == nil {
			var err error
			sink, err = audio.NewSink(ctx)
			if err != nil {
				s.state("error", err.Error())
				if !s.wait(ctx, 5*time.Second) {
					return nil
				}
				continue
			}
		}
		s.mu.Lock()
		select {
		case <-s.changed:
		default:
		}
		c := s.config
		sessionCtx, cancel := context.WithCancel(ctx)
		s.sessionCancel = cancel
		s.mu.Unlock()
		if !c.Enabled || c.Host == "" && c.DeviceID == "" {
			cancel()
			s.state("idle", "Select a destination and enable streaming in settings.")
			if !s.wait(ctx, 0) {
				return nil
			}
			continue
		}
		err := s.session(sessionCtx, c)
		cancel()
		if ctx.Err() != nil {
			return nil
		}
		if errors.Is(err, context.Canceled) {
			continue
		}
		if errors.Is(err, errTakenOver) {
			s.state("idle", err.Error())
			if !s.wait(ctx, 0) {
				return nil
			}
			continue
		}
		if err != nil {
			s.state("reconnecting", err.Error())
		}
		if !s.wait(ctx, 3*time.Second) {
			return nil
		}
		// Reconcile without resetting desktop routing or volume.
		sink = nil
	}
}
func (s *Service) wait(ctx context.Context, d time.Duration) bool {
	var timer <-chan time.Time
	if d > 0 {
		t := time.NewTimer(d)
		defer t.Stop()
		timer = t.C
	}
	select {
	case <-ctx.Done():
		return false
	case <-s.changed:
		return true
	case <-timer:
		return true
	}
}
func (s *Service) session(ctx context.Context, c config.Config) error {
	started := time.Now()
	phase := started
	mark := func(name string) {
		now := time.Now()
		slog.Info("connection timing", "phase", name, "duration", now.Sub(phase), "elapsed", now.Sub(started))
		phase = now
	}
	streamConfig := c
	s.state("connecting", "Connecting muted; verifying the configured receiver volume.")
	if c.DeviceID != "" {
		scanCtx, cancel := context.WithTimeout(ctx, 3*time.Second)
		device, err := s.Discovery.Find(scanCtx, c.DeviceID)
		cancel()
		if err == nil {
			c.Host, c.Port, c.DeviceName = device.Host, device.Port, device.Name
		}
	}
	if ctx.Err() != nil {
		return ctx.Err()
	}
	mark("discovery")
	client, err := cast.Dial(ctx, net.JoinHostPort(c.Host, strconv.Itoa(c.Port)))
	if err != nil {
		return err
	}
	defer client.Close()
	mark("connect")
	if err = client.SetVolume(ctx, c.ReceiverVolume, true); err != nil {
		return fmt.Errorf("verify muted receiver: %w", err)
	}
	mark("initial volume verification")
	app, err := client.Launch(ctx, c.Codec == "cast-opus")
	if err != nil {
		return err
	}
	mark("receiver launch")
	ownedSession := true
	defer func() {
		if !ownedSession {
			return
		}
		cleanup, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()
		if err := client.Stop(cleanup, app); err != nil {
			slog.Debug("stop owned session", "error", err)
		}
	}()
	if err = client.SetVolume(ctx, c.ReceiverVolume, true); err != nil {
		return fmt.Errorf("verify volume after launch: %w", err)
	}

	mark("post-launch volume verification")
	var stream *audio.Stream
	if c.Codec == "cast-opus" {
		session, negotiateErr := client.NegotiateAudio(ctx, app, c.Bitrate, c.TargetDelayMS)
		if negotiateErr != nil {
			return negotiateErr
		}
		mark("stream negotiation")
		stream, err = audio.NewRealtime(ctx, c, client.LocalIP(), client.RemoteIP(), session)
	} else {
		stream, err = audio.NewStream(ctx, c, client.LocalIP(), client.RemoteIP())
	}
	if err != nil {
		return err
	}
	defer stream.Close()
	if c.Codec != "cast-opus" {
		if err = stream.WaitReady(ctx); err != nil {
			return err
		}
		if err = client.Load(ctx, app, stream.URL, stream.ContentType); err != nil {
			return fmt.Errorf("load audio: %w", err)
		}
	}
	mark("audio pipeline")
	watchCtx, stopWatch := context.WithCancel(ctx)
	defer stopWatch()
	states, err := audio.WatchSink(watchCtx)
	if err != nil {
		return err
	}
	native, err := audio.ReadSink(ctx)
	if err != nil {
		return err
	}
	latest := s.Config()
	if !streamConfig.RestartRequired(latest) {
		c.ReceiverVolume = latest.ReceiverVolume
	}
	if err = client.SetVolume(ctx, c.ReceiverVolume, native.Muted); err != nil {
		return fmt.Errorf("verify playback volume: %w", err)
	}
	stream.Allowed.Store(!native.Muted)
	mark("playback volume verification")

	s.state("streaming", "Connected. Select ShoutOut in your desktop’s audio output menu.")
	ticker := time.NewTicker(time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case err := <-stream.Done:
			return fmt.Errorf("audio pipeline: %w", err)

		case <-s.liveChanged:
			next := s.Config()
			if streamConfig.RestartRequired(next) {
				continue
			}
			if next.ReceiverVolume != c.ReceiverVolume {
				// Keep the encoder and transport running while changing receiver gain.
				if err = client.SetLiveVolume(ctx, next.ReceiverVolume, native.Muted); err != nil {
					stream.Allowed.Store(false)
					return fmt.Errorf("apply receiver volume: %w", err)
				}
			}
			c = next
		case next, ok := <-states:
			if !ok {
				return errors.New("desktop audio connection closed")
			}
			if next.Muted != native.Muted {
				stream.Allowed.Store(false)
				if err = client.SetVolume(ctx, c.ReceiverVolume, next.Muted); err != nil {
					return err
				}
				stream.Allowed.Store(!next.Muted)
			}
			native = next
		case <-ticker.C:
			rs, err := client.Status(ctx)
			if err != nil {
				return err
			}
			owned := false
			for _, a := range rs.Applications {
				if a.SessionID == app.SessionID {
					owned = true
				}
			}
			if !owned {
				stream.Allowed.Store(false)
				ownedSession = false
				return errTakenOver
			}
			if rs.Volume.Level > config.MaxReceiverVolume || rs.Volume.Level > c.ReceiverVolume+0.00001 {
				stream.Allowed.Store(false)
				if err = client.SetVolume(ctx, c.ReceiverVolume, true); err != nil {
					return fmt.Errorf("receiver volume changed; stream muted: %w", err)
				}
				if err = audio.SetMuted(ctx, true); err != nil {
					return err
				}
				return errors.New("external receiver volume exceeded configured limit; reconnecting muted")
			}
			if rs.Volume.Muted != native.Muted {
				stream.Allowed.Store(false)
				if err = audio.SetMuted(ctx, rs.Volume.Muted); err != nil {
					return err
				}
				native.Muted = rs.Volume.Muted
				stream.Allowed.Store(!native.Muted)
			}
			ms := cast.MediaStatus{PlayerState: "CONNECTING"}
			stats := stream.RealtimeStats()
			if c.Codec == "cast-opus" {
				if stats.Feedback > 0 {
					ms.PlayerState = "TRANSMITTING"
				}
			} else {
				ms, err = client.Media(ctx, app)
				if err != nil {
					return err
				}
			}
			if ms.Media.ContentID != "" && ms.Media.ContentID != stream.URL {
				stream.Allowed.Store(false)
				ownedSession = false
				return errTakenOver
			}
			s.mu.Lock()
			s.status = Status{RequestedDelayMS: c.TargetDelayMS, ReceiverDelayMS: stats.ReceiverDelayMS, AudioFrames: stats.Frames, AcknowledgedFrame: stats.Acknowledged, Retransmits: stats.Retransmits, FeedbackReports: stats.Feedback, SinkMuted: native.Muted, SinkVolume: native.VolumePercent(), PlaybackSeconds: ms.CurrentTime, State: "streaming", Message: "Select ShoutOut as your audio output. Receiver latency has not been measured.", Sink: audio.SinkName, Device: c.DeviceName, PlayerState: ms.PlayerState, ReceiverVolume: rs.Volume.Level, ReceiverMuted: rs.Volume.Muted, EncodedBytes: stream.Bytes.Load(), MediaRequests: stream.Requests.Load(), Subscribers: stream.Subscribers(), Peak: math.Float64frombits(stream.Peak.Load()), Updated: time.Now()}
			s.mu.Unlock()
			if ms.PlayerState == "IDLE" {
				return fmt.Errorf("receiver stopped audio: %s", ms.IdleReason)
			}
		}
	}
}
