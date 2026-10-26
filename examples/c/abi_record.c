/* This example uses only the stable record ABI (no C varargs). */
#include <m-mdy-m/log.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_event_t event;
    mmlog_status_t rc;
    (void)mmlog_logger_config_init(&config);
    rc = mmlog_logger_create(&config, &logger);
    if (rc != MMLLOG_OK) return 1;
    (void)mmlog_event_init(&event);
    event.level = MMLLOG_LEVEL_INFO;
    event.layer = "storage";
    event.component = "sqlite";
    event.file = "store.c";
    event.line = 91;
    event.function = "commit";
    event.request_id = "job-12";
    event.trace_id = "trace-12";
    event.duration_ns = UINT64_C(800000);
    event.status = "committed";
    event.message = "transaction committed";
    event.message_length = strlen(event.message);
    rc = mmlog_logger_write(logger, &event);
    mmlog_logger_destroy(logger);
    return rc == MMLLOG_OK ? 0 : 2;
}
