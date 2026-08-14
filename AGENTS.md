# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project overview

`azure-uamqp-php` is a native **PHP extension** (`uamqpphpbinding`, module name `uamqp`)
written in C++ that binds to the [Azure uAMQP C](https://github.com/Azure/azure-uamqp-c)
library (AMQP 1.0) via [PHP-CPP](https://github.com/CopernicaMarketingSoftware/PHP-CPP).
It lets PHP applications publish and consume messages from Azure Service Bus (or other
AMQP 1.0 brokers).

PHP-visible classes: `Azure\uAMQP\Connection` and `Azure\uAMQP\Message`.

## Source layout

- `main.cpp` — extension entry point (`get_module`); registers PHP classes/methods here.
- `Connection.{h,cpp}` — AMQP connection lifecycle (connect, publish, consume, close), wraps
  `CONNECTION_HANDLE`, TLS/SASL/socket IO setup.
- `Session.{h,cpp}`, `Consumer.{h,cpp}`, `Producer.{h,cpp}` — session/link management, message
  consumption and publishing.
- `Message.{h,cpp}` — PHP-facing AMQP message wrapper (body, application properties, message
  annotations).
- `examples/` — sample PHP scripts (`connection.php`, `producer.php`, `consumer.php`,
  `parameters.php.dist` — copy to `parameters.php` and fill in real credentials, never commit it).
- `scripts/` — e2e/dev helper scripts (`run-e2e.sh`, `test-producer.php`, `test-consumer.php`,
  `emulator-parameters.php`, stub generation).
- `docker/`, `Dockerfile`, `docker-compose.yml` — local dev/e2e environment using the Azure
  Service Bus emulator + MSSQL.
- `packaging/` — `build-deb.sh` and `dependencies.txt` for Debian package builds.
- `docs/INSTALL.md` — detailed `setup.sh` documentation.
- `.github/workflows/` — `build-deb.yml` (tag-triggered .deb build/release) and `e2e.yml`
  (Docker Compose based end-to-end test).

## Build

Two build paths exist; **`setup.sh` is the primary/authoritative one** used in CI:

- `sudo bash setup.sh` — full automated flow: installs apt deps, builds/installs Azure C Shared
  Utility, Azure uAMQP C, and PHP-CPP from source, then builds and installs this extension and
  enables it in PHP. Supports env vars `PHUAMQP_PHP_MAJOR_VERSION` (default `8.3`),
  `PHUAMQP_PHP_CPP_VERSION` (default `2.4.1`), `PHUAMQP_LIBS_BUILD_DIR`. See `docs/INSTALL.md`.
- `make` — compiles the extension directly assuming Azure C Shared Utility, Azure uAMQP C, and
  PHP-CPP are already installed under `/usr/local`. `make install` copies the `.so` into the PHP
  extension dir. `make clean` removes build artifacts.
- `CMakeLists.txt` exists but only builds a standalone `php_uamqp` executable from the sources;
  it is not the extension build path.

There is no separate configure/build step for tests beyond the e2e flow — this is a compiled
extension, not a typical app.

## Testing / verification

- `quick-check.sh` — quick sanity check (`php --ri uamqpphpbinding`) after install.
- End-to-end test: `scripts/run-e2e.sh`, driven via Docker Compose
  (`docker compose build phuamqp && docker compose up -d phuamqp-servicebus-emulator phuamqp-mssql`,
  then run the script inside the `phuamqp` container). It waits for the Service Bus emulator
  health endpoint, starts a consumer (`scripts/test-consumer.php`) then a producer
  (`scripts/test-producer.php`), and asserts the consumer receives the produced messages.
- CI (`.github/workflows/e2e.yml`) runs this exact flow on every push/PR.
- There is no unit test suite for the C++ code; validate changes by rebuilding the extension
  and running the e2e flow (or `quick-check.sh` for a minimal smoke test).

## Conventions

- C++14, extension code uses PHP-CPP types (`Php::Parameters`, `Php::Value`, `Php::ByVal`, etc.).
  Method signatures/parameter types are declared centrally in `main.cpp`'s `get_module()` — when
  adding/changing a PHP-exposed method, update both the class implementation and this
  registration.
- Class headers guard raw uAMQP C handles (`CONNECTION_HANDLE`, `XIO_HANDLE`, etc.) behind the
  PHP-CPP `Php::Base` wrapper classes (`Connection`, `Session`, `Consumer`, `Message`).
  Forward-declare (`class Session;`) rather than including headers when only a pointer is needed,
  to keep compile times/header coupling down.
- Environment variables for the docker/e2e flow are prefixed `PHUAMQP_`; the underlying
  AMQP/service bus connection vars in the Docker image are prefixed `AZSB_AMQP_`. Keep this
  distinction when adding new configuration.
- Never commit real Service Bus credentials — `examples/parameters.php.dist` is the template;
  actual `parameters.php` files with secrets must stay untracked (see `.gitignore`).
- Required headers for the build come from `/usr/local/include/azureiot`,
  `/usr/local/include/c_logging/v2`, `/usr/local/include/azure_c_shared_utility`,
  `/usr/local/include/macro_utils`, `/usr/local/include/umock_c` — if introducing new build
  logic, mirror the include/link setup already present in `Makefile` and `setup.sh` rather than
  inventing a new one.

## Packaging & release

- Tags matching `v*` trigger `.github/workflows/build-deb.yml`, which runs `setup.sh` then
  `packaging/build-deb.sh` to produce and upload a `.deb`, then attaches it to the GitHub release.
- `packaging/dependencies.txt` documents the shared library dependency tree for the built
  extension; update it if you change linked libraries.
