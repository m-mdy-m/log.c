# Installation and build

This document explains how to get the source, build and test it, install it for your own user or the whole system, and link another program against the result.

The core library is C11 and has no third-party runtime dependency. You need a C compiler and either GNU Make or CMake to build it. Python 3 is used by some repository checks and packaging tools. The optional Lua module requires Lua 5.4 development files.

## 1. Supported platforms

| Platform | Recommended toolchain | Notes |
|---|---|---|
| Linux x86-64 / ARM64 | GCC or Clang | Static and shared builds; SDK release packages are produced by CI. |
| macOS x86-64 / Apple silicon | Apple Clang | Static and shared builds; an archive must match the Mac CPU architecture. |
| Windows x86-64 | MinGW-w64 UCRT64 through MSYS2 | Released Windows libraries target this ABI. They are not MSVC libraries. |
| FreeBSD x86-64 | Clang or system C compiler | Native build/test and SDK asset are included in release automation. |
| OpenBSD x86-64 | Clang or system C compiler | Native build/test and SDK asset are included in release automation. |

These are the project's current CI and packaging targets, not a claim that every compiler on every operating system has been tested. The header is C11; other toolchains should be validated before shipping.

## 2. Clone the source

Install Git, then run:

```sh
git clone https://github.com/m-mdy-m/log.c.git
cd log.c
```

If you prefer SSH and have an SSH key configured with GitHub:

```sh
git clone git@github.com:m-mdy-m/log.c.git
cd log.c
```

The repository contains the public header, implementation, tests, examples, build files, optional Lua binding, and this documentation. You do not need to download another runtime package to start from source.

## 3. Build and test with Make

On Debian/Ubuntu, for example, install the compiler and Make:

```sh
sudo apt-get update
sudo apt-get install build-essential make pkg-config python3
```

Then run:

```sh
make                 # Build static and shared libraries
make examples        # Build the two C examples
make check           # Run tests, ABI/export checks, docs checks and installed-consumer E2E
```

The primary build gate is `make check`. It runs the unit and concurrent-writer tests, compiles the examples, verifies required shared-library ABI symbols, checks local documentation links, and builds/runs a separate consumer against a staged install. It does not install files into `/usr/local`.

To remove generated build files:

```sh
make clean
```

Important Make targets:

| Target | What it does |
|---|---|
| `all` | Builds static and shared libraries under `build/lib/`. |
| `static` | Builds only the static archive. |
| `shared` | Builds only the shared library. |
| `examples` | Builds C convenience-API and record-ABI examples. |
| `test` | Runs the C unit and concurrent-writer tests. |
| `docs-check` | Checks local Markdown links. |
| `abi-check` | Checks required exported symbols in the shared library. |
| `e2e` | Installs into an isolated local directory and compiles/runs a separate C consumer. |
| `check` | Runs all of the checks above. |
| `lua-check` | Optional Lua 5.4 module build and sample run; needs Lua development files. |
| `package` | Runs checks and creates source archives. |
| `package-linux` | Runs checks and creates a Linux x86-64 SDK archive. |

## 4. Build and test with CMake

CMake 3.16 or newer is required. From the repository root:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build-cmake --config Release --parallel
ctest --test-dir build-cmake -C Release --output-on-failure
```

To build a static library instead:

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build-static --config Release --parallel
ctest --test-dir build-static -C Release --output-on-failure
```

Install into a local directory for testing:

```sh
cmake --install build-cmake --prefix "$PWD/stage"
```

This installs into `stage/`; it does not require administrator privileges. You can turn off the tests or examples when configuring CMake. The option names come from `CMakeLists.txt`:

```sh
cmake -S . -B build-minimal \
  -DM_MDY_M_LOG_BUILD_TESTS=OFF \
  -DM_MDY_M_LOG_BUILD_EXAMPLES=OFF
```

## 5. Install with Make

### Install for your own user

A user-local prefix avoids changing system directories:

```sh
make install PREFIX="$HOME/.local"
```

Make sure the compiler can find the installed package metadata:

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
pkg-config --modversion m-mdy-m-log
pkg-config --cflags --libs m-mdy-m-log
```

### Install for the whole system

On a Unix-like machine, a system-wide installation typically needs administrator permission:

```sh
sudo make install PREFIX=/usr/local
```

The installed public header is `include/m-mdy-m/log.h`. The package installs a static library and a platform-specific shared library under `lib/`, plus `lib/pkgconfig/m-mdy-m-log.pc`.

### Uninstall

Use the same `PREFIX` you used during installation:

```sh
make uninstall PREFIX="$HOME/.local"
```

Review the target and prefix before running this command. It removes this project's known installed files from that prefix.

## 6. Install with CMake

After building with CMake:

```sh
cmake --install build-cmake --prefix "$HOME/.local"
```

The installed CMake package supports `find_package(m-mdy-m-log CONFIG REQUIRED)` and exports the target `m-mdy-m::log`. The `pkg-config` package name is `m-mdy-m-log`.

## 7. Link your own C program

### Option A: compile the implementation directly

This is the fastest way to try the API while working inside a source checkout:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude app.c src/log.c -o app
./app
```

### Option B: use `pkg-config` after installing

```sh
cc app.c $(pkg-config --cflags --libs m-mdy-m-log) -o app
./app
```

If you installed to `$HOME/.local`, set `PKG_CONFIG_PATH` first as shown above. Keep the `pkg-config` expansion after the source/object files so static linkers see the library in the right order.

### Option C: use CMake from your application

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app C)
find_package(m-mdy-m-log CONFIG REQUIRED)
add_executable(my_app app.c)
target_link_libraries(my_app PRIVATE m-mdy-m::log)
```

Configure your project with the install prefix visible to CMake, for example:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/.local"
```

## 8. Windows (MSYS2 UCRT64)

The published Windows SDK targets 64-bit MinGW-w64 UCRT64. Install [MSYS2](https://www.msys2.org/), open the **UCRT64** terminal, and install the toolchain:

```sh
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Then clone the repository and use the Make commands shown above. For a release ZIP, open PowerShell in the extracted archive and run:

```powershell
.\install.ps1 -Prefix "$env:LOCALAPPDATA\m-mdy-m-log"
```

Use the MinGW-w64 UCRT64 compiler when linking against that archive. **The archive is not built for MSVC**, and mixing a MinGW library with an MSVC-built application is not a supported configuration.

## 9. Install from a GitHub Release archive

When a version has been published, open the project's [Releases page](https://github.com/m-mdy-m/log.c/releases) and choose the archive matching both your OS and CPU architecture. Do not use a Linux library on macOS, or an x86-64 library on ARM64.

For Linux, macOS, FreeBSD, or OpenBSD, extract the `.tar.gz` and run:

```sh
./install.sh --prefix "$HOME/.local"
```

For Windows, extract the `.zip` and run `install.ps1` as shown above. Each SDK package includes the header, libraries for its target platform, metadata, examples, documentation and license/attribution files. Verify the archive against the release's `SHA256SUMS.txt` before using it.

## 10. Optional Lua 5.4 module

The core library does not depend on Lua. The adapter is optional and targets the Lua 5.4 C API. On Debian/Ubuntu, install Lua's development headers and `pkg-config`:

```sh
sudo apt-get install lua5.4 liblua5.4-dev pkg-config
make lua-check
```

Or run the sample with the build output explicitly available to the Lua loader:

```sh
make lua
LUA_CPATH='build/lua/?.so;;' LD_LIBRARY_PATH='build/lib' lua5.4 examples/lua/basic.lua
```

The Lua module is named `m_mdy_m_log`. Its API is documented in [Language bindings](language-bindings.md#lua-54).

## 11. Docker SDK image

The Docker image is a **build environment** for applications that use the logger. It is not a daemon and does not run a logging server. Once the project publishes the image to Docker Hub, use the published name from the repository's release instructions:

```sh
docker pull <dockerhub-username>/log.c:latest
docker run --rm -it -v "$PWD:/workspace" <dockerhub-username>/log.c:latest
```

The image is intended for `linux/amd64` and `linux/arm64`. Docker Hub publication requires the repository's `DOCKERHUB_USERNAME` and `DOCKERHUB_TOKEN` Actions secrets. See [Release guide](release.md#docker-hub-sdk-image).
