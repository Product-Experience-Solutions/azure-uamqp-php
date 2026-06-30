#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND="${DEBIAN_FRONTEND:-noninteractive}"

PHP_MAJOR_VERSION="8.3"
if [ -n "${PHUAMQP_PHP_MAJOR_VERSION:-}" ]; then
    PHP_MAJOR_VERSION="${PHUAMQP_PHP_MAJOR_VERSION}"
fi

PHP_RUNTIME_BIN="php${PHP_MAJOR_VERSION}"
if ! command -v "${PHP_RUNTIME_BIN}" >/dev/null 2>&1; then
    PHP_RUNTIME_BIN="php"
fi

if ! command -v "${PHP_RUNTIME_BIN}" >/dev/null 2>&1; then
    echo "✗ Unable to locate a PHP runtime binary (php${PHP_MAJOR_VERSION} or php)" >&2
    exit 1
fi

PHP_EXTENSION_DIR="$("${PHP_RUNTIME_BIN}" -r 'echo ini_get("extension_dir");' 2>/dev/null || true)"
if [ -n "${PHP_EXTENSION_DIR}" ] && [ -d "${PHP_EXTENSION_DIR}" ]; then
    extension_found=0
    for extension_name in "uamqpphpbinding.so" "uamqp.so" "azure_uamqp.so"; do
        if [ -e "${PHP_EXTENSION_DIR}/${extension_name}" ]; then
            extension_found=1
            break
        fi
    done

    if [ "${extension_found}" -eq 0 ]; then
        PHP_EXTENSION_DIR=""
    fi
fi

if [ -z "${PHP_EXTENSION_DIR}" ] || [ ! -d "${PHP_EXTENSION_DIR}" ]; then
    for root in /usr/lib/php /usr/local/lib/php /usr/lib/x86_64-linux-gnu/php; do
        [ -d "${root}" ] || continue
        candidate="$(find "${root}" -maxdepth 3 -type f \( -name "uamqpphpbinding.so" -o -name "uamqp.so" -o -name "azure_uamqp.so" \) -print -quit 2>/dev/null || true)"
        if [ -n "${candidate}" ]; then
            PHP_EXTENSION_DIR="$(dirname "${candidate}")"
            break
        fi
    done
fi

if [ -z "${PHP_EXTENSION_DIR}" ] || [ ! -d "${PHP_EXTENSION_DIR}" ]; then
    echo "✗ Unable to determine the PHP extension directory" >&2
    exit 1
fi

PHP_INI_DIR="/etc/php/${PHP_MAJOR_VERSION}"
MODULE_AVAILABLE_DIR="${PHP_INI_DIR}/mods-available"

MODULE_NAMES=(
    "uamqpphpbinding"
    "uamqp"
)

EXTENSION_NAMES=(
    "uamqpphpbinding.so"
    "uamqp.so"
    "azure_uamqp.so"
)

remove_file_if_exists() {
    local file_path="$1"

    if [ -e "${file_path}" ] || [ -L "${file_path}" ]; then
        rm -f "${file_path}"
        echo "✓ Removed ${file_path}"
    fi
}

echo "============================================================================="
echo "Removing Azure uAMQP PHP Extension"
echo "PHP Version: ${PHP_MAJOR_VERSION}"
echo "Extension Dir: ${PHP_EXTENSION_DIR}"
echo "============================================================================="

for module_name in "${MODULE_NAMES[@]}"; do
    if command -v phpdismod >/dev/null 2>&1; then
        phpdismod -v "${PHP_MAJOR_VERSION}" "${module_name}" >/dev/null 2>&1 || true
    fi

    remove_file_if_exists "${MODULE_AVAILABLE_DIR}/${module_name}.ini"

    for sapi_dir in cli fpm cgi apache2; do
        remove_file_if_exists "${PHP_INI_DIR}/${sapi_dir}/conf.d/20-${module_name}.ini"
    done
done

for extension_name in "${EXTENSION_NAMES[@]}"; do
    remove_file_if_exists "${PHP_EXTENSION_DIR}/${extension_name}"
done

echo "============================================================================="
echo "✓ Azure uAMQP PHP Extension removed"
echo "============================================================================="
