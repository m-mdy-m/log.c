# Contributing

Thanks for considering a contribution to `m-mdy-m/log.c`. Bug reports, focused fixes, tests, and clearer documentation are welcome.

## Before you start

- For a bug, include the operating system, compiler, steps to reproduce, and the smallest useful example. Remove secrets and private data from logs.
- For a larger change or a public API change, open an issue first so the approach can be discussed.
- For a security issue, follow [SECURITY.md](SECURITY.md); do not report it in a public issue.
- Follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Build and test

You need a C11 compiler, Make, and Python 3 for the repository checks. From a clone of the repository, run:

```sh
git clone https://github.com/m-mdy-m/log.c.git
cd log.c
make clean check
```

CMake is also supported:

```sh
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel
ctest --test-dir build-cmake --output-on-failure
```

`make check` builds both library forms and examples, runs unit and concurrent-writer tests, checks exported ABI symbols and documentation links, and tests an installed consumer.

## Changes to the public API

- Treat `include/m-mdy-m/log.h` as the authoritative public interface.
- Add or update tests for changed behavior.
- Update the relevant documents under `docs/`, especially `abi.md` and `user-guide.md` when public behavior changes.
- Update `CHANGELOG.md` for user-visible changes.
- Keep the non-variadic `mmlog_logger_write()` entry point usable by foreign-function interfaces. Do not require other languages to call C variadic functions.
- Do not silently change existing ABI symbols, enum values, struct layouts, or ownership rules. Explain compatibility implications in the pull request.

## Pull requests

Keep each pull request focused. Describe the problem, the change, how it was tested, and any platform or ABI implications. State clearly which checks you ran and which you could not run. Make sure `make clean check` passes before requesting review, and do not include generated build directories or local artifacts.

By submitting a contribution, you agree that it may be distributed under the project's existing license, as stated in [LICENSE](LICENSE).
