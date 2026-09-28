# ShoutOut

**Use your Google Audio compatible speakers for system audio**

ShoutOut turns a Google Cast speaker into a Linux audio output. Select it in your desktop’s audio menu and send sound from your games, browser, music player—or your whole desktop.

![ShoutOut settings in KDE](shoutout.png)

- **Feels native.** A native settings window, optional KDE System Settings integration, and your desktop’s volume and mute controls.
- **Speakers appear automatically.** Live discovery keeps your destination list up to date.
- **Choose your balance.** Low latency, Balanced and High quality presets, plus custom controls.
- **Tame sensitive speakers.** Adjustable volume scaling, applied without interrupting playback.

| Preset | Playback |
|---|---|
| Low latency | Opus · 128 kbps · 20 ms receiver target |
| Balanced | Opus · 192 kbps · 100 ms receiver target |
| High quality | AAC · 320 kbps |

Receiver targets are buffer settings, not total audible latency. Playback is tested on Chromecast Audio; Cast Streaming is experimental.

## Get started

For **Linux + PipeWire**, with Qt6 settings and optional KDE Plasma 6 integration. KDE is tested; GNOME and Omarchy sessions still need validation. Build from source with Go, FFmpeg and Qt6 development libraries; see [requirements and installation details](GUIDE.md).

On Arch Linux, use the [system-wide package](ARCH.md):

```sh
make package-install
shoutout configure
```

Or install from source for your user:

```sh
make build settings
./build/shoutout install
shoutout configure
```

Choose your speaker, select **ShoutOut** as your audio output, and unmute. Start with a low volume scale for sensitive speakers.

`shoutout configure` always opens the standalone window. KDE users can also use the optional System Settings page.

[Usage & troubleshooting](GUIDE.md) · [Development plan](PLAN.md) · [Validation](VALIDATION.md)

[MIT licensed](../LICENSE). [Third-party notices](DEPENDENCIES.md).
