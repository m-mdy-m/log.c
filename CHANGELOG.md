# Changelog

All notable changes to `m-mdy-m/log.c` are documented in this file.

The project follows [Semantic Versioning](https://semver.org/) for release
versions and [Keep a Changelog](https://keepachangelog.com/) for this file.
The package version and the public C ABI version are separate: this release is
package version `0.1.0`, while the current ABI reports major `1`, minor `0`.

## [0.1.0] - 2026-10-26

This is the first release of the redesigned, standalone `m-mdy-m/log.c`
library, based on the previous `rxi/log.c` project.

### Added

- A small C11 logging library with independently configured logger instances.
- Six log levels: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, and `FATAL`.
- Configurable console output, append-file sinks, callback sinks, per-sink level
  filters, quiet mode, flushing, and `AUTO` / `ALWAYS` / `NEVER` color modes.
- Structured event fields for UTC timestamps, layer, component, source location,
  function, request ID, trace ID, duration, and status.
- A version-queryable C ABI with opaque logger handles, fixed-width status and
  level values, and a non-variadic `mmlog_logger_write()` entry point for FFI.
- C convenience macros and `mmlog_logf()` for formatted logging.
- Bounded message handling and escaping of control characters so one record
  cannot inject extra terminal lines through an untrusted message.
- Public header at `include/m-mdy-m/log.h`, available to consumers as
  `#include <m-mdy-m/log.h>`.
- Make and CMake builds, static and shared libraries, installation and
  uninstallation targets, CMake package metadata, and `pkg-config` metadata.
- C unit tests, concurrent-write tests, ABI export checks, documentation checks,
  and end-to-end tests for installed static/shared consumers.
- A Lua 5.4 adapter example and FFI guidance for Python, Rust, Go, C#, Zig, and
  other runtimes that can call a C ABI.
- GitHub Actions workflows for supported platform builds, release asset creation,
  checksums, and optional multi-architecture Docker Hub SDK publication.
- Beginner-first documentation, release instructions, `SECURITY.md`,
  `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `CODEOWNERS`, and `.editorconfig`.
- `NOTICE` attribution documenting the use of `rxi/log.c` as the starting point.

### Changed

- Replaced the original global-state `log_*` API with an instance-based API
  under the `mmlog_*` / `MMLLOG_*` names.
- Made the installable header path and distribution metadata explicit for use as
  a standalone library instead of requiring consumers to copy source files into
  their own projects.
- Reorganized the documentation around first-time users, installation, the
  user guide, ABI, language bindings, architecture, development, and releases.

### Compatibility notes

- This is a redesigned API and is **not source-compatible** with the original
  `rxi/log.c` functions such as `log_info()` and `log_set_level()`. Existing
  callers must migrate to a logger instance and the `MMLLOG_*` macros/functions.
- The stable foreign-function entry point is `mmlog_logger_write()`. FFI
  callers must not call the variadic C convenience function `mmlog_logf()`.
- Release and Docker workflows are configured in the repository; assets and
  images are not considered published until their respective workflows run
  successfully with the required credentials.
