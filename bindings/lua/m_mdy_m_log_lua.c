#include <m-mdy-m/log.h>
#include <lua.h>
#include <lauxlib.h>
#include <string.h>

typedef struct lua_logger { mmlog_logger_t *logger; } lua_logger_t;
#define LUA_LOGGER_MT "m_mdy_m_log.logger"

static const char *const level_keys[] = {"trace", "debug", "info", "warn", "error", "fatal", NULL};
static int parse_level(lua_State *L, int index) {
    int i;
    if (lua_isinteger(L, index)) {
        lua_Integer v = lua_tointeger(L, index);
        luaL_argcheck(L, v >= MMLLOG_LEVEL_TRACE && v <= MMLLOG_LEVEL_FATAL, index, "invalid log level");
        return (int)v;
    }
    {
        const char *s = luaL_checkstring(L, index);
        for (i = 0; level_keys[i]; ++i) if (strcmp(s, level_keys[i]) == 0) return i;
        luaL_argerror(L, index, "level must be trace/debug/info/warn/error/fatal or 0..5");
    }
    return MMLLOG_LEVEL_INFO;
}
static lua_logger_t *check_logger(lua_State *L) {
    lua_logger_t *u = (lua_logger_t *)luaL_checkudata(L, 1, LUA_LOGGER_MT);
    luaL_argcheck(L, u->logger != NULL, 1, "logger is closed");
    return u;
}
static void optional_string_field(lua_State *L, int table, const char *key, const char **out) {
    lua_getfield(L, table, key);
    if (!lua_isnil(L, -1)) *out = luaL_checkstring(L, -1);
    lua_pop(L, 1);
}
static uint64_t optional_u64_field(lua_State *L, int table, const char *key) {
    uint64_t value = 0;
    lua_getfield(L, table, key);
    if (!lua_isnil(L, -1)) {
        lua_Integer n = luaL_checkinteger(L, -1);
        luaL_argcheck(L, n >= 0, table, "field must be a non-negative integer");
        value = (uint64_t)n;
    }
    lua_pop(L, 1);
    return value;
}
static int logger_log_level(lua_State *L, lua_logger_t *u, int level, int message_index, int fields_index) {
    mmlog_event_t event;
    size_t message_length = 0;
    const char *message = luaL_checklstring(L, message_index, &message_length);
    (void)mmlog_event_init(&event);
    event.level = level;
    event.message = message;
    event.message_length = message_length;
    if (lua_istable(L, fields_index)) {
        optional_string_field(L, fields_index, "layer", &event.layer);
        optional_string_field(L, fields_index, "component", &event.component);
        optional_string_field(L, fields_index, "file", &event.file);
        optional_string_field(L, fields_index, "function", &event.function);
        optional_string_field(L, fields_index, "request_id", &event.request_id);
        optional_string_field(L, fields_index, "trace_id", &event.trace_id);
        optional_string_field(L, fields_index, "status", &event.status);
        event.duration_ns = optional_u64_field(L, fields_index, "duration_ns");
        event.timestamp_unix_ns = optional_u64_field(L, fields_index, "timestamp_unix_ns");
        lua_getfield(L, fields_index, "line");
        if (!lua_isnil(L, -1)) {
            lua_Integer line = luaL_checkinteger(L, -1);
            luaL_argcheck(L, line >= 0 && (uint64_t)line <= UINT32_MAX, fields_index, "line out of range");
            event.line = (uint32_t)line;
        }
        lua_pop(L, 1);
    }
    {
        mmlog_status_t rc = mmlog_logger_write(u->logger, &event);
        if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log: %s", mmlog_status_name(rc));
    }
    return 0;
}
static int l_new(lua_State *L) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;
    lua_logger_t *u;
    (void)mmlog_logger_config_init(&config);
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "level");
        if (!lua_isnil(L, -1)) config.min_level = (uint32_t)parse_level(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "color");
        if (!lua_isnil(L, -1)) {
            const char *color = luaL_checkstring(L, -1);
            if (strcmp(color, "auto") == 0) config.color_mode = MMLLOG_COLOR_AUTO;
            else if (strcmp(color, "always") == 0) config.color_mode = MMLLOG_COLOR_ALWAYS;
            else if (strcmp(color, "never") == 0) config.color_mode = MMLLOG_COLOR_NEVER;
            else return luaL_error(L, "color must be auto, always or never");
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "output");
        if (!lua_isnil(L, -1)) {
            const char *out = luaL_checkstring(L, -1);
            if (strcmp(out, "stderr") == 0) config.output = MMLLOG_OUTPUT_STDERR;
            else if (strcmp(out, "stdout") == 0) config.output = MMLLOG_OUTPUT_STDOUT;
            else if (strcmp(out, "none") == 0) config.output = MMLLOG_OUTPUT_NONE;
            else return luaL_error(L, "output must be stderr, stdout or none");
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "max_message_bytes");
        if (!lua_isnil(L, -1)) {
            lua_Integer n = luaL_checkinteger(L, -1);
            luaL_argcheck(L, n >= 1 && n <= MMLLOG_MAX_MESSAGE_BYTES, 1, "max_message_bytes out of range");
            config.max_message_bytes = (uint32_t)n;
        }
        lua_pop(L, 1);
    }
    {
        mmlog_status_t rc = mmlog_logger_create(&config, &logger);
        if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log create: %s", mmlog_status_name(rc));
    }
    u = (lua_logger_t *)lua_newuserdatauv(L, sizeof(*u), 0);
    u->logger = logger;
    luaL_getmetatable(L, LUA_LOGGER_MT);
    lua_setmetatable(L, -2);
    return 1;
}
static int l_gc(lua_State *L) {
    lua_logger_t *u = (lua_logger_t *)luaL_checkudata(L, 1, LUA_LOGGER_MT);
    if (u->logger) { mmlog_logger_destroy(u->logger); u->logger = NULL; }
    return 0;
}
static int l_close(lua_State *L) { return l_gc(L); }
static int l_log(lua_State *L) {
    lua_logger_t *u = check_logger(L);
    return logger_log_level(L, u, parse_level(L, 2), 3, 4);
}
#define DEFINE_LEVEL_FN(name, level) \
    static int l_##name(lua_State *L) { \
        lua_logger_t *u = check_logger(L); \
        return logger_log_level(L, u, level, 2, 3); \
    }
DEFINE_LEVEL_FN(trace, MMLLOG_LEVEL_TRACE)
DEFINE_LEVEL_FN(debug, MMLLOG_LEVEL_DEBUG)
DEFINE_LEVEL_FN(info, MMLLOG_LEVEL_INFO)
DEFINE_LEVEL_FN(warn, MMLLOG_LEVEL_WARN)
DEFINE_LEVEL_FN(error, MMLLOG_LEVEL_ERROR)
DEFINE_LEVEL_FN(fatal, MMLLOG_LEVEL_FATAL)
static int l_set_level(lua_State *L) {
    lua_logger_t *u = check_logger(L);
    mmlog_status_t rc = mmlog_logger_set_min_level(u->logger, parse_level(L, 2));
    if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log: %s", mmlog_status_name(rc));
    return 0;
}
static int l_set_color(lua_State *L) {
    lua_logger_t *u = check_logger(L);
    const char *s = luaL_checkstring(L, 2);
    mmlog_color_mode_t mode;
    if (strcmp(s, "auto") == 0) mode = MMLLOG_COLOR_AUTO;
    else if (strcmp(s, "always") == 0) mode = MMLLOG_COLOR_ALWAYS;
    else if (strcmp(s, "never") == 0) mode = MMLLOG_COLOR_NEVER;
    else return luaL_argerror(L, 2, "expected auto, always or never");
    {
        mmlog_status_t rc = mmlog_logger_set_color_mode(u->logger, mode);
        if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log: %s", mmlog_status_name(rc));
    }
    return 0;
}
static int l_set_quiet(lua_State *L) {
    lua_logger_t *u = check_logger(L);
    int quiet = lua_toboolean(L, 2);
    mmlog_status_t rc = mmlog_logger_set_quiet(u->logger, quiet);
    if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log: %s", mmlog_status_name(rc));
    return 0;
}
static int l_add_file(lua_State *L) {
    lua_logger_t *u = check_logger(L);
    const char *path = luaL_checkstring(L, 2);
    int level = lua_isnoneornil(L, 3) ? MMLLOG_LEVEL_TRACE : parse_level(L, 3);
    mmlog_status_t rc = mmlog_logger_add_file(u->logger, path, level);
    if (rc != MMLLOG_OK) return luaL_error(L, "m-mdy-m-log add_file: %s", mmlog_status_name(rc));
    return 0;
}
static int l_level_name(lua_State *L) {
    int level = parse_level(L, 1);
    lua_pushstring(L, mmlog_level_name(level));
    return 1;
}

int luaopen_m_mdy_m_log(lua_State *L) {
    static const luaL_Reg methods[] = {
        {"log", l_log}, {"trace", l_trace}, {"debug", l_debug}, {"info", l_info},
        {"warn", l_warn}, {"error", l_error}, {"fatal", l_fatal},
        {"set_level", l_set_level}, {"set_color", l_set_color}, {"set_quiet", l_set_quiet},
        {"add_file", l_add_file}, {"close", l_close}, {NULL, NULL}
    };
    luaL_newmetatable(L, LUA_LOGGER_MT);
    lua_pushcfunction(L, l_gc); lua_setfield(L, -2, "__gc");
    lua_newtable(L); luaL_setfuncs(L, methods, 0); lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, l_new); lua_setfield(L, -2, "new");
    lua_pushcfunction(L, l_level_name); lua_setfield(L, -2, "level_name");
    lua_pushinteger(L, MMLLOG_LEVEL_TRACE); lua_setfield(L, -2, "TRACE");
    lua_pushinteger(L, MMLLOG_LEVEL_DEBUG); lua_setfield(L, -2, "DEBUG");
    lua_pushinteger(L, MMLLOG_LEVEL_INFO); lua_setfield(L, -2, "INFO");
    lua_pushinteger(L, MMLLOG_LEVEL_WARN); lua_setfield(L, -2, "WARN");
    lua_pushinteger(L, MMLLOG_LEVEL_ERROR); lua_setfield(L, -2, "ERROR");
    lua_pushinteger(L, MMLLOG_LEVEL_FATAL); lua_setfield(L, -2, "FATAL");
    lua_pushinteger(L, (lua_Integer)mmlog_abi_version()); lua_setfield(L, -2, "ABI_VERSION");
    return 1;
}
