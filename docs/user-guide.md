# User guide

This guide explains how to use `m-mdy-m/log.c` from a C application. You do not need to know how a logger is implemented to follow it. For compiling and installing the library, see [Installation and build](installation.md).

## 1. What problem does it solve?

When a program fails, writing `printf("something happened")` everywhere quickly becomes messy. A logging library gives those messages a common format and lets you control how much detail you see.

For example, a web service can write:

```text
2026-10-26T10:22:31.042Z INFO layer=http component=router request=req-123 status=ok | request completed: /health
```

The exact timestamp and optional fields depend on the event. Each log record is rendered as one line. Newlines, terminal escape bytes, and other control characters inside fields or messages are escaped so one message cannot silently turn into several terminal lines.

## 2. Your first program

### Step 1: include the public header

```c
#include <m-mdy-m/log.h>
```

This is the public include path. Do not include `src/log.c` from a source file. The implementation is compiled separately or supplied by the installed library.

### Step 2: create a logger

```c
#include <m-mdy-m/log.h>
#include <stdio.h>

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_status_t rc;

    rc = mmlog_logger_config_init(&config);
    if (rc != MMLLOG_OK) {
        fprintf(stderr, "config: %s\n", mmlog_status_name(rc));
        return 1;
    }

    rc = mmlog_logger_create(&config, &logger);
    if (rc != MMLLOG_OK) {
        fprintf(stderr, "logger: %s\n", mmlog_status_name(rc));
        return 1;
    }

    MMLLOG_INFO(logger, "service %s started on port %d", "api", 8080);
    MMLLOG_WARN(logger, "cache hit rate is %.1f%%", 72.5);

    rc = mmlog_logger_flush(logger);
    if (rc != MMLLOG_OK) {
        fprintf(stderr, "flush: %s\n", mmlog_status_name(rc));
    }

    mmlog_logger_destroy(logger);
    return 0;
}
```

`mmlog_logger_config_init` fills the configuration with safe defaults. `mmlog_logger_create` creates the logger. The `MMLLOG_INFO` and `MMLLOG_WARN` macros add the source filename, line number, and C function name automatically. Always destroy an owned logger when you are finished with it.

Build and run this example from the repository root:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude \
  examples/c/basic.c src/log.c -o basic
./basic
```

You need a C11 compiler. This direct compile is a quick way to experiment; for an application with more than one source file, use the installed library, Make, or CMake as described in [Installation](installation.md).

## 3. Understand the severity levels

The library has six levels, from most detailed to most urgent:

| Level | Use it for | Example |
|---|---|---|
| `TRACE` | Very detailed execution steps | entering a parser state |
| `DEBUG` | Information useful while debugging | selected cache policy |
| `INFO` | Normal events worth recording | service started |
| `WARN` | Unexpected situations that did not stop the operation | retrying after a timeout |
| `ERROR` | An operation failed | database write failed |
| `FATAL` | A serious error after which the application may not be able to continue | startup invariant failed |

`FATAL` does **not** terminate the process. It is a severity label. Your application decides whether and how to exit.

The default minimum level is `TRACE`, so no level is filtered out by the logger-level filter. To show `INFO` and more urgent events only:

```c
mmlog_logger_set_min_level(logger, MMLLOG_LEVEL_INFO);
```

This filter applies to every destination. A file or callback destination can then have its own stricter minimum level.

## 4. Add context to a message

A message answers “what happened?” Context answers “where, to whom, and how long did it take?”

```c
mmlog_fields_t fields;
if (mmlog_fields_init(&fields) == MMLLOG_OK) {
    fields.layer = "http";
    fields.component = "request-handler";
    fields.request_id = "req-7f3a";
    fields.trace_id = "trace-01";
    fields.duration_ns = UINT64_C(12000000); /* 12 milliseconds */
    fields.status = "ok";

    MMLLOG_CTX_INFO(logger, &fields, "request completed: %s", "/health");
}
```

The `*_init` call sets `struct_size` and clears optional fields. The logger only prints a context field when its format flag is enabled and its value is not empty. With the default format, the available fields are:

| Field | Meaning | Unit or note |
|---|---|---|
| `timestamp_unix_ns` | Event time | Unix epoch nanoseconds; zero means “use the current UTC time” |
| `layer` | Broad part of the application | Example: `http`, `storage`, `runtime` |
| `component` | Smaller part of a layer | Example: `router`, `cache`, `sqlite` |
| `file`, `line` | Source location | Filled automatically by C convenience macros |
| `function` | Function that reported the event | Filled automatically by C convenience macros |
| `request_id` | Identifier for one request or job | Use a non-secret, bounded identifier |
| `trace_id` | Identifier shared across related operations | Useful across components |
| `duration_ns` | Time spent | Nanoseconds; rendered as milliseconds |
| `status` | Outcome label | Example: `ok`, `timeout`, `failed` |

The record ABI in [ABI reference](abi.md) can set `file`, `line`, and `function` explicitly. For a foreign language, use that interface instead of the C convenience macros.

## 5. Choose where logs go

By default, records go to standard error (`stderr`). The output can be selected when creating the logger:

```c
config.output = MMLLOG_OUTPUT_STDERR; /* default */
config.output = MMLLOG_OUTPUT_STDOUT;
config.output = MMLLOG_OUTPUT_NONE;   /* no default console output */
```

`MMLLOG_OUTPUT_NONE` does not disable file or callback destinations. It only disables the default console destination.

### Write to a file

```c
mmlog_status_t rc = mmlog_logger_add_file(
    logger, "service.log", MMLLOG_LEVEL_INFO);
if (rc != MMLLOG_OK) {
    fprintf(stderr, "could not add file sink: %s\n", mmlog_status_name(rc));
}
```

The library opens the file in append-binary mode and owns the opened file handle. Do not close that handle yourself; the library does not expose it. The logger closes file sinks when it is destroyed. Each file sink has its own minimum level.

### Receive records in your own code

A callback is useful when the application needs to forward messages to an in-memory collector or test them:

```c
#include <m-mdy-m/log.h>
#include <stdio.h>

static void collect_log(void *user_data, int32_t level,
                        const char *line, size_t line_length) {
    (void)user_data;
    /* `line` is not followed by a newline. Copy it before returning if needed later. */
    fprintf(stderr, "captured %d: %.*s\n", (int)level,
            (int)line_length, line);
}

/* After creating `logger`: */
if (mmlog_logger_add_callback(logger, collect_log, NULL,
                              MMLLOG_LEVEL_WARN) != MMLLOG_OK) {
    /* Handle the error. */
}
```

The callback receives a sanitized plain-text record, not the original `printf` format string. The pointer is temporary and is valid only until the callback returns. Copy the bytes if you need to keep them. Callbacks run after the internal lock is released, so they may log again; however, simultaneous writers can invoke a callback concurrently. Protect your callback's own shared state.

The implementation allows up to 16 combined file and callback destinations per logger. A seventeenth destination returns `MMLLOG_EFULL`.

## 6. Quiet mode and colours

Quiet mode hides only the default console output:

```c
mmlog_logger_set_quiet(logger, 1); /* hide console records */
mmlog_logger_set_quiet(logger, 0); /* show console records again */
```

File and callback destinations still receive messages that pass their filters.

Colour behavior is controlled by:

```c
config.color_mode = MMLLOG_COLOR_AUTO;   /* default */
config.color_mode = MMLLOG_COLOR_ALWAYS;
config.color_mode = MMLLOG_COLOR_NEVER;
```

`AUTO` uses colours only when the selected console stream is a terminal. A non-empty `NO_COLOR` environment variable or `TERM=dumb` disables colours in `AUTO` mode. Colour is used only for the console output; file and callback output is plain text.

## 7. Change the line format

The default format includes the UTC timestamp, severity, all available context fields, source file and line, then the message. To omit a field, clear its bit in `config.format_flags` before creating the logger. For example, to omit the timestamp:

```c
config.format_flags &= ~MMLLOG_F_TIMESTAMP;
```

The flags are `MMLLOG_F_TIMESTAMP`, `MMLLOG_F_LAYER`, `MMLLOG_F_COMPONENT`, `MMLLOG_F_SOURCE`, `MMLLOG_F_FUNCTION`, `MMLLOG_F_REQUEST_ID`, `MMLLOG_F_TRACE_ID`, `MMLLOG_F_DURATION`, and `MMLLOG_F_STATUS`. `MMLLOG_F_ALL` enables them all. The severity and message separator remain present even when other flags are cleared.

See [CLI format](architecture.md#record-format) for the rendering rules and escaping behavior.

## 8. Check return values

Most operations return `mmlog_status_t`. `MMLLOG_OK` means success; other values identify invalid arguments, memory allocation failure, I/O failure, or a full sink list.

```c
mmlog_status_t rc = mmlog_logger_add_file(logger, "service.log", MMLLOG_LEVEL_DEBUG);
if (rc != MMLLOG_OK) {
    fprintf(stderr, "add file sink failed: %s\n", mmlog_status_name(rc));
}
```

Use `mmlog_status_name` for a human-readable description and see the [ABI reference](abi.md#status-codes) for numeric values. A filtered-out event is not an error: `mmlog_logger_write` returns `MMLLOG_OK` when the event is below the configured minimum level.

## 9. Using the logger from multiple threads

The logger serializes its core state and output operations. Independent threads may write to a logger at the same time, but you **must stop and join those threads before destroying the logger**. Destroying a logger concurrently with a write or configuration change is invalid.

Callbacks execute outside the internal lock to avoid deadlocks and allow re-entry. For this reason, a callback can run at the same time on multiple writer threads, and callback completion order is not guaranteed to match record order. Make callback state thread-safe if several threads can write.

## 10. Practical advice

- Put IDs and outcomes in context fields instead of repeating them in every message.
- Use `DEBUG` for verbose diagnostic details and `INFO` for events operators need to see.
- Keep messages useful and concise. This library does not provide log rotation, a JSON serializer, network transport, or an asynchronous queue.
- Do not log passwords, credentials, access tokens, session cookies, or private user data.
- Do not assume `FATAL` exits the process or flushes unrelated output streams. Handle shutdown in the application.
- If the library is used from Python, Rust, Go, Lua, or another FFI, use the record API rather than calling a C variadic function. See [Language bindings](language-bindings.md).
