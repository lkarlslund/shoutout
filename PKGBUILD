# Maintainer: Lars Karlslund
pkgbase=shoutout-git
pkgname=('shoutout-git' 'shoutout-kde-git')
pkgver=0.1.0.r33.g7a4bcfa
pkgrel=1
pkgdesc='Use your Google Audio compatible speakers for system audio, with native desktop settings'
arch=('x86_64')
url='https://github.com/lkarlslund/shoutout'
license=('MIT')
install=shoutout.install
depends=('ffmpeg' 'libpulse' 'pipewire-pulse' 'qt6-base' 'libglvnd' 'systemd' 'glibc' 'libgcc' 'libstdc++')
makedepends=('git' 'go>=2:1.26' 'cmake' 'ninja' 'kcmutils' 'kcoreaddons')
provides=('shoutout')
conflicts=('shoutout')
source=('shoutout::git+https://github.com/lkarlslund/shoutout.git#branch=main')
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/shoutout"
  printf '0.1.0.r%s.g%s' "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

prepare() {
  cd "$srcdir/shoutout"
  GOTOOLCHAIN=local go mod download
}

build() {
  cd "$srcdir/shoutout"
  CGO_ENABLED=1 GOTOOLCHAIN=local go build -trimpath -buildmode=pie -mod=readonly \
    -ldflags "-linkmode=external -extldflags '${LDFLAGS}' -X main.version=${pkgver}" \
    -o build/shoutout ./cmd/shoutout
  cmake -S kde -B build/kde -G Ninja \
    -DCMAKE_BUILD_TYPE=None -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build/kde
}

check() {
  cd "$srcdir/shoutout"
  GOTOOLCHAIN=local go test -race ./...
  GOTOOLCHAIN=local go vet ./...
  ctest --test-dir build/kde --output-on-failure
}

package_shoutout-git() {
  optdepends=('shoutout-kde-git: KDE System Settings integration')
  cd "$srcdir/shoutout"
  install -Dm755 build/shoutout "$pkgdir/usr/bin/shoutout"
  DESTDIR="$pkgdir" cmake --install build/kde --component core
  install -Dm644 packaging/shoutout.service "$pkgdir/usr/lib/systemd/user/shoutout.service"
  install -d "$pkgdir/usr/lib/systemd/user/default.target.wants"
  ln -s ../shoutout.service "$pkgdir/usr/lib/systemd/user/default.target.wants/shoutout.service"
  install -Dm644 packaging/shoutout.desktop "$pkgdir/usr/share/applications/shoutout.desktop"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
  install -Dm644 licenses/*.txt -t "$pkgdir/usr/share/licenses/$pkgname/"
  install -Dm644 docs/*.md docs/*.png -t "$pkgdir/usr/share/doc/shoutout/"
}

package_shoutout-kde-git() {
  pkgdesc='ShoutOut integration for KDE System Settings'
  depends=("shoutout-git=$pkgver" 'kcmutils' 'kcoreaddons' 'qt6-base' 'systemsettings' 'libglvnd' 'glibc' 'libgcc' 'libstdc++')
  provides=('shoutout-kde')
  conflicts=('shoutout-kde')
  install=
  cd "$srcdir/shoutout"
  DESTDIR="$pkgdir" cmake --install build/kde --component kde
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
