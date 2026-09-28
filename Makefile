.PHONY: build settings kde test install uninstall package package-install clean

CMAKE_FLAGS ?=

VERSION ?= $(shell git describe --tags --always --dirty)

build:
	CGO_ENABLED=0 go build -trimpath -ldflags "-s -w -X main.version=$(VERSION)" -o build/shoutout ./cmd/shoutout

settings:
	cmake -S kde -B build/kde -G Ninja $(CMAKE_FLAGS)
	cmake --build build/kde

kde: settings

test:
	go test -race ./...
	go vet ./...

install: build settings
	./build/shoutout install

uninstall:
	shoutout uninstall

MAKEPKG_FLAGS ?= -s

package:
	mkdir -p build/arch/sources
	BUILDDIR="$(CURDIR)/build/arch" SRCDEST="$(CURDIR)/build/arch/sources" PKGDEST="$(CURDIR)/build" SRCPKGDEST="$(CURDIR)/build" makepkg $(MAKEPKG_FLAGS)

package-install: package
	PKGDEST="$(CURDIR)/build" bash packaging/install-package.sh

clean:
	rm -rf build
