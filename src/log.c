#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#  define _POSIX_C_SOURCE 200809L
#endif

/*
 * m-mdy-m-log — minimal C logger with a stable, non-variadic record ABI.
 * SPDX-License-Identifier: MIT
 *
 * Ancestry: m-mdy-m/log.c is a public fork of rxi/log.c. This implementation
 * keeps the small logging-library philosophy while defining a new ABI and
 * hardening the CLI formatter. See NOTICE for upstream attribution.
 */
#include <m-mdy-m/log.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  include <io.h>
#  include <windows.h>
#  define MMLLOG_LOCK_TYPE SRWLOCK
#  define MMLLOG_ISATTY(fd) (_isatty(fd) != 0)
#  define MMLLOG_FILENO(stream) _fileno(stream)
#else
#  include <stdatomic.h>
#  include <unistd.h>
#  define MMLLOG_LOCK_TYPE atomic_flag
#  define MMLLOG_ISATTY(fd) (isatty(fd) != 0)
#  define MMLLOG_FILENO(stream) fileno(stream)
#endif

#define MMLLOG_MAX_SINKS 16u
#define MMLLOG_MAX_FIELD_BYTES 256u
#define MMLLOG_DEFAULT_FLAGS MMLLOG_F_ALL

typedef enum sink_kind { SINK_FILE = 1, SINK_CALLBACK = 2 } sink_kind_t;
typedef struct mmlog_sink {
    sink_kind_t kind;
    int32_t min_level;
    FILE *file;
    mmlog_sink_fn callback;
    void *user_data;
} mmlog_sink_t;

struct mmlog_logger {
    MMLLOG_LOCK_TYPE lock;
    int32_t min_level;
    mmlog_color_mode_t color_mode;
    mmlog_output_t output;
    uint32_t format_flags;
    size_t max_message_bytes;
    int quiet;
    size_t sink_count;
    mmlog_sink_t sinks[MMLLOG_MAX_SINKS];
};

typedef struct string_buffer {
    char *data;
    size_t length;
    size_t capacity;
    size_t limit;
} string_buffer_t;

static const char *const LEVEL_NAMES[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static const char *const LEVEL_COLORS[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};

static void logger_lock(mmlog_logger_t *logger) {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&logger->lock);
#else
    while (atomic_flag_test_and_set_explicit(&logger->lock, memory_order_acquire)) {
        /* Short critical sections only; user callbacks are never invoked while locked. */
    }
#endif
}
static void logger_unlock(mmlog_logger_t *logger) {
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&logger->lock);
#else
    atomic_flag_clear_explicit(&logger->lock, memory_order_release);
#endif
}
static int valid_level(int32_t level) { return level >= MMLLOG_LEVEL_TRACE && level <= MMLLOG_LEVEL_FATAL; }
static int valid_color(mmlog_color_mode_t mode) {
    return mode == MMLLOG_COLOR_AUTO || mode == MMLLOG_COLOR_ALWAYS || mode == MMLLOG_COLOR_NEVER;
}
static int valid_output(mmlog_output_t output) {
    return output == MMLLOG_OUTPUT_STDERR || output == MMLLOG_OUTPUT_STDOUT || output == MMLLOG_OUTPUT_NONE;
}

static int buffer_reserve(string_buffer_t *b, size_t extra) {
    size_t needed, next;
    char *p;
    if (extra > b->limit || b->length > b->limit - extra) return 0;
    needed = b->length + extra + 1u;
    if (needed <= b->capacity) return 1;
    next = b->capacity ? b->capacity : 128u;
    while (next < needed) {
        if (next > (b->limit + 1u) / 2u) { next = b->limit + 1u; break; }
        next *= 2u;
    }
    if (next < needed) return 0;
    p = (char *)realloc(b->data, next);
    if (!p) return 0;
    b->data = p;
    b->capacity = next;
    return 1;
}
static int buffer_append_n(string_buffer_t *b, const char *s, size_t n) {
    if (n == 0) {
        if (!b->data && !buffer_reserve(b, 0)) return 0;
        b->data[b->length] = '\0';
        return 1;
    }
    if (!s || !buffer_reserve(b, n)) return 0;
    memcpy(b->data + b->length, s, n);
    b->length += n;
    b->data[b->length] = '\0';
    return 1;
}
static int buffer_append(string_buffer_t *b, const char *s) {
    return s ? buffer_append_n(b, s, strlen(s)) : 1;
}
static int buffer_append_u64(string_buffer_t *b, uint64_t value) {
    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)value);
    return n >= 0 && (size_t)n < sizeof(tmp) && buffer_append_n(b, tmp, (size_t)n);
}
static int buffer_append_escape(string_buffer_t *b, const char *s, size_t n, size_t raw_limit) {
    size_t i, take = n < raw_limit ? n : raw_limit;
    char escaped[5];
    if (!s) return 1;
    for (i = 0; i < take; ++i) {
        unsigned char c = (unsigned char)s[i];
        const char *short_escape = NULL;
        switch (c) {
            case '\n': short_escape = "\\n"; break;
            case '\r': short_escape = "\\r"; break;
            case '\t': short_escape = "\\t"; break;
            case '\b': short_escape = "\\b"; break;
            case '\f': short_escape = "\\f"; break;
            case '\v': short_escape = "\\v"; break;
            case '\0': short_escape = "\\x00"; break;
            case 0x1b: short_escape = "\\x1B"; break;
            default: break;
        }
        if (short_escape) {
            if (!buffer_append(b, short_escape)) return 0;
        } else if (c < 0x20u || c == 0x7fu) {
            int k = snprintf(escaped, sizeof(escaped), "\\x%02X", (unsigned)c);
            if (k != 4 || !buffer_append_n(b, escaped, 4u)) return 0;
        } else if (!buffer_append_n(b, (const char *)&s[i], 1u)) {
            return 0;
        }
    }
    if (n > take && !buffer_append(b, "...")) return 0;
    return 1;
}
static size_t cstr_bounded_length(const char *s, size_t max) {
    size_t n = 0;
    if (!s) return 0;
    while (n < max && s[n] != '\0') ++n;
    return n;
}
static int append_metadata(string_buffer_t *b, const char *label, const char *value) {
    size_t n;
    if (!value || !value[0]) return 1;
    if (!buffer_append(b, " ") || !buffer_append(b, label) || !buffer_append(b, "=")) return 0;
    n = cstr_bounded_length(value, MMLLOG_MAX_FIELD_BYTES + 1u);
    return buffer_append_escape(b, value, n, MMLLOG_MAX_FIELD_BYTES);
}
static uint64_t current_time_unix_ns(void) {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) != TIME_UTC) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
static int utc_broken_down(time_t value, struct tm *out) {
#if defined(_WIN32)
    return gmtime_s(out, &value) == 0;
#else
    return gmtime_r(&value, out) != NULL;
#endif
}
static int append_timestamp(string_buffer_t *b, uint64_t unix_ns) {
    time_t seconds = (time_t)(unix_ns / UINT64_C(1000000000));
    unsigned millis = (unsigned)((unix_ns / UINT64_C(1000000)) % 1000u);
    struct tm tm_value;
    char stamp[40];
    size_t n;
    if (!utc_broken_down(seconds, &tm_value)) return buffer_append(b, "time-unavailable ");
    n = strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S", &tm_value);
    if (n == 0) return buffer_append(b, "time-unavailable ");
    stamp[n] = '\0';
    if (!buffer_append(b, stamp) || !buffer_append(b, ".")) return 0;
    {
        char ms[8];
        int k = snprintf(ms, sizeof(ms), "%03uZ ", millis);
        if (k < 0 || (size_t)k >= sizeof(ms)) return 0;
        return buffer_append_n(b, ms, (size_t)k);
    }
}
static const char *status_color(int32_t level) {
    return valid_level(level) ? LEVEL_COLORS[level] : "";
}
static int render_line(mmlog_logger_t *logger, const mmlog_event_t *event,
                       int color, char **out_line, size_t *out_length) {
    size_t max_output, msg_len, field_limit = MMLLOG_MAX_FIELD_BYTES;
    uint64_t timestamp = event->timestamp_unix_ns ? event->timestamp_unix_ns : current_time_unix_ns();
    string_buffer_t b = {0};
    const uint32_t flags = logger->format_flags;
    const char *level_name = mmlog_level_name(event->level);
    int ok = 1;
    if (logger->max_message_bytes > (SIZE_MAX - 8192u) / 4u) return 0;
    max_output = logger->max_message_bytes * 4u + 8192u;
    b.limit = max_output;
    if ((flags & MMLLOG_F_TIMESTAMP) && !append_timestamp(&b, timestamp)) ok = 0;
    if (ok && color) ok = buffer_append(&b, status_color(event->level));
    if (ok) ok = buffer_append(&b, level_name);
    if (ok && color) ok = buffer_append(&b, "\x1b[0m");
    if (ok) ok = buffer_append(&b, " ");
    if (ok && (flags & MMLLOG_F_LAYER)) ok = append_metadata(&b, "layer", event->layer);
    if (ok && (flags & MMLLOG_F_COMPONENT)) ok = append_metadata(&b, "component", event->component);
    if (ok && (flags & MMLLOG_F_SOURCE) && event->file && event->file[0]) {
        ok = buffer_append(&b, " ") && buffer_append_escape(&b, event->file,
            cstr_bounded_length(event->file, field_limit + 1u), field_limit);
        if (ok && event->line) ok = buffer_append(&b, ":") && buffer_append_u64(&b, event->line);
    }
    if (ok && (flags & MMLLOG_F_FUNCTION)) ok = append_metadata(&b, "function", event->function);
    if (ok && (flags & MMLLOG_F_REQUEST_ID)) ok = append_metadata(&b, "request", event->request_id);
    if (ok && (flags & MMLLOG_F_TRACE_ID)) ok = append_metadata(&b, "trace", event->trace_id);
    if (ok && (flags & MMLLOG_F_DURATION) && event->duration_ns) {
        char duration[48];
        double ms = (double)event->duration_ns / 1000000.0;
        int n = snprintf(duration, sizeof(duration), " duration=%.3fms", ms);
        ok = n >= 0 && (size_t)n < sizeof(duration) && buffer_append_n(&b, duration, (size_t)n);
    }
    if (ok && (flags & MMLLOG_F_STATUS)) ok = append_metadata(&b, "status", event->status);
    if (ok) ok = buffer_append(&b, " | ");
    msg_len = event->message_length;
    if (msg_len > logger->max_message_bytes) msg_len = logger->max_message_bytes;
    if (ok && event->message) ok = buffer_append_escape(&b, event->message, msg_len,
                                                         logger->max_message_bytes);
    if (ok && event->message_length > logger->max_message_bytes) {
        if (!buffer_append(&b, "...")) ok = 0;
    }
    if (!ok) { free(b.data); return 0; }
    if (!b.data) {
        b.data = (char *)malloc(1u);
        if (!b.data) return 0;
        b.data[0] = '\0';
    }
    *out_line = b.data;
    *out_length = b.length;
    return 1;
}
static int env_disables_color(void) {
    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");
    return (no_color && no_color[0] != '\0') || (term && strcmp(term, "dumb") == 0);
}
static int console_has_color(mmlog_logger_t *logger, FILE *stream) {
    if (logger->color_mode == MMLLOG_COLOR_NEVER) return 0;
    if (logger->color_mode == MMLLOG_COLOR_ALWAYS) return 1;
    if (env_disables_color() || !stream) return 0;
    if (!MMLLOG_ISATTY(MMLLOG_FILENO(stream))) return 0;
#if defined(_WIN32)
    {
        HANDLE handle = (HANDLE)_get_osfhandle(MMLLOG_FILENO(stream));
        DWORD mode;
        if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode)) return 0;
        if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            DWORD updated = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (!SetConsoleMode(handle, updated)) return 0;
        }
    }
#endif
    return 1;
}
static int output_line(FILE *stream, const char *line, size_t len) {
    if (!stream) return 0;
    if (len && fwrite(line, 1u, len, stream) != len) return 0;
    if (fputc('\n', stream) == EOF) return 0;
    return fflush(stream) == 0;
}

uint32_t mmlog_abi_version(void) { return MMLLOG_ABI_VERSION; }
const char *mmlog_level_name(int32_t level) {
    return valid_level(level) ? LEVEL_NAMES[level] : "UNKNOWN";
}
const char *mmlog_status_name(mmlog_status_t status) {
    switch (status) {
        case MMLLOG_OK: return "ok";
        case MMLLOG_EINVAL: return "invalid argument";
        case MMLLOG_ENOMEM: return "out of memory";
        case MMLLOG_EIO: return "I/O error";
        case MMLLOG_ELIMIT: return "configured limit exceeded";
        case MMLLOG_EFULL: return "sink capacity reached";
        default: return "unknown status";
    }
}
mmlog_status_t mmlog_logger_config_init(mmlog_logger_config_t *config) {
    if (!config) return MMLLOG_EINVAL;
    memset(config, 0, sizeof(*config));
    config->struct_size = (uint32_t)sizeof(*config);
    config->min_level = MMLLOG_LEVEL_TRACE;
    config->color_mode = MMLLOG_COLOR_AUTO;
    config->output = MMLLOG_OUTPUT_STDERR;
    config->format_flags = MMLLOG_DEFAULT_FLAGS;
    config->max_message_bytes = MMLLOG_DEFAULT_MESSAGE_BYTES;
    return MMLLOG_OK;
}
mmlog_status_t mmlog_fields_init(mmlog_fields_t *fields) {
    if (!fields) return MMLLOG_EINVAL;
    memset(fields, 0, sizeof(*fields));
    fields->struct_size = (uint32_t)sizeof(*fields);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_event_init(mmlog_event_t *event) {
    if (!event) return MMLLOG_EINVAL;
    memset(event, 0, sizeof(*event));
    event->struct_size = (uint32_t)sizeof(*event);
    event->level = MMLLOG_LEVEL_INFO;
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_create(const mmlog_logger_config_t *config,
                                   mmlog_logger_t **out_logger) {
    mmlog_logger_t *logger;
    mmlog_logger_config_t defaults;
    if (!out_logger) return MMLLOG_EINVAL;
    *out_logger = NULL;
    if (!config) {
        (void)mmlog_logger_config_init(&defaults);
        config = &defaults;
    }
    if (config->struct_size < MMLLOG_CONFIG_V1_SIZE ||
        !valid_level((int32_t)config->min_level) ||
        !valid_color((mmlog_color_mode_t)config->color_mode) ||
        !valid_output((mmlog_output_t)config->output) ||
        config->max_message_bytes == 0u ||
        config->max_message_bytes > MMLLOG_MAX_MESSAGE_BYTES) return MMLLOG_EINVAL;
    logger = (mmlog_logger_t *)calloc(1u, sizeof(*logger));
    if (!logger) return MMLLOG_ENOMEM;
#if defined(_WIN32)
    InitializeSRWLock(&logger->lock);
#else
    atomic_flag_clear(&logger->lock);
#endif
    logger->min_level = (int32_t)config->min_level;
    logger->color_mode = (mmlog_color_mode_t)config->color_mode;
    logger->output = (mmlog_output_t)config->output;
    logger->format_flags = config->format_flags & MMLLOG_F_ALL;
    logger->max_message_bytes = (size_t)config->max_message_bytes;
    *out_logger = logger;
    return MMLLOG_OK;
}
void mmlog_logger_destroy(mmlog_logger_t *logger) {
    size_t i;
    if (!logger) return;
    /* Caller must stop concurrent writers before destruction. */
    for (i = 0; i < logger->sink_count; ++i) {
        if (logger->sinks[i].kind == SINK_FILE && logger->sinks[i].file) {
            (void)fclose(logger->sinks[i].file);
        }
    }
    free(logger);
}
mmlog_status_t mmlog_logger_set_min_level(mmlog_logger_t *logger, int32_t min_level) {
    if (!logger || !valid_level(min_level)) return MMLLOG_EINVAL;
    logger_lock(logger); logger->min_level = min_level; logger_unlock(logger);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_set_color_mode(mmlog_logger_t *logger, mmlog_color_mode_t mode) {
    if (!logger || !valid_color(mode)) return MMLLOG_EINVAL;
    logger_lock(logger); logger->color_mode = mode; logger_unlock(logger);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_set_quiet(mmlog_logger_t *logger, int quiet) {
    if (!logger || (quiet != 0 && quiet != 1)) return MMLLOG_EINVAL;
    logger_lock(logger); logger->quiet = quiet; logger_unlock(logger);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_add_file(mmlog_logger_t *logger, const char *path, int32_t min_level) {
    FILE *file;
    if (!logger || !path || !path[0] || !valid_level(min_level)) return MMLLOG_EINVAL;
    file = fopen(path, "ab");
    if (!file) return MMLLOG_EIO;
    logger_lock(logger);
    if (logger->sink_count >= MMLLOG_MAX_SINKS) {
        logger_unlock(logger); fclose(file); return MMLLOG_EFULL;
    }
    logger->sinks[logger->sink_count++] = (mmlog_sink_t){SINK_FILE, min_level, file, NULL, NULL};
    logger_unlock(logger);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_add_callback(mmlog_logger_t *logger, mmlog_sink_fn callback,
                                         void *user_data, int32_t min_level) {
    if (!logger || !callback || !valid_level(min_level)) return MMLLOG_EINVAL;
    logger_lock(logger);
    if (logger->sink_count >= MMLLOG_MAX_SINKS) { logger_unlock(logger); return MMLLOG_EFULL; }
    logger->sinks[logger->sink_count++] = (mmlog_sink_t){SINK_CALLBACK, min_level, NULL, callback, user_data};
    logger_unlock(logger);
    return MMLLOG_OK;
}
mmlog_status_t mmlog_logger_flush(mmlog_logger_t *logger) {
    size_t i;
    int ok = 1;
    if (!logger) return MMLLOG_EINVAL;
    logger_lock(logger);
    if (logger->output == MMLLOG_OUTPUT_STDERR && fflush(stderr) != 0) ok = 0;
    if (logger->output == MMLLOG_OUTPUT_STDOUT && fflush(stdout) != 0) ok = 0;
    for (i = 0; i < logger->sink_count; ++i) {
        if (logger->sinks[i].kind == SINK_FILE && fflush(logger->sinks[i].file) != 0) ok = 0;
    }
    logger_unlock(logger);
    return ok ? MMLLOG_OK : MMLLOG_EIO;
}

mmlog_status_t mmlog_logger_write(mmlog_logger_t *logger, const mmlog_event_t *event) {
    char *plain_line = NULL, *colored_line = NULL;
    size_t plain_length = 0, colored_length = 0, i, callback_count = 0;
    mmlog_sink_t callback_sinks[MMLLOG_MAX_SINKS];
    FILE *stream = NULL;
    int use_color = 0, output_ok = 1;
    mmlog_status_t status = MMLLOG_OK;
    if (!logger || !event || event->struct_size < MMLLOG_EVENT_V1_SIZE ||
        !valid_level(event->level) || (!event->message && event->message_length != 0u)) return MMLLOG_EINVAL;
    logger_lock(logger);
    if (event->level < logger->min_level) { logger_unlock(logger); return MMLLOG_OK; }
    if (!render_line(logger, event, 0, &plain_line, &plain_length)) {
        logger_unlock(logger); return MMLLOG_ENOMEM;
    }
    if (!logger->quiet && logger->output != MMLLOG_OUTPUT_NONE) {
        stream = logger->output == MMLLOG_OUTPUT_STDOUT ? stdout : stderr;
        use_color = console_has_color(logger, stream);
        if (use_color) {
            if (!render_line(logger, event, 1, &colored_line, &colored_length)) {
                free(plain_line); logger_unlock(logger); return MMLLOG_ENOMEM;
            }
        }
        if (!output_line(stream, use_color ? colored_line : plain_line,
                         use_color ? colored_length : plain_length)) output_ok = 0;
    }
    for (i = 0; i < logger->sink_count; ++i) {
        mmlog_sink_t *sink = &logger->sinks[i];
        if (event->level < sink->min_level) continue;
        if (sink->kind == SINK_FILE) {
            if (!output_line(sink->file, plain_line, plain_length)) output_ok = 0;
        } else if (callback_count < MMLLOG_MAX_SINKS) {
            callback_sinks[callback_count++] = *sink;
        }
    }
    logger_unlock(logger);
    /* External code runs outside the spin lock, so callbacks may safely log again. */
    for (i = 0; i < callback_count; ++i) {
        callback_sinks[i].callback(callback_sinks[i].user_data, event->level,
                                   plain_line, plain_length);
    }
    free(colored_line);
    free(plain_line);
    if (!output_ok) status = MMLLOG_EIO;
    return status;
}

mmlog_status_t mmlog_logf(mmlog_logger_t *logger, int32_t level,
                          const mmlog_fields_t *fields, const char *file, uint32_t line,
                          const char *function, const char *format, ...) {
    mmlog_event_t event;
    char *message;
    size_t cap, length;
    int n;
    va_list args;
    if (!logger || !valid_level(level) || !format) return MMLLOG_EINVAL;
    if (fields && fields->struct_size < MMLLOG_FIELDS_V1_SIZE) return MMLLOG_EINVAL;
    cap = logger->max_message_bytes + 1u;
    message = (char *)malloc(cap);
    if (!message) return MMLLOG_ENOMEM;
    va_start(args, format);
    n = vsnprintf(message, cap, format, args);
    va_end(args);
    if (n < 0) { free(message); return MMLLOG_EINVAL; }
    length = (size_t)n < cap ? (size_t)n : cap - 1u;
    if ((size_t)n >= cap && cap >= 4u) {
        message[cap - 4u] = '.'; message[cap - 3u] = '.'; message[cap - 2u] = '.';
        message[cap - 1u] = '\0'; length = cap - 1u;
    }
    (void)mmlog_event_init(&event);
    event.level = level;
    event.file = file;
    event.line = line;
    event.function = function;
    event.message = message;
    event.message_length = length;
    if (fields) {
        event.timestamp_unix_ns = fields->timestamp_unix_ns;
        event.duration_ns = fields->duration_ns;
        event.layer = fields->layer;
        event.component = fields->component;
        event.request_id = fields->request_id;
        event.trace_id = fields->trace_id;
        event.status = fields->status;
    }
    {
        mmlog_status_t result = mmlog_logger_write(logger, &event);
        free(message);
        return result;
    }
}
