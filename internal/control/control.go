// Package control provides user-private local IPC for native settings and the CLI.
package control

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"net"
	"os"
	"path/filepath"
	"sync"
	"time"

	"github.com/lkarlslund/shoutout/internal/config"
	"github.com/lkarlslund/shoutout/internal/discovery"
	"github.com/lkarlslund/shoutout/internal/service"
)

type Request struct {
	Method string         `json:"method"`
	Config *config.Config `json:"config,omitempty"`
}
type Response struct {
	Devices []discovery.Device `json:"devices"`
	Config  config.Config      `json:"config"`
	Status  service.Status     `json:"status"`
	Error   string             `json:"error,omitempty"`
}

func Path() (string, error) {
	d := os.Getenv("XDG_RUNTIME_DIR")
	if d == "" {
		return "", errors.New("XDG_RUNTIME_DIR is required")
	}
	return filepath.Join(d, "shoutout.sock"), nil
}
func Listen() (net.Listener, error) {
	p, err := Path()
	if err != nil {
		return nil, err
	}
	if err = os.Remove(p); err != nil && !errors.Is(err, os.ErrNotExist) {
		return nil, err
	}
	l, err := net.Listen("unix", p)
	if err != nil {
		return nil, err
	}
	if err = os.Chmod(p, 0600); err != nil {
		l.Close()
		return nil, err
	}
	return l, nil
}
func Run(ctx context.Context, s *service.Service, l net.Listener) error {
	done := make(chan struct{})
	defer close(done)
	var wg sync.WaitGroup
	defer wg.Wait()
	go func() {
		select {
		case <-ctx.Done():
			l.Close()
		case <-done:
		}
	}()
	for {
		conn, err := l.Accept()
		if err != nil {
			if ctx.Err() != nil {
				return nil
			}
			return err
		}
		wg.Add(1)
		go func() {
			defer wg.Done()
			defer conn.Close()
			conn.SetDeadline(time.Now().Add(5 * time.Second))
			var req Request
			d := json.NewDecoder(io.LimitReader(conn, 16384))
			d.DisallowUnknownFields()
			err := d.Decode(&req)
			if err == nil {
				switch req.Method {
				case "watch-devices":
					watchDevices(ctx, s, conn)
					return
				case "status":
				case "configure":
					if req.Config == nil {
						err = errors.New("missing configuration")
					} else {
						err = s.Update(*req.Config)
					}
				default:
					err = errors.New("unknown method")
				}
			}
			result := Response{Config: s.Config(), Status: s.Status(), Devices: s.Discovery.Snapshot()}
			if err != nil {
				result.Error = err.Error()
			}
			json.NewEncoder(conn).Encode(result)
		}()
	}
}
func Call(req Request) (Response, error) {
	p, err := Path()
	if err != nil {
		return Response{}, err
	}
	conn, err := net.DialTimeout("unix", p, 2*time.Second)
	if err != nil {
		return Response{}, err
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(6 * time.Second))
	if err = json.NewEncoder(conn).Encode(req); err != nil {
		return Response{}, err
	}
	var r Response
	err = json.NewDecoder(io.LimitReader(conn, 65536)).Decode(&r)
	if err == nil && r.Error != "" {
		err = errors.New(r.Error)
	}
	return r, err
}

func watchDevices(ctx context.Context, s *service.Service, conn net.Conn) {
	conn.SetDeadline(time.Time{})
	updates, unsubscribe := s.Discovery.Subscribe()
	defer unsubscribe()
	closed := make(chan struct{})
	go func() { var b [1]byte; conn.Read(b[:]); close(closed) }()
	for {
		select {
		case <-ctx.Done():
			return
		case <-closed:
			return
		case devices := <-updates:
			conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if json.NewEncoder(conn).Encode(struct {
				Devices []discovery.Device `json:"devices"`
			}{devices}) != nil {
				return
			}
		}
	}
}
