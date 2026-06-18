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

## Notes

- Run the script with `sudo` or as `root` because it installs packages and writes to system directories.
- The script is intended for Debian-based environments.
- The old manual build instructions have been replaced by the automated setup script.
