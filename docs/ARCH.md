# Arch Linux package

Build and install the latest Git version system-wide:

```sh
sudo pacman -S --needed base-devel git
git clone https://github.com/lkarlslund/shoutout.git
cd shoutout
make package-install
shoutout configure
```

The root [PKGBUILD](../PKGBUILD) builds two packages. `shoutout-git` installs:

- `/usr/bin/shoutout` and `/usr/bin/shoutout-settings`
- `/usr/lib/systemd/user/shoutout.service`
- A desktop launcher, documentation and license notices under `/usr/share/`

Installation enables ShoutOut for user sessions and starts it immediately for
logged-in users. Future sessions start it automatically. Audio runs as each
user, with their own settings and PipeWire session. Open ShoutOut from your application launcher and choose your speaker; no service setup or reboot is needed.

The optional `shoutout-kde-git` package adds the System Settings module.
`make package-install` includes it when `XDG_CURRENT_DESKTOP` contains `KDE`;
other desktops receive only the core package. To add it manually, install the
matching `build/shoutout-kde-git-<version>-x86_64.pkg.tar.zst` package with
`sudo pacman -U`. Reopen System Settings after installation.

When upgrading a KDE installation manually, install matching core and KDE
packages together to retain the System Settings entry.

Choose a destination and unmute ShoutOut in your desktop's output selector. Do not run
`shoutout install` for a package installation; that command installs a separate
copy in your home directory.

## Build only or remove

`make package` builds the `.pkg.tar.zst` in `build/` without installing it.
All package working files also stay under `build/`. The root `PKGBUILD`
can still be used directly with `makepkg`. `check()` runs Go
race tests, vet and isolated Qt settings tests; these use simulated receivers and do not play speaker audio.
This recipe tracks Git `main`; it has not been submitted to the AUR.

Remove it with `sudo pacman -Rns shoutout-git` (include `shoutout-kde-git` if installed); the package stops its running
user services and removes automatic startup. Personal settings remain. An existing virtual
output disappears when the audio session ends.


## Dependencies

Runtime dependencies cover FFmpeg encoding, `pactl`/`parec` from `libpulse`,
PipeWire's PulseAudio server, Qt6, systemd, and the directly linked C/C++ runtimes.
Only the optional KDE package requires KDE libraries and System Settings.
PipeWire pulls in its session-manager dependency. Discovery runs inside
ShoutOut and does not require a separate discovery service.

Build dependencies are Git, Go 1.26 or newer, CMake, Ninja and KDE development libraries (the split recipe builds both packages), on top of Arch's
standard `base-devel` tools. Arch's Go package uses epoch `2`, so the version
constraint is `go>=2:1.26`. FFmpeg supplies AAC and Opus encoding; separate
encoder development packages are not needed.
