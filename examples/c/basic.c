#include <m-mdy-m/log.h>
#include <stdio.h>

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    mmlog_fields_t fields;
    mmlog_status_t status;
    if (mmlog_logger_config_init(&config) != MMLLOG_OK) return 1;
    config.color_mode = MMLLOG_COLOR_AUTO;
    config.min_level = MMLLOG_LEVEL_TRACE;
    status = mmlog_logger_create(&config, &logger);
    if (status != MMLLOG_OK) {
        fprintf(stderr, "logger create: %s\n", mmlog_status_name(status));
        return 1;
    }
    MMLLOG_INFO(logger, "service %s started on port %d", "api", 8080);
    MMLLOG_DEBUG(logger, "worker count: %d", 4);
    MMLLOG_WARN(logger, "cache hit rate is %.1f%%", 72.5);
    (void)mmlog_fields_init(&fields);
    fields.layer = "http";
    fields.component = "request-handler";
    fields.request_id = "req-7f3a";
    fields.trace_id = "tr-01";
    fields.duration_ns = UINT64_C(12000000);
    fields.status = "ok";
    MMLLOG_CTX_INFO(logger, &fields, "request completed: %s", "/health");
    (void)mmlog_logger_flush(logger);
    mmlog_logger_destroy(logger);
    return 0;
}
