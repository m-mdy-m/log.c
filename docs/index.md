# m-mdy-m/log.c documentation

`m-mdy-m/log.c` is a small C11 library that lets an application write useful, consistently formatted log messages. It can write to the terminal, append to a file, or call code supplied by your application. It also has a stable, non-variadic C interface that can be called from languages such as Python, Rust, Go, and Lua.

The library is **not** a separate program, server, or logging daemon. You build it into your application or load its shared library. Your application decides what an event means and when to log it; the library formats and delivers the event.

The public header is [`include/m-mdy-m/log.h`](../include/m-mdy-m/log.h). It defines C ABI **1.0**. When this document and the public header disagree, the header is the final authority; please report the mismatch.

## Which document should I read?

| If you want to… | Read… |
|---|---|
| Understand what the library does and start writing logs | [User guide](user-guide.md) |
| Clone, build, test, install, or link the library | [Installation and build](installation.md) |
| Call it from Python or another language through the C ABI | [Language bindings](language-bindings.md) |
| Look up public structs, constants, status codes, and ABI rules | [C ABI reference](abi.md) |
| Understand how a log record moves through the library | [Architecture](architecture.md) |
| Change the code or add tests | [Developer guide](development.md) |
| Package a release, verify it, or publish GitHub/Docker assets | [Release guide](release.md) |
| Fix a common build, link, or output problem | [Troubleshooting](troubleshooting.md) |

New users should read this page, follow the first example in the [user guide](user-guide.md), then use the [installation guide](installation.md) for their platform. Anyone writing an FFI binding should read the [ABI reference](abi.md) before writing bindings code.

## The basic idea

A typical application does four things:

1. Creates a logger (a small object that holds settings and output destinations).
2. Writes a message with a severity such as `INFO` or `ERROR`.
3. Optionally adds context, such as the component, request ID, or elapsed time.
4. Destroys the logger before the application or the subsystem that owns it shuts down.

```c
#include <m-mdy-m/log.h>

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;

    if (mmlog_logger_config_init(&config) != MMLLOG_OK) return 1;
    if (mmlog_logger_create(&config, &logger) != MMLLOG_OK) return 1;

    MMLLOG_INFO(logger, "service started");

    mmlog_logger_destroy(logger);
    return 0;
}
```

The complete, copyable example is in [`examples/c/basic.c`](../examples/c/basic.c).

## Rules used throughout these documents

- **Must** means a requirement for a correct caller. **Should** means a strong recommendation.
- Initialize public configuration, field, and event structs with their matching `*_init` function. Do not guess `struct_size` values.
- A logger returned by `mmlog_logger_create` is opaque. Do not allocate, copy, or inspect its internals.
- C logging macros use C variadic arguments. Code crossing a language boundary should use `mmlog_logger_write` instead; see [Language bindings](language-bindings.md).
- Strings passed to a logging call are borrowed only while that synchronous call runs. Keep them alive until the call returns.
- Stop all threads that use a logger before calling `mmlog_logger_destroy`.
- Do not log passwords, access tokens, private keys, or other secrets.
- The core is synchronous and has no third-party runtime dependency. Optional language adapters and build tools are separate from the core.
