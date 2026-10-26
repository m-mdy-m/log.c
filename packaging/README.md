# Packaging and installation

The repository is `m-mdy-m/log.c`; installable distribution assets are named `m-mdy-m-log` to avoid a generic `log` package collision.

## Local installation with Make

```sh
make
make clean check
sudo make install PREFIX=/usr/local
pkg-config --cflags --libs m-mdy-m-log
```

For package builders, stage rather than modify the host:

```sh
make install PREFIX=/usr DESTDIR="$PWD/pkgroot"
bash tests/e2e_install.sh "$PWD/pkgroot/usr"
```

The E2E consumer is built in a temporary directory against the staged installed header and static/shared libraries. This catches broken include paths, archives, shared library names, and runtime lookup errors.

## CMake

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build-cmake --config Release
ctest --test-dir build-cmake -C Release --output-on-failure
cmake --install build-cmake --prefix "$PWD/stage"
```

Use `-DBUILD_SHARED_LIBS=OFF` for a static build. The installed CMake package supports `find_package(m-mdy-m-log CONFIG REQUIRED)` and target `m-mdy-m::log`; public headers install to `include/m-mdy-m/log.h`.

## Local Linux x86-64 release bundle

On Linux x86-64:

```sh
make package-linux
# build/packages/m-mdy-m-log-v0.1.0-linux-x86_64.tar.gz
```

It contains the installed header, static and shared library, relocatable pkg-config metadata, installation helper, docs, examples and build metadata. It is a native Linux x86-64 artifact, not cross-platform.

## GitHub Release

The release matrix generates Linux x86-64/ARM64, macOS x86-64/ARM64, Windows x86-64 UCRT64, and FreeBSD/OpenBSD x86-64 SDK archives. The release job attaches these, source ZIP/TAR.GZ archives, and SHA-256 checksums. A separate Docker workflow verifies the SDK image build; the release flow publishes multi-architecture Docker Hub tags when the required repository secrets are configured.
