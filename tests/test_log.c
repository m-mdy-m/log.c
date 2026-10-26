#include <m-mdy-m/log.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Contract checks must run in Release builds too; standard assert may compile away under NDEBUG. */
#undef assert
#define assert(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct capture_state {
    unsigned calls;
    int32_t last_level;
    char last_line[8192];
    size_t last_length;
    mmlog_logger_t *reenter_logger;
    int reenter_once;
} capture_state_t;

static void capture(void *user_data, int32_t level, const char *line, size_t length) {
    capture_state_t *s = (capture_state_t *)user_data;
    size_t n = length < sizeof(s->last_line) - 1u ? length : sizeof(s->last_line) - 1u;
    ++s->calls;
    s->last_level = level;
    s->last_length = length;
    memcpy(s->last_line, line, n);
    s->last_line[n] = '\0';
    if (s->reenter_logger && s->reenter_once) {
        mmlog_event_t event;
        s->reenter_once = 0;
        (void)mmlog_event_init(&event);
        event.level = MMLLOG_LEVEL_DEBUG;
        event.message = "nested";
        event.message_length = 6;
        assert(mmlog_logger_write(s->reenter_logger, &event) == MMLLOG_OK);
    }
}

static mmlog_logger_t *new_test_logger(int32_t min_level, capture_state_t *capture_state) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    assert(mmlog_logger_config_init(&config) == MMLLOG_OK);
    config.output = MMLLOG_OUTPUT_NONE;
    config.color_mode = MMLLOG_COLOR_NEVER;
    config.min_level = (uint32_t)min_level;
    assert(mmlog_logger_create(&config, &logger) == MMLLOG_OK);
    assert(logger != NULL);
    assert(mmlog_logger_add_callback(logger, capture, capture_state, MMLLOG_LEVEL_TRACE) == MMLLOG_OK);
    return logger;
}

static void test_abi_and_names(void) {
    assert((mmlog_abi_version() >> 16u) == MMLLOG_ABI_MAJOR);
    assert(strcmp(mmlog_level_name(MMLLOG_LEVEL_TRACE), "TRACE") == 0);
    assert(strcmp(mmlog_level_name(MMLLOG_LEVEL_FATAL), "FATAL") == 0);
    assert(strcmp(mmlog_level_name(99), "UNKNOWN") == 0);
    assert(strcmp(mmlog_status_name(MMLLOG_EINVAL), "invalid argument") == 0);
}

static void test_context_and_escape(void) {
    capture_state_t state = {0};
    mmlog_logger_t *logger = new_test_logger(MMLLOG_LEVEL_TRACE, &state);
    mmlog_event_t event;
    const char message[] = "hello\n\x1b[31mworld\t!";
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO;
    event.timestamp_unix_ns = UINT64_C(1793011200123000000);
    event.layer = "edge";
    event.component = "http";
    event.file = "src/server.c";
    event.line = 42;
    event.function = "handle_request";
    event.request_id = "req-42";
    event.trace_id = "trace-9";
    event.duration_ns = UINT64_C(2341000);
    event.status = "ok";
    event.message = message;
    event.message_length = sizeof(message) - 1u;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(state.calls == 1u && state.last_level == MMLLOG_LEVEL_INFO);
    assert(strstr(state.last_line, "INFO") != NULL);
    assert(strstr(state.last_line, "layer=edge") != NULL);
    assert(strstr(state.last_line, "component=http") != NULL);
    assert(strstr(state.last_line, "src/server.c:42") != NULL);
    assert(strstr(state.last_line, "function=handle_request") != NULL);
    assert(strstr(state.last_line, "request=req-42") != NULL);
    assert(strstr(state.last_line, "trace=trace-9") != NULL);
    assert(strstr(state.last_line, "duration=2.341ms") != NULL);
    assert(strstr(state.last_line, "status=ok") != NULL);
    assert(strstr(state.last_line, "hello\\n\\x1B[31mworld\\t!") != NULL);
    assert(strchr(state.last_line, '\n') == NULL);
    assert(strchr(state.last_line, '\x1b') == NULL);
    mmlog_logger_destroy(logger);
}

static void test_filter_and_quiet(void) {
    capture_state_t state = {0};
    mmlog_logger_t *logger = new_test_logger(MMLLOG_LEVEL_ERROR, &state);
    mmlog_event_t event;
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO; event.message = "filtered"; event.message_length = 8;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(state.calls == 0u);
    event.level = MMLLOG_LEVEL_ERROR; event.message = "written"; event.message_length = 7;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(state.calls == 1u);
    assert(mmlog_logger_set_min_level(logger, MMLLOG_LEVEL_FATAL) == MMLLOG_OK);
    assert(mmlog_logger_set_quiet(logger, 1) == MMLLOG_OK);
    event.level = MMLLOG_LEVEL_FATAL;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(state.calls == 2u); /* Quiet suppresses only the console; callback still works. */
    mmlog_logger_destroy(logger);
}

static void test_callback_can_reenter(void) {
    capture_state_t state = {0};
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_event_t event;
    assert(mmlog_logger_config_init(&config) == MMLLOG_OK);
    config.output = MMLLOG_OUTPUT_NONE;
    assert(mmlog_logger_create(&config, &logger) == MMLLOG_OK);
    state.reenter_logger = logger;
    state.reenter_once = 1;
    assert(mmlog_logger_add_callback(logger, capture, &state, MMLLOG_LEVEL_TRACE) == MMLLOG_OK);
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO; event.message = "outer"; event.message_length = 5;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(state.calls == 2u);
    mmlog_logger_destroy(logger);
}

static void test_invalid_inputs(void) {
    mmlog_logger_t *logger = NULL;
    mmlog_logger_config_t config;
    mmlog_event_t event;
    assert(mmlog_logger_config_init(&config) == MMLLOG_OK);
    config.output = MMLLOG_OUTPUT_NONE;
    assert(mmlog_logger_create(&config, &logger) == MMLLOG_OK);
    (void)mmlog_event_init(&event);
    event.level = 999;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_EINVAL);
    event.level = MMLLOG_LEVEL_INFO;
    event.struct_size = 1;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_EINVAL);
    assert(mmlog_logger_set_min_level(logger, 77) == MMLLOG_EINVAL);
    assert(mmlog_logger_set_color_mode(logger, (mmlog_color_mode_t)44) == MMLLOG_EINVAL);
    mmlog_logger_destroy(logger);
}

static void test_file_sink(void) {
    const char *path = "m-mdy-m-log-test-output.tmp";
    capture_state_t state = {0};
    mmlog_logger_t *logger = new_test_logger(MMLLOG_LEVEL_TRACE, &state);
    mmlog_event_t event;
    FILE *file;
    char contents[2048];
    size_t n;
    remove(path);
    assert(mmlog_logger_add_file(logger, path, MMLLOG_LEVEL_WARN) == MMLLOG_OK);
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO; event.message = "not-in-file"; event.message_length = 11;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    event.level = MMLLOG_LEVEL_WARN; event.message = "saved"; event.message_length = 5;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(mmlog_logger_flush(logger) == MMLLOG_OK);
    mmlog_logger_destroy(logger);
    file = fopen(path, "rb"); assert(file != NULL);
    n = fread(contents, 1u, sizeof(contents) - 1u, file); contents[n] = '\0'; fclose(file);
    assert(strstr(contents, "saved") != NULL);
    assert(strstr(contents, "not-in-file") == NULL);
    remove(path);
}

static void test_message_limit_and_sink_limit(void) {
    capture_state_t state = {0};
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_event_t event;
    unsigned i;
    assert(mmlog_logger_config_init(&config) == MMLLOG_OK);
    config.output = MMLLOG_OUTPUT_NONE;
    config.max_message_bytes = 8;
    assert(mmlog_logger_create(&config, &logger) == MMLLOG_OK);
    assert(mmlog_logger_add_callback(logger, capture, &state, MMLLOG_LEVEL_TRACE) == MMLLOG_OK);
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO;
    event.message = "0123456789";
    event.message_length = 10;
    assert(mmlog_logger_write(logger, &event) == MMLLOG_OK);
    assert(strstr(state.last_line, "01234567...") != NULL);
    mmlog_logger_destroy(logger);

    assert(mmlog_logger_config_init(&config) == MMLLOG_OK);
    config.output = MMLLOG_OUTPUT_NONE;
    assert(mmlog_logger_create(&config, &logger) == MMLLOG_OK);
    for (i = 0; i < 16u; ++i) {
        assert(mmlog_logger_add_callback(logger, capture, &state, MMLLOG_LEVEL_TRACE) == MMLLOG_OK);
    }
    assert(mmlog_logger_add_callback(logger, capture, &state, MMLLOG_LEVEL_TRACE) == MMLLOG_EFULL);
    mmlog_logger_destroy(logger);
}

int main(void) {
    test_abi_and_names();
    test_context_and_escape();
    test_filter_and_quiet();
    test_callback_can_reenter();
    test_invalid_inputs();
    test_file_sink();
    test_message_limit_and_sink_limit();
    puts("m-mdy-m-log tests: PASS");
    return 0;
}
