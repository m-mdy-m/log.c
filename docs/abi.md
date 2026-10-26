# C ABI reference

This is the precise reference for the public C binary interface (ABI) of `m-mdy-m/log.c`. The [user guide](user-guide.md) explains typical use. This file documents the types, version, status codes, ownership and rules that C callers and foreign-language runtimes must follow.

The authoritative header is `include/m-mdy-m/log.h`. Current ABI: **1.0**. The encoded value from `mmlog_abi_version()` is `0x00010000`. If the header and this document disagree, trust the header and report the mismatch.

## 1. What does “ABI” mean?

A source API tells the compiler which functions and types exist. An ABI is how compiled code agrees on function names, calling conventions, integer widths, struct layout, and how data crosses the library boundary. This matters when an application loads a prebuilt shared library or when Python/Rust/Go calls the C code.

For cross-language use, call `mmlog_logger_write`. It takes one event struct and does **not** use C variable arguments. `mmlog_logf` is a convenience function for C callers only.

## 2. Version query

```c
uint32_t encoded = mmlog_abi_version();
uint32_t major = encoded >> 16;
uint32_t minor = encoded & 0xffffu;
if (major != MMLLOG_ABI_MAJOR) {
    /* Do not assume the caller and the loaded library are compatible. */
}
```

The upper 16 bits hold the ABI major version; the lower 16 bits hold the minor version. The C header defines `MMLLOG_ABI_MAJOR`, `MMLLOG_ABI_MINOR`, and `MMLLOG_ABI_VERSION`.

An ABI major mismatch means the binding should be considered incompatible. A compatible minor release may add functionality while preserving the existing ABI. Bindings should query the runtime rather than assuming the shared library found by the loader is the expected version.

## 3. Public scalar types

| Type | Definition | Purpose |
|---|---|---|
| `mmlog_logger_t` | Opaque struct | Logger handle; its fields are private. |
| `mmlog_level_t` | `int32_t` | Severity values. |
| `mmlog_status_t` | `int32_t` | Function result values. |
| `mmlog_color_mode_t` | `int32_t` | Console colour mode. |
| `mmlog_output_t` | `int32_t` | Default console output selection. |
| `size_t` | Platform-defined unsigned size | Byte length for the message/callback line. Must match the calling process ABI. |
| `uint64_t` | 64-bit unsigned integer | Nanosecond timestamps and durations. |

Do not create your own `mmlog_logger_t` values, copy one by value, or inspect its memory. Obtain the pointer from `mmlog_logger_create` and release it with `mmlog_logger_destroy`.

## 4. Severity values

| Value | Constant | Meaning |
|---:|---|---|
| 0 | `MMLLOG_LEVEL_TRACE` | Highly detailed diagnostics |
| 1 | `MMLLOG_LEVEL_DEBUG` | Debugging information |
| 2 | `MMLLOG_LEVEL_INFO` | Normal operational events |
| 3 | `MMLLOG_LEVEL_WARN` | Unexpected but recoverable conditions |
| 4 | `MMLLOG_LEVEL_ERROR` | Failed operation |
| 5 | `MMLLOG_LEVEL_FATAL` | Critical event label; does not exit the process |

The numeric values are part of ABI 1 and must not be reordered.

## 5. Status codes

| Value | Constant | Meaning |
|---:|---|---|
| 0 | `MMLLOG_OK` | Success; filtered-out messages also return success. |
| -1 | `MMLLOG_EINVAL` | Invalid pointer, struct size, level, mode, value, or event shape. |
| -2 | `MMLLOG_ENOMEM` | Required memory allocation failed. |
| -3 | `MMLLOG_EIO` | File or console I/O failed, or a file could not be opened. |
| -4 | `MMLLOG_ELIMIT` | Reserved status in ABI 1.0; the current core does not return it from its public operations. |
| -5 | `MMLLOG_EFULL` | The logger already has its maximum number of file and callback destinations. |

`mmlog_status_name(status)` returns a static, human-readable name. It is safe to call for an unknown numeric value; unknown values return `"unknown status"`.

## 6. Initializers and struct-size rules

Use the provided initialization functions:

```c
mmlog_logger_config_t config;
mmlog_fields_t fields;
mmlog_event_t event;

mmlog_logger_config_init(&config);
mmlog_fields_init(&fields);
mmlog_event_init(&event);
```

Each initializer clears the struct and sets `struct_size`. The event initializer also sets its level to `MMLLOG_LEVEL_INFO`. Do not fill a public struct with uninitialized stack memory or guess the struct-size value. The current v1 layout requirements are expressed by `MMLLOG_CONFIG_V1_SIZE`, `MMLLOG_FIELDS_V1_SIZE`, and `MMLLOG_EVENT_V1_SIZE`.

A null config pointer passed to `mmlog_logger_create(NULL, &logger)` asks the library to use defaults. The `out_logger` pointer itself must not be null; on failure the function sets `*out_logger` to null.

## 7. Configuration structure

`mmlog_logger_config_t` contains:

| Field | Type | Default | Meaning |
|---|---|---|---|
| `struct_size` | `uint32_t` | Set by initializer | Size/version guard. |
| `min_level` | `uint32_t` | `MMLLOG_LEVEL_TRACE` | Logger-wide minimum severity. |
| `color_mode` | `uint32_t` | `MMLLOG_COLOR_AUTO` | Console colour policy. |
| `output` | `uint32_t` | `MMLLOG_OUTPUT_STDERR` | Default console stream or none. |
| `format_flags` | `uint32_t` | `MMLLOG_F_ALL` | Optional timestamp and context fields to print. |
| `max_message_bytes` | `uint32_t` | 16384 | Message length limit, from 1 through 1048576 bytes. |

Colour values: `MMLLOG_COLOR_AUTO = 0`, `MMLLOG_COLOR_ALWAYS = 1`, `MMLLOG_COLOR_NEVER = 2`. Output values: `MMLLOG_OUTPUT_STDERR = 0`, `MMLLOG_OUTPUT_STDOUT = 1`, `MMLLOG_OUTPUT_NONE = 2`.

The hard message limit is `MMLLOG_MAX_MESSAGE_BYTES` (1 MiB). `MMLLOG_DEFAULT_MESSAGE_BYTES` is 16 KiB. In ABI 1.0, unknown format bits are masked off when the logger is created.

## 8. Event structure

`mmlog_event_t` is the stable record interface intended for FFI. Its fields are:

| Field | Type | Meaning |
|---|---|---|
| `struct_size` | `uint32_t` | Set by `mmlog_event_init`. |
| `level` | `int32_t` | One of the six severity constants. |
| `timestamp_unix_ns` | `uint64_t` | UTC Unix timestamp in nanoseconds; zero means use current time. |
| `duration_ns` | `uint64_t` | Operation duration in nanoseconds. |
| `line` | `uint32_t` | Source line, if known. |
| `reserved` | `uint32_t` | Reserved; leave zero. |
| `layer` | `const char *` | Broad component name. |
| `component` | `const char *` | Specific component name. |
| `file` | `const char *` | Source file or module name. |
| `function` | `const char *` | Function or operation name. |
| `request_id` | `const char *` | Request/job identifier. |
| `trace_id` | `const char *` | Trace identifier. |
| `status` | `const char *` | Outcome label. |
| `message` | `const char *` | Message bytes. May be null only when length is zero. |
| `message_length` | `size_t` | Message byte count; do not assume NUL termination. |

Event strings and message memory are borrowed only until `mmlog_logger_write` returns. The call formats the record synchronously and does not keep pointers to the event's message or metadata. Metadata strings use C-string termination; the message uses the explicit `message_length`, so an embedded NUL byte in a message is representable and escaped in the rendered line.

## 9. Context fields structure

`mmlog_fields_t` is a convenience wrapper for C's `mmlog_logf` and the `MMLLOG_CTX_*` macros:

| Field | Type | Meaning |
|---|---|---|
| `struct_size` | `uint32_t` | Set by `mmlog_fields_init`. |
| `reserved` | `uint32_t` | Reserved; leave zero. |
| `timestamp_unix_ns` | `uint64_t` | Event timestamp; zero means now. |
| `duration_ns` | `uint64_t` | Duration. |
| `layer` | `const char *` | Broad component. |
| `component` | `const char *` | Specific component. |
| `request_id` | `const char *` | Request/job ID. |
| `trace_id` | `const char *` | Trace ID. |
| `status` | `const char *` | Outcome. |

The `MMLLOG_CTX_TRACE`, `MMLLOG_CTX_DEBUG`, `MMLLOG_CTX_INFO`, `MMLLOG_CTX_WARN`, `MMLLOG_CTX_ERROR`, and `MMLLOG_CTX_FATAL` macros take the logger, a pointer to this structure, and the normal `printf`-style format arguments.

## 10. Format flags

| Flag | Bit | Printed value |
|---|---:|---|
| `MMLLOG_F_TIMESTAMP` | 0 | UTC timestamp with millisecond precision |
| `MMLLOG_F_LAYER` | 1 | `layer=...` |
| `MMLLOG_F_COMPONENT` | 2 | `component=...` |
| `MMLLOG_F_SOURCE` | 3 | Source filename and, when nonzero, `:line` |
| `MMLLOG_F_FUNCTION` | 4 | `function=...` |
| `MMLLOG_F_REQUEST_ID` | 5 | `request=...` |
| `MMLLOG_F_TRACE_ID` | 6 | `trace=...` |
| `MMLLOG_F_DURATION` | 7 | `duration=...ms` |
| `MMLLOG_F_STATUS` | 8 | `status=...` |

`MMLLOG_F_ALL` combines these flags. The severity and ` | ` message separator are always emitted.

## 11. Ownership and lifetime

| Value | Who owns it? | How long must it remain valid? |
|---|---|---|
| Logger pointer | Caller owns the logger lifecycle | Until `mmlog_logger_destroy`; stop all users before destroy. |
| Config/fields/event structs | Caller | For the duration of the call that reads them. |
| Event strings and message | Caller | Until `mmlog_logger_write` returns. |
| File path passed to `mmlog_logger_add_file` | Caller | During the add call. |
| File opened by `mmlog_logger_add_file` | Logger | The logger closes it during destruction. |
| Callback function and `user_data` | Caller | Must remain valid while the logger may invoke the callback. |
| Callback `line` pointer | Library temporary | Only until callback return; copy it if retaining. |

The implementation serializes logger state and synchronous output. Callbacks execute after the logger lock is released; they can re-enter the logger, but concurrent writers can call the same callback at the same time. Protect callback-owned shared state. Destroying a logger concurrently with any other operation is not supported.

## 12. Stable exported operations

| Function | Purpose |
|---|---|
| `mmlog_abi_version` | Return encoded ABI version. |
| `mmlog_level_name` / `mmlog_status_name` | Convert numeric values to static names. |
| `mmlog_logger_config_init` | Initialize a config with defaults. |
| `mmlog_fields_init` / `mmlog_event_init` | Initialize public input structures. |
| `mmlog_logger_create` / `mmlog_logger_destroy` | Create and release a logger. |
| `mmlog_logger_set_min_level` | Set logger-wide filtering. |
| `mmlog_logger_set_color_mode` | Change console colour behavior. |
| `mmlog_logger_set_quiet` | Hide or show console output. |
| `mmlog_logger_add_file` | Add an append-mode file destination. |
| `mmlog_logger_add_callback` | Add an application callback destination. |
| `mmlog_logger_flush` | Flush console and file outputs. |
| `mmlog_logger_write` | Write one stable, non-variadic record. |
| `mmlog_logf` | C variadic convenience function; do not call through a foreign FFI. |

## 13. ABI compatibility checklist

When changing public declarations:

1. Do not reorder or change existing enum numeric values, function parameters, calling conventions, or existing struct fields.
2. Preserve all existing exported symbol names in a compatible ABI release.
3. Make all cross-language entry points non-variadic; do not require FFI clients to call `mmlog_logf`.
4. Add a regression test for every new or changed public behavior.
5. Update this document, the user guide, examples, changelog, package version and ABI policy together.
6. Build and test both static and shared consumers before release.
