# Installing m-mdy-m/log.c from a release asset

Choose the archive matching both your operating system and CPU architecture. Each platform archive contains the public header (`include/m-mdy-m/log.h`), static library, supported shared-library artifacts, `pkg-config` metadata where applicable, installation helper, examples, documentation, `LICENSE`, and the upstream `NOTICE`. Check `BUILD-INFO.txt` and verify the release `SHA256SUMS.txt` before using assets.

## Linux / FreeBSD / macOS

Extract the `.tar.gz` archive and run the user-local installer:

```sh
./install.sh
./install.sh --prefix "$HOME/.local"
```

For a system-wide installation, use a writable prefix such as `/usr/local` with the privileges required by your system.

```sh
export PKG_CONFIG_PATH="$HOME/.local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cc app.c $(pkg-config --cflags --libs m-mdy-m-log) -o app
```

The static archive is `lib/libm-mdy-m-log.a`. Shared-library files are provided only for the exact target OS/architecture of the archive. An archive built for macOS cannot be used on Linux or vice versa.

## Windows x86-64 (MinGW-w64 UCRT64)

Extract the `.zip` in PowerShell and run:

```powershell
.\install.ps1
.\install.ps1 -Prefix "$env:LOCALAPPDATA\m-mdy-m-log"
```

This is a MinGW-w64 UCRT64 package, not an MSVC package. Use the matching MinGW-w64 toolchain when linking it. If using the shared library, keep the DLL on the application `PATH` or beside the executable.

## Public include path

```c
#include <m-mdy-m/log.h>
```

Link against `m-mdy-m-log` (or use `pkg-config` on systems that provide it). Do not use libraries built for another OS, architecture, or C runtime.
