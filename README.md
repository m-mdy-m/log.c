# m-mdy-m/log.c

`m-mdy-m/log.c` is a small C11 logging library. It writes `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, and `FATAL` messages to the console, a file, or an application callback. The core has no third-party runtime dependency and provides a stable C ABI for language bindings.

## Build

```sh
git clone https://github.com/m-mdy-m/log.c.git
cd log.c
make
make check
```

To install locally on a Unix-like system:

```sh
make install PREFIX="$HOME/.local"
```

See [Installation and build](docs/installation.md) for CMake, Windows, release packages, and linking options.

## Use

```c
#include <m-mdy-m/log.h>

int main(void) {
    mmlog_logger_config_t config;
    mmlog_logger_t *logger = NULL;

    if (mmlog_logger_config_init(&config) != MMLLOG_OK) return 1;
    if (mmlog_logger_create(&config, &logger) != MMLLOG_OK) return 1;

    MMLLOG_INFO(logger, "service started");

    mmlog_logger_destroy(logger);
    return 0;
}
```

Compile this example directly from the source tree:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude app.c src/log.c -o app
./app
```

For more examples, see [`examples/c/basic.c`](examples/c/basic.c).

## Documentation

- [Documentation index](docs/index.md)
- [User guide](docs/user-guide.md)
- [Installation and build](docs/installation.md)
- [C ABI reference](docs/abi.md)
- [Language bindings](docs/language-bindings.md)
- [Architecture](docs/architecture.md)
- [Developer guide](docs/development.md)
- [Release guide](docs/release.md)
- [Troubleshooting](docs/troubleshooting.md)

## Project policies

[Contributing](CONTRIBUTING.md) · [Code of Conduct](CODE_OF_CONDUCT.md) · [Security](SECURITY.md)

## License

MIT. See [LICENSE](LICENSE) and [NOTICE](NOTICE) for license and upstream attribution.
