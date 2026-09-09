# Azure uAMQP PHP

PHP binding for Azure uAMQP C (AMQP 1.0) used for Azure Service Bus and other AMQP 1.0-compatible systems.

This directory now uses an automated setup flow driven by `setup.sh`.

## Quick start

Use the installation guide for the full setup details:

- [`INSTALL.md`](./docs/INSTALL.md)

Run the installer from this directory:

```bash
sudo bash setup.sh
```

## What `setup.sh` does

The script automates the complete build and install process:

1. Detects the extension source directory and build location.
2. Reads optional environment variables for PHP and PHP-CPP versions.
3. Creates a temporary `libs-build` directory.
4. Installs required Debian packages.
5. Builds and installs:
   - Azure C Shared Utility
   - Azure uAMQP C
   - PHP-CPP
6. Builds and installs the PHP extension.
7. Enables and verifies the extension in PHP.
8. Cleans up temporary build artifacts.

## Configuration

The supported environment variables and their defaults are documented in [`INSTALL.md`](./docs/INSTALL.md).

## Receiving safely

`Connection::setCallback($resource, $onMessage, $loop, $maxLinkCredit)` accepts an optional positive
fourth argument. With it, one registration receives at most that many messages; the native
receiver does not replenish the batch. Omitting it keeps the three-argument continuous mode,
which requests another message after settling the previous one.

Return `false` from the message callback to complete the current message and stop. Returning
from the loop or calling `close()` also stops receiving. Already-authorized surplus messages
are released, and shutdown waits up to two seconds for receiver drain plus one second for
the peer's detach response. A failed drain or detach raises an error instead of reporting a
successful close. The native objects remain alive until callbacks finish.

A callback exception abandons its delivery for retry and reaches the PHP caller after cleanup.
Successful callbacks complete deliveries immediately; if processing happens later, use a
durable handoff before returning or make that processing idempotent. This API does not defer
completion to a later application acknowledgement.

Messages retain their bodies and metadata after callbacks. Use `getMessageId()`,
`getDeliveryCount()`, `getApplicationProperties()`, and `getMessageAnnotations()` for correlation.
Individual property/annotation getters accept an optional legacy type argument; without it,
PHP scalar types are preserved. Unsigned values exceeding PHP's integer range are decimal
strings. `setMessageId(string)` sets the broker message ID.

With connection debug enabled, `UAMQP_DEBUG_FILE` receives protocol frames and structured
receiver events including timestamps, connection identity, delivery IDs, broker metadata,
stop reasons, and attempted disposition names. A logged disposition attempt is not a broker
confirmation. Event records do not include message bodies.

Version 0.2.5 requires the patched uAMQP library built by `setup.sh`; copying only the extension
onto an older unpatched library is insufficient. The dependency patch and its application
helper are maintained under `patches/` and `scripts/apply-uamqp-patches.sh`. Header dependencies
are tracked by `make`, so changes to native class layouts trigger the necessary rebuilds.

After building in the Docker development environment, run `make test test-credit`,
`php tests/message-metadata-test.php`, and `bash scripts/run-e2e.sh`. The broker regression uses
only the local emulator and verifies repeated batches of 1, 2, and 10, early stops, exceptions,
message identity, and an empty dead-letter queue.

## Releasing

Edit [VERSION](VERSION) to change the extension and Debian package version together. See the
[release guide](docs/RELEASING.md) for verification and tagging steps.

## Notes

- Run the script with `sudo` or as `root` because it installs packages and writes to system directories.
- The script is intended for Debian-based environments.
- The old manual build instructions have been replaced by the automated setup script.
