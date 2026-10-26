local log = require("m_mdy_m_log")

local logger = log.new({ level = "trace", color = "auto", output = "stderr" })
logger:info("service started", { layer = "api", component = "bootstrap", status = "ok" })
logger:debug("worker configured", { component = "worker-pool" })
logger:warn("cache is warming", { component = "cache", duration_ns = 1250000 })
logger:log("error", "request failed", {
  layer = "http",
  component = "router",
  request_id = "req-7f3a",
  trace_id = "tr-01",
  status = "timeout",
  duration_ns = 250000000,
  file = "router.lua",
  line = 81,
  ["function"] = "route_request",
})
logger:add_file("service.log", "info")
logger:info("this line goes to console and service.log")
logger:close()
