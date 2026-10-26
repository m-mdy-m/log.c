/* POSIX-only stress test; production library itself has no pthread dependency. */
#include <m-mdy-m/log.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREAD_COUNT 8
#define RECORDS_PER_THREAD 1000

typedef struct test_state {
    atomic_uint calls;
    atomic_uint malformed_lines;
} test_state_t;

typedef struct worker_args {
    mmlog_logger_t *logger;
    unsigned worker_id;
} worker_args_t;

static void sink(void *user_data, int32_t level, const char *line, size_t length) {
    test_state_t *state = (test_state_t *)user_data;
    (void)level;
    if (memchr(line, '\n', length) != NULL || memchr(line, '\r', length) != NULL) {
        atomic_fetch_add_explicit(&state->malformed_lines, 1u, memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&state->calls, 1u, memory_order_relaxed);
}

static void *worker(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    unsigned i;
    for (i = 0; i < RECORDS_PER_THREAD; ++i) {
        mmlog_event_t event;
        char message[80];
        int n = snprintf(message, sizeof(message), "worker=%u record=%u", args->worker_id, i);
        if (n < 0 || (size_t)n >= sizeof(message)) return (void *)1;
        (void)mmlog_event_init(&event);
        event.level = MMLLOG_LEVEL_INFO;
        event.layer = "concurrency-test";
        event.message = message;
        event.message_length = (size_t)n;
        if (mmlog_logger_write(args->logger, &event) != MMLLOG_OK) return (void *)1;
    }
    return NULL;
}

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    test_state_t state;
    pthread_t threads[THREAD_COUNT];
    worker_args_t args[THREAD_COUNT];
    unsigned i;
    atomic_init(&state.calls, 0u);
    atomic_init(&state.malformed_lines, 0u);
    if (mmlog_logger_config_init(&config) != MMLLOG_OK) return 1;
    config.output = MMLLOG_OUTPUT_NONE;
    if (mmlog_logger_create(&config, &logger) != MMLLOG_OK) return 1;
    if (mmlog_logger_add_callback(logger, sink, &state, MMLLOG_LEVEL_TRACE) != MMLLOG_OK) return 1;
    for (i = 0; i < THREAD_COUNT; ++i) {
        args[i].logger = logger;
        args[i].worker_id = i;
        if (pthread_create(&threads[i], NULL, worker, &args[i]) != 0) return 2;
    }
    for (i = 0; i < THREAD_COUNT; ++i) {
        void *result = NULL;
        if (pthread_join(threads[i], &result) != 0 || result != NULL) return 3;
    }
    mmlog_logger_destroy(logger);
    if (atomic_load_explicit(&state.calls, memory_order_relaxed) != THREAD_COUNT * RECORDS_PER_THREAD) {
        fprintf(stderr, "threaded test: wrong number of callback calls: %u\n",
                atomic_load_explicit(&state.calls, memory_order_relaxed));
        return 4;
    }
    if (atomic_load_explicit(&state.malformed_lines, memory_order_relaxed) != 0u) {
        fprintf(stderr, "threaded test: callback received a raw line break\n");
        return 5;
    }
    puts("m-mdy-m-log threaded test: PASS (8000 records)");
    return 0;
}
