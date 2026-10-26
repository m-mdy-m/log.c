# Architecture

This document explains what happens inside `m-mdy-m/log.c` when an application writes a log record. It describes the current implementation, not a promise that every internal detail will remain unchanged. The public promises are defined by [`include/m-mdy-m/log.h`](../include/m-mdy-m/log.h) and the [ABI reference](abi.md).

## 1. Design goals

The implementation aims to be:

- Small and synchronous, without a background logging thread or queue.
- Easy to embed in a C11 application.
- Usable from other languages through a non-variadic record API.
- Predictable about memory bounds and line formatting.
- Extensible through file and callback destinations without requiring an external runtime package.

The core uses the C standard library. The platform-specific pieces are small: POSIX uses `atomic_flag` for its internal lock and Windows uses an SRW lock; terminal detection is implemented for the target OS.

## 2. Record flow

A typical record follows this sequence:

```text
C macro or foreign caller
        |
        v
 mmlog_event_t record
        |
        v
 validate pointer, struct size, level and message length
        |
        v
 apply logger-wide minimum level
        |
        v
 render one plain-text line (escape controls, bound data)
        |
        +------> console (optional ANSI colour)
        |
        +------> file sinks (plain text)
        |
        +------> callback sinks (plain text, after lock release)
```

The C convenience macros call `mmlog_logf`, which formats a `printf`-style message and converts it to the same record interface. Foreign-language callers should construct the record directly and call `mmlog_logger_write`.

## 3. Logger state and destinations

A logger holds its minimum level, colour mode, selected console output, format flags, maximum message bytes, quiet flag, and a fixed list of destinations. The current implementation supports up to 16 combined file and callback destinations per logger.

- **Console:** standard error by default, standard output optionally, or disabled.
- **File:** opened in append mode when added; owned and closed by the logger.
- **Callback:** invoked with the level and a pointer/length pair containing the rendered plain-text line.

The logger-wide minimum level is checked first. Each added file or callback destination also has a minimum level. Quiet mode suppresses only the default console destination; eligible file and callback destinations still receive records.

The logger is synchronous: a write formats and sends the record before the call returns. Console and file writes are flushed at each output operation. `mmlog_logger_flush` is also provided to explicitly flush the selected console stream and file sinks. There is no asynchronous queue, background worker, log rotation, network exporter, or built-in JSON output.

## 4. Record format

With the default format flags, a line is arranged as:

```text
UTC_TIMESTAMP LEVEL layer=... component=... FILE:LINE function=... request=... trace=... duration=...ms status=... | MESSAGE
```

Empty optional values are skipped. Source location is printed only when the source flag is enabled and a filename is supplied. The source line is added when nonzero. The duration is printed only when enabled and nonzero. The separator and message are always present; the other parts are controlled by the `MMLLOG_F_*` flags.

The timestamp is rendered in UTC with millisecond precision, using a form such as `2026-10-26T10:22:31.042Z`. The internal event timestamp is expressed in Unix nanoseconds. A zero timestamp asks the library to use the current UTC time.

### Escaping and bounds

Messages and metadata values are escaped before the line is emitted:

| Input byte | Output |
|---|---|
| newline | `\\n` |
| carriage return | `\\r` |
| tab | `\\t` |
| NUL | `\\x00` |
| ESC (terminal escape) | `\\x1B` |
| other ASCII control bytes and DEL | `\\xHH` |

This preserves a one-record-per-line output format and prevents message text from injecting terminal colour sequences. This is output escaping, not a JSON encoding or a guarantee that arbitrary bytes are valid UTF-8.

The default message limit is 16 KiB; the configured limit may be set up to 1 MiB. Longer messages are shortened and marked with `...`. Metadata values such as layer, component, filename, function, request ID, trace ID and status are bounded to 256 bytes. Those limits prevent unbounded growth from an accidental or untrusted field.

### Console colours

The console may colour the severity label. `AUTO` uses colours only when the selected stream is a terminal and neither a non-empty `NO_COLOR` nor `TERM=dumb` disables them. `ALWAYS` forces colour; `NEVER` disables it. Files and callbacks always receive the uncoloured plain-text line.

## 5. Locking and callbacks

The logger protects its mutable state and synchronous output operations with an internal lock. On POSIX targets this is an `atomic_flag` spin lock; on Windows it is an SRW lock. This is not a promise that destruction can race with other operations.

Callbacks are copied from the destination list while the logger lock is held, then called after the lock is released. This design allows a callback to write another log message without deadlocking on the same logger. The trade-off is that callback invocations from different writer threads can overlap, and callback completion order is not guaranteed to match record order. Callback code must protect its own mutable state.

The owning application must stop and join all workers before destroying the logger. The logger does not provide a reference-counted handle or an API to wait for in-flight writes.

## 6. What this library does not do

The v1 core does not provide:

- a process-global default logger;
- an asynchronous queue or worker thread;
- log rotation or retention policies;
- JSON or structured network output;
- UDP, HTTP, syslog, or remote exporters;
- signal-safe crash logging;
- automatic redaction of secrets;
- fatal-level process termination.

These are intentionally outside the small core. Applications can route callback output to their own infrastructure, but they must implement that behavior themselves.
