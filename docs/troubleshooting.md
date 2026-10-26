# Troubleshooting

This page lists common problems and the shortest way to identify them. If a problem remains, include the operating system, compiler version, exact command, full error, and whether you built a static or shared library in your bug report.

## `m-mdy-m/log.h` cannot be found

**Cause:** The compiler's include search path does not point to the repository's `include/` directory or the installed `include/` directory.

When building inside the checkout, use:

```sh
cc -Iinclude app.c src/log.c -o app
```

After installation, use the package's include flags:

```sh
cc app.c $(pkg-config --cflags --libs m-mdy-m-log) -o app
```

For a user-local installation, ensure `PKG_CONFIG_PATH` includes `$HOME/.local/lib/pkgconfig`.

## `undefined reference to mmlog_...` or linker error

**Cause:** The program includes the header but does not link the implementation, the library path is missing, or the link order is wrong.

Quick checkout build:

```sh
cc -std=c11 -Iinclude app.c src/log.c -o app
```

Installed library:

```sh
cc app.c $(pkg-config --cflags --libs m-mdy-m-log) -o app
```

Keep the `pkg-config` output after the source/object files. Verify `pkg-config --modversion m-mdy-m-log` reports the expected version.

## The program starts but cannot load a shared library

**Cause:** The operating system's dynamic loader cannot find the shared library or finds one from another version.

- Make sure the library is installed in a loader-visible path, or set the appropriate runtime library path for local testing.
- Check that the library matches the current OS and architecture.
- Check `mmlog_abi_version()` when using a prebuilt library from an FFI binding.
- Prefer a static build when your deployment model requires no runtime shared-library lookup.

Do not copy a `.so` from Linux to macOS or use an ARM64 library in an x86-64 process.

## `mmlog_logger_create` returns an error

Check the status with `mmlog_status_name`. Typical reasons include invalid config values or a failed memory allocation. Call `mmlog_logger_config_init` before changing selected config fields. The `max_message_bytes` value must be between 1 and `MMLLOG_MAX_MESSAGE_BYTES`.

## Logs appear at `WARN` and above but not `INFO`/`DEBUG`

The logger-wide minimum level filters records before they reach destinations. Check the config's `min_level` or call:

```c
mmlog_logger_set_min_level(logger, MMLLOG_LEVEL_TRACE);
```

Also check the minimum level passed to `mmlog_logger_add_file` or `mmlog_logger_add_callback`; each extra destination has its own filter.

## Quiet mode seems to ignore my file logs

`mmlog_logger_set_quiet(logger, 1)` hides only the default console output. It should not silence file or callback sinks. Ensure the extra sink was added successfully and its minimum level allows the record.

## Colours do not appear

This is expected in `MMLLOG_COLOR_AUTO` when output is redirected or is not a terminal. A non-empty `NO_COLOR` environment variable or `TERM=dumb` also disables colours in AUTO mode. Use `MMLLOG_COLOR_ALWAYS` only when you specifically want escape codes in that console stream. Files and callbacks are always plain text.

## A newline in the message appears as `\n`

That is intentional. The formatter escapes newlines and terminal control characters to keep one record on one output line. The callback receives the same plain-text, escaped line as the file output.

## Python or another FFI crashes or prints garbage

Check the ABI declarations in this order:

1. Did you call `mmlog_abi_version` and check the major version?
2. Does each struct exactly match the C header, including field order and alignment?
3. Did you set each FFI function's argument and return types?
4. Did you use native `size_t` (`ctypes.c_size_t` in Python, `usize` in Rust)?
5. Did `mmlog_event_init` set the event's `struct_size`?
6. Are all strings and message bytes alive until `mmlog_logger_write` returns?
7. Did you call `mmlog_logger_destroy` only after all uses of the logger had stopped?

Do not call `mmlog_logf` through FFI; use `mmlog_logger_write`.

## A callback sometimes runs at the same time as another callback

This is allowed. Callbacks run outside the logger's internal lock, so a callback can re-enter the logger without deadlocking. Multiple threads writing the logger may invoke the same callback concurrently, and callback completion order is not guaranteed. Protect shared callback state with your own synchronization.

## `MMLLOG_EFULL` when adding a destination

The current implementation permits at most 16 combined file and callback destinations per logger. Check whether the application accidentally adds a new destination each time it handles a request. Add reusable destinations during setup instead.

## Windows library will not link with MSVC

The published Windows archive targets MinGW-w64 UCRT64. It is not an MSVC-built library. Use the matching UCRT64 compiler/runtime or build and validate the project with the toolchain you intend to support.

## Lua says the module cannot be found

Make sure you built it with Lua 5.4 development files, and that the Lua module path includes `build/lua/?.so` (or the platform-specific module extension) and the shared logger can be found. From the repository root, use the project's sample command:

```sh
LUA_CPATH='build/lua/?.so;;' LD_LIBRARY_PATH='build/lib' lua5.4 examples/lua/basic.lua
```

The core library works without Lua; only the optional adapter needs Lua 5.4.
