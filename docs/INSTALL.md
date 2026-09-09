# Installation Guide for `setup.sh`

This directory contains `setup.sh`, a shell script that builds and installs the Azure uAMQP C library, its dependencies, and the PHP extension.

## What the script does

At a high level, `setup.sh`:

1. Detects the PHP and PHP-CPP versions to use.
2. Uses the extension source directory as the working root.
3. Creates a local `libs-build` directory for building the C dependencies.
4. Installs required Debian packages.
5. Builds and installs:
   - Azure C Shared Utility
   - Azure uAMQP C, with this extension's required receiver-credit patch
   - PHP-CPP
6. Builds the PHP extension.
7. Installs and enables the extension.
8. Verifies that the extension can be loaded by PHP.
9. Cleans up temporary build files.

## How to run it

Run the script from a shell with sufficient privileges, because it installs packages and writes to system directories.

```bash
sudo bash setup.sh
```

## Updating a running application

After installation, restart PHP-FPM and any long-running PHP workers using their service
manager. A graceful FPM reload that re-executes the master also loads the new libraries once
the old workers finish. Restarting only a child worker is insufficient: it inherits the
extension loaded by the existing master.

The setup check uses a fresh CLI process. Verify `phpversion('uamqpphpbinding')` through the
application's FPM pool as well; CLI and FPM can report different versions after an upgrade.
The script does not restart application services automatically.

## Environment variables

The script supports these environment variables:

### `PHP_UAMQP_BUILD_DIR`

Optional override for the extension source directory.

- **If set**: `UAMQP_EXT_DIR` is assigned the same value.
- **If not set**: `UAMQP_EXT_DIR` defaults to the directory where `setup.sh` is located.

### `PHUAMQP_PHP_MAJOR_VERSION`

Optional override for the PHP major version used for the build.

- **If set**: `PHP_MAJOR_VERSION` uses this value.
- **Default**: `8.3`

Example:

```bash
export PHUAMQP_PHP_MAJOR_VERSION=8.3
```

### `PHUAMQP_LIBS_BUILD_DIR`

Optional directory for dependency checkouts and builds; defaults to `libs-build`
inside the extension source directory. The uAMQP patch is applied from the
extension's tracked `patches` directory before compiling this dependency. See
[receiver-credit patch details](../patches/README.md) when upgrading an existing
uAMQP installation. Rebuild both the dependency and extension together.

### `PHUAMQP_PHP_CPP_VERSION`

Optional override for the PHP-CPP version used during the build.

- **If set**: `PHP_CPP_VERSION` uses this value.
- **Default**: `2.4.1`

Example:

```bash
export PHUAMQP_PHP_CPP_VERSION=2.4.1
```

## Default values used by the script

If you do not provide any environment variables, the script uses these defaults:

- `UAMQP_EXT_DIR` = directory containing `setup.sh`
- `UAMQP_LIBS_BUILD_DIR` = `${UAMQP_EXT_DIR}/libs-build`
- `PHP_MAJOR_VERSION` = `8.3`
- `PHP_CPP_VERSION` = `2.4.1`

## Example usage

Run with defaults:

```bash
sudo bash setup.sh
```

Run with custom PHP and PHP-CPP versions:

```bash
export PHUAMQP_PHP_MAJOR_VERSION=8.3
export PHUAMQP_PHP_CPP_VERSION=2.4.1
sudo bash setup.sh
```

Run with a custom extension directory override:

```bash
export PHP_UAMQP_BUILD_DIR=/path/to/your/php-uamqp-source
sudo bash setup.sh
```

## Notes

- The script expects to run on a Debian-based system.
- It installs packages via `apt-get` and therefore must be run as `root` or with `sudo`.
- It creates and later removes the temporary `libs-build` directory after a successful run.

## Debug logging

When a connection is created with debug mode enabled, uAMQP logs are written to standard output.
Set `UAMQP_DEBUG_FILE` to append those logs to a file instead:

```bash
export UAMQP_DEBUG_FILE=/var/log/uamqp-debug.log
```

If the file cannot be opened for writing, the extension reports the error on standard output and
continues logging to standard output.

## Subscription debugging command

The repository includes a standalone C++ diagnostic consumer in `debugging/`. It accepts the
same AMQP resource forms used by the extension, including topic subscriptions:

```text
amqps://host:5671/topic/Subscriptions/subscription?verify=verify_none
host:5671/queue
host:5671/topic
USER:PASSWORD@host:5671/topic/Subscriptions/subscription
```

Build it after the dependencies have been prepared by `setup.sh`:

```bash
make -C debugging
```

Pass credentials without putting them in shell history or source control where possible:

```bash
export UAMQP_ENDPOINT='amqps://host:5671/topic/Subscriptions/subscription?verify=verify_none'
export UAMQP_USER='RootManageSharedAccessKey'
export UAMQP_PASSWORD='service-bus-key'
debugging/build/subscription_consumer --count 2 --timeout 60
```

The endpoint query string is ignored for transport setup; TLS remains enabled for the diagnostic
command. The command enables uAMQP frame tracing and prints connection, receiver, detach, error,
and message logs. Binary AMQP data bodies are printed to standard output; non-binary bodies are
reported without attempting an unsafe binary conversion.

Credentials may be supplied as `USER:PASSWORD@HOST` in the endpoint or through `UAMQP_USER` and
`UAMQP_PASSWORD`. Percent-encoded credential bytes (for example `%2B` and `%3D` in a base64 key)
are decoded before SASL authentication. Do not place credentials in committed scripts or logs.

If DNS resolution shows a hostname containing `USER:PASSWORD@`, the endpoint was parsed
incorrectly and must use the supported user-info form above or separate credential variables. If
the connection reaches `OPENED` but the receiver is detached with `amqp:unauthorized-access`, the
endpoint is valid but the decoded SASL identity/key does not have receive permission for the
specified entity, or the key belongs to a different namespace. Use a namespace-matching SAS policy
with Listen permission on the queue or topic subscription and retry.

Exit status `0` means the requested messages were consumed, `1` means a receive or transport
failure occurred before the requested count, and `2` means command-line validation failed.
The command does not change the PHP extension's queue/topic resource handling, so existing
non-subscription paths remain available for regression checks.

## Message body types

`Azure\uAMQP\Message::getBody()` safely handles all uAMQP body sections:

- `data` sections are concatenated as a PHP string, including messages with multiple sections.
- `value` sections are returned using uAMQP's safe string representation.
- `sequence` sections are returned as newline-separated safe string representations.
- `none` returns an empty string.

`Message::getBodyType()` returns `data`, `value`, `sequence`, or `none`. Body decoding errors raise
a PHP exception instead of calling a data-only decoder and risking a process crash. The consumer
regression scripts print the body type for every received message; use them with messages produced
by systems that emit `amqp-value` or `amqp-sequence` bodies to validate interoperability.
