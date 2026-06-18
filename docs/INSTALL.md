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
   - Azure uAMQP C
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

