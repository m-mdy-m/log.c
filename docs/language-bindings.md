# Language bindings

`m-mdy-m/log.c` is implemented in C, but a language does not need to be written in C to use it. If the runtime can load a shared C library and call functions using the platform's C ABI, it can use the stable record interface.

There are two different styles in the public header:

- **C convenience API:** macros such as `MMLLOG_INFO(logger, "x=%d", x)`. These eventually call the C variadic function `mmlog_logf`.
- **Stable record API:** create an `mmlog_event_t`, then call `mmlog_logger_write(logger, &event)`. This function has no variadic arguments and is the preferred interface for Python, Rust, Go, Lua C modules, C#, Zig, and other FFI environments.

Do not call `mmlog_logf` through an FFI. C variadic calling conventions are not portable across language runtimes and platforms.

## 1. Rules every binding must follow

| Rule | Why it matters |
|---|---|
| Read the exact types from `include/m-mdy-m/log.h`. | A C struct's field order, alignment, and integer width are part of the ABI. |
| Call `mmlog_abi_version` before using a prebuilt shared library. | The loaded library may not be the version expected by the binding. |
| Use `mmlog_logger_config_init` and `mmlog_event_init`. | These functions initialize defaults and the required `struct_size`. |
| Check every status return. | Memory or I/O failures must not be silently treated as success. |
| Use `int32_t`-width signed integers for level/status and `uint64_t` for times. | The C header explicitly fixes these types. |
| Map C `size_t` to the target's native size type. | For Python `ctypes`, use `ctypes.c_size_t`; in Rust, `usize`; in Go cgo, `C.size_t`. |
| Keep all strings and message buffers alive through `mmlog_logger_write`. | The library borrows these pointers synchronously and does not retain them. |
| Destroy the logger only after no threads can use it. | A stale handle is not safe to call. |
| Keep callback code and `user_data` alive as long as it can be called. | The logger stores those pointers, not a copy of the callback. |

## 2. Where the library files live

After installing the SDK, the important files are:

| File | Purpose |
|---|---|
| `include/m-mdy-m/log.h` | Public structs, constants, macros, and function declarations. |
| `lib/libm-mdy-m-log.a` | Static library on Unix-like targets and MinGW packages. |
| `lib/libm-mdy-m-log.so*` | Linux/BSD shared library files, depending on target and package. |
| `lib/libm-mdy-m-log*.dylib` | macOS shared library files. |
| `bin/m-mdy-m-log.dll` or the package's documented DLL name | Windows shared library. |

Use the library built for your own operating system, CPU architecture, and runtime ABI. The published Windows package targets MinGW-w64 UCRT64 and is not intended for MSVC.

## 3. Stable record call in C

This example deliberately avoids the variadic helper. It is the same style an FFI should reproduce:

```c
#include <m-mdy-m/log.h>
#include <string.h>

int main(void) {
    mmlog_logger_t *logger = NULL;
    mmlog_event_t event;
    mmlog_status_t rc;

    rc = mmlog_logger_create(NULL, &logger); /* NULL config = defaults */
    if (rc != MMLLOG_OK) return 1;

    rc = mmlog_event_init(&event);
    if (rc == MMLLOG_OK) {
        event.level = MMLLOG_LEVEL_INFO;
        event.layer = "storage";
        event.component = "sqlite";
        event.status = "committed";
        event.message = "transaction committed";
        event.message_length = strlen(event.message);
        rc = mmlog_logger_write(logger, &event);
    }

    mmlog_logger_destroy(logger);
    return rc == MMLLOG_OK ? 0 : 2;
}
```

Build it in the checkout with:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude \
  examples/c/abi_record.c src/log.c -o abi_record
./abi_record
```

## 4. Python (`ctypes`) example

This example calls the non-variadic ABI directly. First, install the SDK or build a shared library. Then set `M_MDY_M_LOG_LIBRARY` to its exact library path and run the Python code. On Windows, use the path to the SDK's DLL. The script uses `ctypes.CDLL`, which corresponds to this library's C calling convention.

```python
import ctypes
import os

lib = ctypes.CDLL(os.environ["M_MDY_M_LOG_LIBRARY"])

class Config(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("min_level", ctypes.c_uint32),
        ("color_mode", ctypes.c_uint32),
        ("output", ctypes.c_uint32),
        ("format_flags", ctypes.c_uint32),
        ("max_message_bytes", ctypes.c_uint32),
    ]

class Event(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("level", ctypes.c_int32),
        ("timestamp_unix_ns", ctypes.c_uint64),
        ("duration_ns", ctypes.c_uint64),
        ("line", ctypes.c_uint32),
        ("reserved", ctypes.c_uint32),
        ("layer", ctypes.c_char_p),
        ("component", ctypes.c_char_p),
        ("file", ctypes.c_char_p),
        ("function", ctypes.c_char_p),
        ("request_id", ctypes.c_char_p),
        ("trace_id", ctypes.c_char_p),
        ("status", ctypes.c_char_p),
        ("message", ctypes.c_char_p),
        ("message_length", ctypes.c_size_t),
    ]

lib.mmlog_abi_version.argtypes = []
lib.mmlog_abi_version.restype = ctypes.c_uint32
lib.mmlog_status_name.argtypes = [ctypes.c_int32]
lib.mmlog_status_name.restype = ctypes.c_char_p
lib.mmlog_logger_config_init.argtypes = [ctypes.POINTER(Config)]
lib.mmlog_logger_config_init.restype = ctypes.c_int32
lib.mmlog_logger_create.argtypes = [ctypes.POINTER(Config), ctypes.POINTER(ctypes.c_void_p)]
lib.mmlog_logger_create.restype = ctypes.c_int32
lib.mmlog_event_init.argtypes = [ctypes.POINTER(Event)]
lib.mmlog_event_init.restype = ctypes.c_int32
lib.mmlog_logger_write.argtypes = [ctypes.c_void_p, ctypes.POINTER(Event)]
lib.mmlog_logger_write.restype = ctypes.c_int32
lib.mmlog_logger_destroy.argtypes = [ctypes.c_void_p]
lib.mmlog_logger_destroy.restype = None

def check(status):
    if status != 0:
        name = lib.mmlog_status_name(status).decode("utf-8", "replace")
        raise RuntimeError(f"m-mdy-m/log.c: {name} ({status})")

abi = lib.mmlog_abi_version()
if (abi >> 16) != 1:
    raise RuntimeError(f"Unsupported logger ABI: 0x{abi:08x}")

config = Config()
check(lib.mmlog_logger_config_init(ctypes.byref(config)))
logger = ctypes.c_void_p()
check(lib.mmlog_logger_create(ctypes.byref(config), ctypes.byref(logger)))

# Keep these Python byte strings referenced until mmlog_logger_write returns.
layer = b"python"
component = b"ctypes"
message = b"hello from Python"
event = Event()
try:
    check(lib.mmlog_event_init(ctypes.byref(event)))
    event.level = 2  # MMLLOG_LEVEL_INFO
    event.layer = layer
    event.component = component
    event.message = message
    event.message_length = len(message)  # bytes, not characters
    check(lib.mmlog_logger_write(logger, ctypes.byref(event)))
finally:
    lib.mmlog_logger_destroy(logger)
```

Important: this illustrates the ABI layout for the current ABI 1.0. Production bindings should derive their definitions from the public header and test struct sizes/layouts for every platform they support. Never guess a struct's size, and do not pass a Python string object where the C API expects a pointer to encoded bytes.

## 5. Rust, Go, and other FFI runtimes

Use the same C record interface and mirror the header exactly:

- Rust: `#[repr(C)]` structs; `i32` for `int32_t`, `u32` for `uint32_t`, `u64` for `uint64_t`, `usize` for `size_t`, and `*const c_char` for C string pointers. Wrap the opaque logger pointer in a type whose `Drop` calls `mmlog_logger_destroy`. Keep the backing byte strings alive for the call.
- Go: use cgo declarations from the real header where possible. Convert Go strings to C-owned bytes for the duration of the synchronous call, and release temporary memory after it returns. Do not store a Go pointer in `user_data` unless the cgo pointer rules permit that lifetime and use.
- C#: use `DllImport`/P/Invoke with the correct C calling convention, fixed-width integer types, pointer-safe struct declarations, and `UIntPtr`/native-size mapping for `size_t`. Test the struct layout on each target.
- Zig: use `@cImport` on `m-mdy-m/log.h` when the header is available; this avoids manually duplicating C declarations.
- C++: the public header wraps declarations in `extern "C"`; C++ can use the C ABI, but the library does not promise a separate C++ object ABI.

Do not expose the internal `mmlog_logger_t` layout in any binding. It is deliberately opaque.

## 6. Lua 5.4

The repository contains an optional Lua 5.4 module named `m_mdy_m_log`. It is a convenience adapter around the same record ABI. The core library does not require Lua.

Build and run the sample on a machine with Lua 5.4 development files and `pkg-config`:

```sh
make lua-check
```

Or:

```sh
make lua
LUA_CPATH='build/lua/?.so;;' LD_LIBRARY_PATH='build/lib' lua5.4 examples/lua/basic.lua
```

Example:

```lua
local log = require("m_mdy_m_log")
local logger = log.new({ level = "trace", color = "auto", output = "stderr" })

logger:info("service started", { layer = "api", component = "bootstrap" })
logger:warn("cache is warming", { component = "cache", duration_ns = 1250000 })
logger:add_file("service.log", "info")
logger:error("request failed", { request_id = "req-123", status = "timeout" })
logger:close()
```

Available methods are `trace`, `debug`, `info`, `warn`, `error`, `fatal`, `log`, `set_level`, `set_color`, `set_quiet`, `add_file`, and `close`. `log.level_name(level)` returns the name of a numeric level. Level names accepted by Lua are lowercase strings (`"info"`, `"error"`, etc.) or numbers from 0 through 5. Context keys correspond to the event fields: `layer`, `component`, `file`, `function`, `request_id`, `trace_id`, `status`, `duration_ns`, `timestamp_unix_ns`, and `line`.

## 7. Callbacks across the ABI

The callback type is:

```c
typedef void (*mmlog_sink_fn)(void *user_data, int32_t level,
                              const char *line, size_t line_length);
```

Register a callback only when you can keep both the function and its `user_data` valid. The line has no trailing newline and is plain text. Copy the bytes if you want to keep the line after returning. Callback execution is synchronous but occurs outside the logger's internal lock, so the callback can re-enter the logger. Multiple writers may invoke the callback concurrently; a binding should not assume callbacks are serialized.

## 8. ABI binding checklist

- [ ] Query and check the ABI major version.
- [ ] Mirror the exact C struct layout, field types, alignment and `size_t` width.
- [ ] Initialize every input struct through the matching initializer.
- [ ] Set argument and return types explicitly in dynamic FFI declarations.
- [ ] Check every status result.
- [ ] Keep all pointers valid until the synchronous call returns.
- [ ] Never call C varargs through FFI.
- [ ] Stop all workers before destroying a logger.
- [ ] Run binding tests on each operating system and CPU architecture you ship.
