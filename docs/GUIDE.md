# Installation and usage

## Install

For Arch Linux, the [PKGBUILD](ARCH.md) installs system-wide, with optional KDE integration. The instructions below describe the alternative per-user installation.

Requires a systemd user session, PipeWire's PulseAudio compatibility service, `pactl`, `parec`, and FFmpeg with AAC, MP3 and `libopus` encoders. Building also requires Go (see `go.mod`), CMake, Ninja, a C++20 compiler, Qt6 Widgets and Network development files. KDE Frameworks 6 KCMUtils and CoreAddons are optional for the System Settings module. Arch is the current development platform; other distributions need validation.

```sh
make build settings
./build/shoutout doctor
./build/shoutout setup --device "Your speaker name"
./build/shoutout install
shoutout configure
```

The per-user installer copies both executables and a desktop launcher, then enables the systemd user service. When the KDE module was built, it also installs the module and a Plasma environment script. It preserves existing output volume/mute; a newly created output starts muted with a 1% receiver scale.

`shoutout configure` and the ShoutOut application launcher always open the standalone Qt window. KDE can additionally embed the same panel in System Settings. Per-user KDE modules are discovered after the next Plasma login; system packages are discovered after reopening System Settings.

CMake builds the KDE module when its development libraries are available. To explicitly build only standalone settings, use `make build settings CMAKE_FLAGS=-DSHOUTOUT_KDE=OFF`. GNOME and Omarchy use the standalone window and their normal audio output controls; these desktop sessions have not yet been tested. Qt may use different styling from GTK applications. Go-only CI artifacts do not include the required settings executable.

## Use

Choose a detected receiver or enter `address:port` (for example `192.168.1.10:8009`). The service discovers receivers continuously from startup and pushes live changes to the settings dropdown. Devices disappear after 45 seconds without refreshed discovery records, or earlier when they announce departure. Your selected destination is retained if it becomes unavailable. One destination is supported at a time; advertised speaker groups can be selected but have not been validated. Select ShoutOut in your desktop's audio selector, unmute it and route your applications normally.

- Your desktop's normal output slider and mute are authoritative. Internal capture is marked virtual so it does not appear as an application in KDE's mixer.
- Receiver volume scale is configurable from **0–100%**. Keep it low for sensitive speakers. The 5% maximum applies only to development speaker tests.
- Native volume changes apply to captured audio, so their audible effect includes stream delay. Native mute also sends a receiver mute command without restarting playback.
- Encoding choices are **Cast Streaming / Opus (experimental)** and AAC live segments. Existing MP3 configurations remain supported as a legacy option. Cast Streaming sends encrypted, paced Opus frames (5 ms below a 40 ms target, 10 ms below 80 ms, otherwise 20 ms) over UDP, with receiver feedback and bounded retransmission. Its target-delay control accepts 10–1000 ms; the Balanced target is 100 ms. This is a requested receiver buffer, not measured end-to-end latency. Select AAC manually if the receiver does not support this mode.
- For AAC, live segment duration is configurable from 250–2000 ms. The live playlist holds six segments; receiver buffering is additional. MP3 can have very high receiver delay.
- **Low latency** selects Opus at 128 kbps with a 20 ms receiver target. **Balanced** (the new-install default) selects Opus at 192 kbps with a 100 ms target. **High quality** selects AAC at 320 kbps with 500 ms segments. These durations are protocol settings, not measured audible latency. Editing encoding, bitrate or buffering selects Custom. Existing configurations retain their transport settings.
- Volume scale applies to the running session without reconnecting. Volume changes during unmuted playback avoid a mute cycle. Numeric controls accompany sliders for scale, segment length and target delay.
- Capture batching and transport queues are managed internally. There is no separate host-buffer or attenuation setting.
- Applying destination, encoding, segment-length or target-delay changes restarts the stream. The sink and desktop routing remain in place. Another controller taking over the receiver stops automatic reconnection; apply settings to reclaim it deliberately.

Settings use a private Unix socket in `$XDG_RUNTIME_DIR`, with no browser interface. Cast Streaming uses a negotiated UDP endpoint on the receiver, with return feedback to the sender’s ephemeral port. For AAC/MP3, the media server listens on the receiver-facing interface at TCP **17833**, with a random session URL and receiver-address restriction. Discovery uses IPv4 mDNS on UDP **5353**. Allow those network paths when needed; the installer does not change your firewall.

## Manage and develop

```sh
shoutout configure
shoutout status
shoutout devices
journalctl --user -u shoutout
systemctl --user restart shoutout
shoutout uninstall
make test
```

Configuration is stored in `$XDG_CONFIG_HOME/shoutout/config.json`, normally `~/.config/shoutout/config.json`. `setup` is intended before starting the service; use native settings or `shoutout config` / `shoutout apply` while running. Uninstall retains personal settings and removes the owned audio sink and installed files. Restart Plasma after uninstall to clear its inherited plugin search path.

Tests use synthetic PCM and protocol simulations without emitting audio. Agent-run speaker tests must begin muted at 1%, verify receiver status, and never exceed 5%. See [validation](VALIDATION.md) and the [plan](PLAN.md).

