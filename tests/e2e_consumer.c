#include <m-mdy-m/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned callback_count;
static char captured[8192];

static void capture_line(void *user_data, int32_t level, const char *line, size_t length) {
    size_t copy_length = length < sizeof(captured) - 1u ? length : sizeof(captured) - 1u;
    (void)user_data;
    (void)level;
    memcpy(captured, line, copy_length);
    captured[copy_length] = '\0';
    ++callback_count;
}

static int fail(const char *message) {
    fprintf(stderr, "installed-consumer E2E: %s\n", message);
    return EXIT_FAILURE;
}

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_event_t event;
    mmlog_status_t status;

    if (mmlog_abi_version() != MMLLOG_ABI_VERSION) {
        return fail("unexpected C ABI version");
    }
    if (mmlog_logger_config_init(&config) != MMLLOG_OK) {
        return fail("logger config initialization failed");
    }
    config.output = MMLLOG_OUTPUT_NONE;
    config.color_mode = MMLLOG_COLOR_NEVER;
    status = mmlog_logger_create(&config, &logger);
    if (status != MMLLOG_OK || logger == NULL) {
        return fail("logger creation failed");
    }
    if (mmlog_logger_add_callback(logger, capture_line, NULL, MMLLOG_LEVEL_TRACE) != MMLLOG_OK) {
        mmlog_logger_destroy(logger);
        return fail("callback sink registration failed");
    }

    MMLLOG_INFO(logger, "m-mdy-m-log-e2e macro path works");
    if (callback_count != 1u || strstr(captured, "m-mdy-m-log-e2e macro path works") == NULL) {
        mmlog_logger_destroy(logger);
        return fail("installed header or printf-style C API did not produce expected output");
    }

    if (mmlog_event_init(&event) != MMLLOG_OK) {
        mmlog_logger_destroy(logger);
        return fail("event initialization failed");
    }
    event.level = MMLLOG_LEVEL_WARN;
    event.layer = "e2e";
    event.component = "installed-consumer";
    event.request_id = "release-test-42";
    event.message = "m-mdy-m-log-e2e stable ABI path works";
    event.message_length = strlen(event.message);
    status = mmlog_logger_write(logger, &event);
    mmlog_logger_destroy(logger);
    if (status != MMLLOG_OK || callback_count != 2u ||
        strstr(captured, "m-mdy-m-log-e2e stable ABI path works") == NULL ||
        strstr(captured, "release-test-42") == NULL) {
        return fail("installed stable-record ABI path did not produce expected output");
    }

    puts("installed-consumer E2E: PASS");
    return EXIT_SUCCESS;
}
