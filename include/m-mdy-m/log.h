/*
 * m-mdy-m-log — a small, extensible C logging library.
 * SPDX-License-Identifier: MIT
 */
#ifndef M_MDY_M_LOG_LOG_H
#define M_MDY_M_LOG_LOG_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  define M_MDY_M_LOG_CALL __cdecl
#  if defined(M_MDY_M_LOG_SHARED) && defined(M_MDY_M_LOG_BUILD)
#    define M_MDY_M_LOG_API __declspec(dllexport)
#  elif defined(M_MDY_M_LOG_SHARED)
#    define M_MDY_M_LOG_API __declspec(dllimport)
#  else
#    define M_MDY_M_LOG_API
#  endif
#elif defined(__GNUC__) && defined(M_MDY_M_LOG_SHARED)
#  define M_MDY_M_LOG_CALL
#  define M_MDY_M_LOG_API __attribute__((visibility("default")))
#else
#  define M_MDY_M_LOG_CALL
#  define M_MDY_M_LOG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MMLLOG_ABI_MAJOR 1u
#define MMLLOG_ABI_MINOR 0u
#define MMLLOG_ABI_VERSION ((MMLLOG_ABI_MAJOR << 16u) | MMLLOG_ABI_MINOR)
#define MMLLOG_MAX_MESSAGE_BYTES (1024u * 1024u)
#define MMLLOG_DEFAULT_MESSAGE_BYTES (16u * 1024u)

typedef struct mmlog_logger mmlog_logger_t;

/* Use fixed-width scalar types for FFI-stable function arguments and return values. */
typedef int32_t mmlog_level_t;
enum {
    MMLLOG_LEVEL_TRACE = 0,
    MMLLOG_LEVEL_DEBUG = 1,
    MMLLOG_LEVEL_INFO  = 2,
    MMLLOG_LEVEL_WARN  = 3,
    MMLLOG_LEVEL_ERROR = 4,
    MMLLOG_LEVEL_FATAL = 5
};

typedef int32_t mmlog_status_t;
enum {
    MMLLOG_OK = 0,
    MMLLOG_EINVAL = -1,
    MMLLOG_ENOMEM = -2,
    MMLLOG_EIO = -3,
    MMLLOG_ELIMIT = -4,
    MMLLOG_EFULL = -5
};

typedef int32_t mmlog_color_mode_t;
enum {
    MMLLOG_COLOR_AUTO = 0,
    MMLLOG_COLOR_ALWAYS = 1,
    MMLLOG_COLOR_NEVER = 2
};

typedef int32_t mmlog_output_t;
enum {
    MMLLOG_OUTPUT_STDERR = 0,
    MMLLOG_OUTPUT_STDOUT = 1,
    MMLLOG_OUTPUT_NONE = 2
};

typedef enum mmlog_format_flag {
    MMLLOG_F_TIMESTAMP = 1u << 0,
    MMLLOG_F_LAYER = 1u << 1,
    MMLLOG_F_COMPONENT = 1u << 2,
    MMLLOG_F_SOURCE = 1u << 3,
    MMLLOG_F_FUNCTION = 1u << 4,
    MMLLOG_F_REQUEST_ID = 1u << 5,
    MMLLOG_F_TRACE_ID = 1u << 6,
    MMLLOG_F_DURATION = 1u << 7,
    MMLLOG_F_STATUS = 1u << 8
} mmlog_format_flag_t;

#define MMLLOG_F_ALL (MMLLOG_F_TIMESTAMP | MMLLOG_F_LAYER | MMLLOG_F_COMPONENT | \
                      MMLLOG_F_SOURCE | MMLLOG_F_FUNCTION | MMLLOG_F_REQUEST_ID | \
                      MMLLOG_F_TRACE_ID | MMLLOG_F_DURATION | MMLLOG_F_STATUS)

typedef struct mmlog_logger_config {
    uint32_t struct_size;          /* Set by mmlog_logger_config_init. */
    uint32_t min_level;            /* MMLLOG_* level; defaults to TRACE. */
    uint32_t color_mode;           /* mmlog_color_mode_t; defaults to AUTO. */
    uint32_t output;               /* mmlog_output_t; defaults to STDERR. */
    uint32_t format_flags;          /* MMLLOG_F_* mask; defaults to MMLLOG_F_ALL. */
    uint32_t max_message_bytes;     /* 1..MMLLOG_MAX_MESSAGE_BYTES. */
} mmlog_logger_config_t;

/* Metadata reusable across several C log calls. The struct_size field is mandatory. */
typedef struct mmlog_fields {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t timestamp_unix_ns;    /* 0 means use the current UTC time. */
    uint64_t duration_ns;          /* Used when MMLLOG_F_DURATION is enabled. */
    const char *layer;
    const char *component;
    const char *request_id;
    const char *trace_id;
    const char *status;
} mmlog_fields_t;

/* Stable, non-variadic record interface intended for C, Lua, Rust, Go, Python/ctypes, etc.
 * All string pointers are borrowed only for the duration of mmlog_logger_write().
 * message_length is a byte count; embedded NUL/control bytes are safely escaped. */
typedef struct mmlog_event {
    uint32_t struct_size;
    int32_t level;
    uint64_t timestamp_unix_ns;    /* 0 means now. */
    uint64_t duration_ns;
    uint32_t line;
    uint32_t reserved;
    const char *layer;
    const char *component;
    const char *file;
    const char *function;
    const char *request_id;
    const char *trace_id;
    const char *status;
    const char *message;
    size_t message_length;
} mmlog_event_t;

/* A callback receives one sanitized plain-text line without a trailing newline.
 * Callbacks run synchronously after the internal logger lock is released. */
#define MMLLOG_CONFIG_V1_SIZE (offsetof(mmlog_logger_config_t, max_message_bytes) + sizeof(uint32_t))
#define MMLLOG_FIELDS_V1_SIZE (offsetof(mmlog_fields_t, status) + sizeof(const char *))
#define MMLLOG_EVENT_V1_SIZE (offsetof(mmlog_event_t, message_length) + sizeof(size_t))

typedef void (M_MDY_M_LOG_CALL *mmlog_sink_fn)(void *user_data, int32_t level,
                                              const char *line, size_t line_length);

M_MDY_M_LOG_API uint32_t M_MDY_M_LOG_CALL mmlog_abi_version(void);
M_MDY_M_LOG_API const char *M_MDY_M_LOG_CALL mmlog_level_name(int32_t level);
M_MDY_M_LOG_API const char *M_MDY_M_LOG_CALL mmlog_status_name(mmlog_status_t status);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_config_init(mmlog_logger_config_t *config);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_fields_init(mmlog_fields_t *fields);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_event_init(mmlog_event_t *event);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_create(const mmlog_logger_config_t *config,
                                                  mmlog_logger_t **out_logger);
M_MDY_M_LOG_API void M_MDY_M_LOG_CALL mmlog_logger_destroy(mmlog_logger_t *logger);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_set_min_level(mmlog_logger_t *logger,
                                                         int32_t min_level);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_set_color_mode(mmlog_logger_t *logger,
                                                          mmlog_color_mode_t mode);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_set_quiet(mmlog_logger_t *logger, int quiet);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_add_file(mmlog_logger_t *logger,
                                                    const char *path,
                                                    int32_t min_level);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_add_callback(mmlog_logger_t *logger,
                                                        mmlog_sink_fn callback,
                                                        void *user_data,
                                                        int32_t min_level);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_flush(mmlog_logger_t *logger);
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logger_write(mmlog_logger_t *logger,
                                                 const mmlog_event_t *event);

/* C-only convenience helper. Foreign-language callers should use mmlog_logger_write(). */
M_MDY_M_LOG_API mmlog_status_t M_MDY_M_LOG_CALL mmlog_logf(mmlog_logger_t *logger, int32_t level,
                                        const mmlog_fields_t *fields,
                                        const char *file, uint32_t line,
                                        const char *function, const char *format, ...);

#define MMLLOG_TRACE(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_TRACE, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_DEBUG(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_DEBUG, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_INFO(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_INFO, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_WARN(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_WARN, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_ERROR(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_ERROR, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_FATAL(logger, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_FATAL, NULL, __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)

#define MMLLOG_CTX_TRACE(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_TRACE, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_CTX_DEBUG(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_DEBUG, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_CTX_INFO(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_INFO, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_CTX_WARN(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_WARN, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_CTX_ERROR(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_ERROR, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define MMLLOG_CTX_FATAL(logger, fields, ...) \
    mmlog_logf((logger), MMLLOG_LEVEL_FATAL, (fields), __FILE__, (uint32_t)__LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* M_MDY_M_LOG_LOG_H */
