# Validation

Local platform: Arch Linux, KDE Plasma 6, PipeWire 1.6.9, Chromecast Audio named Lars Kontor.

## Confirmed

- User confirms ShoutOut appears as an output and produces audible audio. The continuous MP3 path had approximately 30 seconds of user-observed delay.
- AAC live segments are accepted by the receiver, with repeated playlist/segment requests and advancing PLAYING status. The user estimates roughly two seconds of audible delay with 500 ms segments, 128 kbps AAC and an 80 ms host buffer. This is a listening estimate, not an instrumented measurement.
- Native capture has `node.virtual=true` and no Pulse client association, allowing KDE to exclude it from the Applications list.
- An isolated sink test confirms native volume affects monitor PCM and native mute produces silence. Service restarts adopt the existing sink without resetting gain or routing.
- The native KCM loads, discovers receivers, displays status and exposes the full 0–100% receiver scale. KDE System Settings enumerates it when the per-user Qt plugin path is loaded.
- Settings use private Unix IPC; the former browser UI has been removed.
- Local Go tests, race checks and vet have passed; native compilation and offscreen UI loading have passed. Synthetic live-stream tests exercise actual encoding, manifest availability and receiver address restrictions without contacting speakers.
- Earlier local amd64/arm64 Go builds and vulnerability checks passed. Native modules require builds for the target Qt/KDE environment.

Agent-run hardware tests started muted at 1%, then used 3%, below the absolute 5% test maximum. This restriction is not a product volume cap.

## Cast Streaming / Opus

- Audio-only receiver application is available on Lars Kontor. Negotiation succeeds with explicit audio format fields and a signed-range SSRC for firmware compatibility.
- Encrypted stereo 48 kHz Opus frames receive advancing acknowledgements. The receiver reported the initial 400 ms buffer, then the user selected 60 ms and receiver feedback reported 60 ms. Neither figure is an acoustic measurement.
- The user confirms audible playback and a noticeable reduction in delay compared with HTTP audio.
- Agent-run hardware setup started muted at 1%, then used 3% receiver volume. The user subsequently adjusted settings; later validation was read-only and preserved those choices. The service remains controlled by native KDE gain/mute.
- Tests exercise AES-CTR against a known vector, packet fragmentation, frame-ID wraparound, RTCP bounds checking, stale feedback rejection, packet retransmission and sender clock mapping. A loopback synthetic test runs the actual encoder, decrypts received packets and sends simulated acknowledgements without contacting speakers.
- Native settings include transport selection and a 10–1000 ms target-delay control. Existing AAC presets remain available; selecting a different transport is explicit.
- Volume-only updates preserve the active session. Protocol tests verify unmuted volume changes send only a level update and status query, without mute/stop/launch commands.
- Native sliders cover receiver scale, segment duration and target delay, with synchronized numeric inputs.
- Attenuation and host-buffer fields have been removed from settings and configuration; legacy saved fields are ignored on load and disappear on save. Capture batching is managed internally.
- Go race tests, vet and native UI smoke checks pass. Two five-second fuzz runs completed roughly 940,000 RTCP parser cases and 497,000 Ogg parser cases without failures.

## Outstanding

Instrumented acoustic latency, long-duration drift, network recovery, suspend/resume, speaker groups, fresh distro installation and packaged distribution need further validation. One selected receiver/group is supported; multiple simultaneous destinations are not implemented. No automatic game/video synchronization is provided.

The per-user plugin becomes available to the ordinary System Settings launcher after a Plasma login. `shoutout configure` supplies its search path immediately. No privileged system plugin install is required by this method.

GitHub Actions previously refused to start jobs due to an account billing/spending-limit restriction. No hosted workflow steps ran; local checks are independent of that restriction.

## Reconnect setup timing

Selected-device discovery now returns once its complete mDNS records arrive,
while the settings device list still collects all replies. A read-only lookup
of Lars Kontor took 72 ms. Receiver setup reads the current volume and changes
only mute when the level already matches; changing the level still uses the
mute/set/reassert/verify sequence. Volume confirmation remains mandatory.

After deployment, service timing logs on Lars Kontor recorded 137 ms from
connection setup through enabling playback (discovery 4 ms, TLS 33 ms, launch
36 ms, negotiation 16 ms). This excludes stopping a previous stream and is
not a measurement of the audible interruption or end-to-end audio latency.
Race tests and vet passed, including receiver confirmation, minimal mute-only
commands, unchanged volume verification, and fragmented discovery records.

## Background discovery and live settings

The daemon now starts continuous discovery independently of playback enablement
and open settings windows. It probes every ten seconds and expires individual
records at their advertised TTL or 45 seconds, whichever is shorter. A private
socket subscription sends the initial cached list and subsequent changes to the
KDE module. Slow subscribers receive the latest snapshot without blocking the
worker. Reconnect lookup uses this shared cache. Socket reopening retains records
and refreshes network interface membership once per minute.

Race tests cover arrivals, departures, renewed expiry, short TTLs, goodbye
records, subscriber isolation, coalescing, cancellation, and control shutdown
with an open subscription. The native smoke check received five live devices
without a discovery button and verified the combined address:port field. The
installed service remained streaming with the existing user settings. Physical
device disconnection was not exercised; expiry uses synthetic DNS records in tests.

## Playback presets

The native smoke check activates each preset without saving or changing speaker
playback: Low latency selects Opus/128 kbps/20 ms, Balanced selects
Opus/192 kbps/100 ms, and High quality selects AAC/320 kbps/500 ms segments.
New configurations default to Balanced. A configuration migration test verifies
that old preset names become Custom without altering their playback parameters.
The normal encoding selector contains Opus and AAC; a legacy MP3 configuration
retains its entry when loaded. Go race tests and vet passed.


## Arch Linux package

`makepkg` successfully built `shoutout-git` on the development Arch host, using
the public Git source. The package includes the binary under `/usr/bin`, the
KDE module in the standard Qt plugin tree, a system-wide systemd user-unit,
a desktop entry, documentation and MIT/third-party notices. Package checks ran
Go race tests and vet. The packaged, stripped KDE module passed the native
smoke check and the packaged executable reported its VCS version. The desktop
entry passed `desktop-file-validate` and the package file list was inspected.

This was not a clean-chroot build or a system-wide installation test.

The Arch package now ships an automatic user-session startup symlink and an
install hook that starts the service in logged-in users' managers. Mock-command
checks covered install, upgrade, removal and skipping root. A rebuilt package
passed its Go checks and contains the expected symlink and `.INSTALL` script.
Actual privileged pacman installation remains untested on this host.


## Host-side Cast Streaming latency reduction

A synthetic real-time input fed stereo float32 PCM at 48 kHz in 10 ms writes to
FFmpeg, with no audio device or speaker involved. Across three one-second runs,
median audio-page output lag relative to the end of the corresponding frame was
70.5–70.6 ms with raw PCM input and 20.2–20.4 ms with streaming WAV input and
3840-byte demux packets. Measurements used the steady middle portion of each
stream (0.2–0.9 seconds); they exclude capture, networking and receiver playback.

The Cast Streaming path now requests 10 ms capture batches and bounds encoder
input packets to 10 ms. It retains 20 ms Opus frames and the configured receiver
buffer. The WAV length is ignored so the stream can exceed RIFF's size limit.
A regression test supplies only 40 ms of PCM while holding the input open and
requires an encrypted 20 ms Opus packet; it fails on the previous pipeline and
passes on the updated one. Full Go race tests and vet pass.

These changes have not yet been acoustically measured or hardware-tested on a
receiver. The installed service and the user's speaker settings were unchanged.


## Shorter Opus frames

Frame duration now follows the receiver target: 5 ms below 40 ms, 10 ms
below 80 ms, and 20 ms otherwise. Capture requests and WAV input packets are
5 ms; the PCM gate also processes 5 ms batches while preserving fade duration.
RTP timestamps, pacing and the acknowledgement window use the selected frame
duration. KDE permits custom targets from 10 ms, in 10 ms steps. The Low latency preset uses a 20 ms target; Balanced remains at 100 ms.

Silent receiver tests accepted 5 and 10 ms frames. A 20-second synthetic capture
through the real encoder and UDP transport to Køkkenet, muted at verified 1%,
sent 3,995 frames at a 20 ms target using 5 ms frames. Receiver events reported
zero late frames and transport reported zero retransmissions. Median receiver
arrival-to-playout-event time was 14 ms. This excludes host capture/encoding and
network time; it is not an acoustic measurement or a long-duration stability
claim. Desktop capture scheduling under load still needs real-world evaluation.

Automated tests exercise encrypted 5, 10 and 20 ms frames and matching RTP
timestamps. A regression test supplies only 15 ms of PCM and requires output
without closing the input. Race tests, vet and native KDE build pass.


## Standalone settings and optional desktop integration

`shoutout configure` now starts the companion `shoutout-settings` executable.
The standalone Qt window and optional KDE System Settings module embed the same
panel, with live discovery, presets, volume scaling, status and validation.
Apply, Reset, Restore Defaults and unsaved-change confirmation are available in
the standalone window. Qt-only builds do not require or link KDE frameworks.
The Arch core package includes both executables; a separate KDE package installs
the System Settings module. The package installer selects the KDE integration
when the calling desktop identifies itself as KDE.

Isolated Qt tests cover loading, preset selection, failed and successful Apply,
address validation, defaults and reset with a fake backend. Native smoke checks
exercise both hosts with the running service without applying changes. The
Qt-only build and Go race/vet checks pass. GNOME and Omarchy sessions are not yet
hardware-tested. No speaker playback changes are required to test this UI.
