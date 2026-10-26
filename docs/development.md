# Developer guide

This guide is for people changing `m-mdy-m/log.c` itself. The [user guide](user-guide.md) defines expected behavior, the [ABI reference](abi.md) describes the public contract, and the [architecture](architecture.md) explains the current implementation. This guide covers the source layout, tests, and how to make a safe change.

## 1. Repository layout

| Path | What belongs there |
|---|---|
| `include/m-mdy-m/log.h` | The public C API and stable ABI. Treat it as the source of truth. |
| `src/log.c` | Core implementation: formatting, bounds, sinks, locks and public function bodies. |
| `tests/test_log.c` | Functional and contract regression tests. |
| `tests/test_threaded.c` | Multi-threaded writer test. |
| `tests/e2e_install.sh` | Compiles and runs an external consumer against an installed prefix. |
| `tests/e2e_windows_install.ps1` | Checks the actual Windows release ZIP and PowerShell installer. |
| `tests/e2e-cmake/` | A separate consumer that checks the installed CMake package. |
| `examples/c/` | C convenience API and stable record-ABI examples. |
| `bindings/lua/`, `examples/lua/` | Optional Lua 5.4 adapter and sample. |
| `cmake/`, `CMakeLists.txt` | Build rules and the installed CMake package. |
| `packaging/` | `pkg-config` template and release archive installers. |
| `tools/` | ABI symbol check, documentation link check, and packaging/E2E helpers. |
| `.github/workflows/` | Cross-platform CI, release packaging, and Docker image workflows. |
| `docs/` | User, ABI, architecture, build, binding, development and release documentation. |

## 2. The normal development loop

Run from the repository root:

```sh
make clean
make check
```

`make check` builds the static and shared libraries, runs the C tests, compiles the examples, checks the documentation's local links, inspects the shared library for required exported symbols, and performs an installed-consumer E2E run. It is the minimum local gate for a change.

CMake should also be checked before a release or after changing build/install rules:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build-cmake --config Release --parallel
ctest --test-dir build-cmake -C Release --output-on-failure
cmake --install build-cmake --prefix "$PWD/stage"
```

For a static CMake build, use a different build directory and `-DBUILD_SHARED_LIBS=OFF`. Do not reuse one configured build directory when changing between static and shared modes unless you intentionally clean/reconfigure it.

The extended packaging/install validation is:

```sh
bash tools/build_test_install.sh "$PWD/stage-e2e"
```

It builds separate static and shared CMake variants, checks CTest, installs each one, tests `find_package(m-mdy-m-log CONFIG REQUIRED)` from a separate project, produces a release archive, runs the archive's installer and builds/runs an external consumer against the installed files. Windows CI also runs `install.ps1` on the actual ZIP and validates the installed files.

## 3. Verification gates

| Gate | Command or workflow | What it checks |
|---|---|---|
| Compile and unit tests | `make check` | C11 build, functional behavior, invalid inputs, levels, metadata and sink behavior. |
| Concurrent writers | included in `make check` | Concurrent record writes; currently stress-tests 8,000 records. |
| ABI symbols | `make abi-check` after building shared | Required public exported names exist in the shared library. |
| Documentation links | `make docs-check` | Relative links in Markdown resolve in this checkout. |
| Installed C consumer | `make e2e` | Header, archive/shared library, link and runtime behavior work after installation. |
| CMake package | `tools/build_test_install.sh` | A separate consumer can import and link `m-mdy-m::log`. |
| Release archive | `make package` / release CI | Source and platform SDK archives contain the expected files. |
| Windows release installer | Windows release CI | PowerShell installer runs against the ZIP and required files are present. |
| Lua adapter | `make lua-check` | Lua 5.4 module builds and the sample runs; requires Lua dev files. |

Only report checks that were actually run for the change being reviewed. A workflow being defined is not evidence that it passed.

## 4. Rules for modifying the public ABI

The public ABI is used by prebuilt applications and foreign-language bindings. Changing it casually can break consumers even if the C source still compiles.

- Do not reorder or renumber existing severity or status values.
- Do not rename or remove existing exported functions in an ABI-compatible release.
- Do not change calling conventions, existing argument types or the order/types of existing struct fields.
- Keep FFI entry points non-variadic. Do not require non-C languages to call `mmlog_logf`.
- Initialize/validate `struct_size` consistently. If public structs must grow, update the size contract and ABI notes before implementing the change.
- Add tests for success, invalid arguments, failure behavior and cross-boundary ownership/lifetimes.
- Update `docs/abi.md`, `docs/user-guide.md`, `docs/language-bindings.md`, examples, and `CHANGELOG.md` together.
- Ensure the exported-symbol check and a compiled external consumer pass for shared and static builds.

Do not bump the major or minor version just to make a local build work. Record the compatibility reason in the changelog and release notes.

## 5. Source and style rules

- Keep the core small and synchronous unless a separate design establishes the cost and value of a larger feature.
- Avoid third-party runtime dependencies in `src/log.c`.
- Keep implementation-private state out of the public header.
- Do not expose a raw `FILE *` through the ABI; it can cross incompatible C runtimes on Windows.
- Do not execute commands or perform network I/O from the logging core.
- Add a deterministic regression test for every externally observable change.
- Never put real credentials or personal data in logs, examples, CI output or tests.
- Prefer readable checks with a clear failure message over tests whose assertions disappear in Release builds.

## 6. Documentation rules

The documentation is for a reader who may never have used this project before. Define unfamiliar terms, begin with the practical task, show commands that work from the repository root, and explain what the output means. Keep beginner workflows in `user-guide.md` and `installation.md`; keep exact binary contracts in `abi.md`; describe internal design in `architecture.md`. Avoid duplicating large reference tables across files—link to the source of truth instead.

When changing docs, run:

```sh
make docs-check
```

## 7. Pull request checklist

- [ ] The user-visible behavior and rationale are described.
- [ ] Tests cover the new behavior and important failure paths.
- [ ] `make clean check` passes locally.
- [ ] CMake/CTest and installed-package checks were run when relevant.
- [ ] Public ABI and versioning were reviewed.
- [ ] README, docs, examples and changelog agree.
- [ ] Claims about operating-system support match actual CI jobs.
- [ ] No release was published and no branch was pushed as part of local verification.
