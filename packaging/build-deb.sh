#!/usr/bin/env bash
set -euo pipefail
# PHUAMQP_PACKAGE_VERSION="v0.1.1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PHP_MAJOR_VERSION="${PHUAMQP_PHP_MAJOR_VERSION:-${PHP_MAJOR_VERSION:-8.3}}"
PHP_CONFIG_BIN="php${PHP_MAJOR_VERSION}-config"
if ! command -v "${PHP_CONFIG_BIN}" >/dev/null 2>&1; then
  PHP_CONFIG_BIN="php-config"
fi
PHP_API="${PHUAMQP_PHP_API:-$(${PHP_CONFIG_BIN} --phpapi)}"
PACKAGE_VERSION="${PHUAMQP_PACKAGE_VERSION:-${GITHUB_REF_NAME:-}}"
PACKAGE_VERSION="${PACKAGE_VERSION#v}"
PACKAGE_VERSION="${PACKAGE_VERSION:-0.1.1}"

EXTENSION_NAME="uamqpphpbinding.so"
MODULE_NAME="uamqpphpbinding"
BUILD_OUTPUT="${PROJECT_ROOT}/${EXTENSION_NAME}"
PACKAGE_ROOT="${PROJECT_ROOT}/pkg"
OUTPUT_PACKAGE="${PROJECT_ROOT}/php${PHP_MAJOR_VERSION}-uamqpphpbinding_${PACKAGE_VERSION}_amd64.deb"

if [[ ! -f "${BUILD_OUTPUT}" ]]; then
  echo "Built extension not found: ${BUILD_OUTPUT}" >&2
  echo "Run ./setup.sh first." >&2
  exit 1
fi

rm -rf "${PACKAGE_ROOT}" "${OUTPUT_PACKAGE}"
mkdir -p \
  "${PACKAGE_ROOT}/DEBIAN" \
  "${PACKAGE_ROOT}/usr/lib/php/${PHP_API}" \
  "${PACKAGE_ROOT}/etc/php/${PHP_MAJOR_VERSION}/mods-available"

cp "${BUILD_OUTPUT}" "${PACKAGE_ROOT}/usr/lib/php/${PHP_API}/${EXTENSION_NAME}"

cat > "${PACKAGE_ROOT}/etc/php/${PHP_MAJOR_VERSION}/mods-available/${MODULE_NAME}.ini" <<EOF
extension=${EXTENSION_NAME}
EOF

cat > "${PACKAGE_ROOT}/DEBIAN/control" <<EOF
Package: php${PHP_MAJOR_VERSION}-ubuntu-uamqpphpbinding
Version: ${PACKAGE_VERSION}
Section: php
Priority: optional
Architecture: amd64
Maintainer: Mirche Arsov <mirche.arsov@product-experience-solutions.com>
Depends: php${PHP_MAJOR_VERSION}-common
Description: Azure AMQP 1.0 extension for PHP
EOF

cat > "${PACKAGE_ROOT}/DEBIAN/postinst" <<EOF
#!/bin/sh
set -e
phpenmod -v "${PHP_MAJOR_VERSION}" "${MODULE_NAME}" >/dev/null 2>&1 || true
exit 0
EOF
chmod 755 "${PACKAGE_ROOT}/DEBIAN/postinst"

mapfile -t dependency_paths < <(
  ldd "${BUILD_OUTPUT}" | awk '
    $1 ~ /^\// { print $1 }
    $2 == "=>" && $3 ~ /^\// { print $3 }
    $3 == "=>" && $4 ~ /^\// { print $4 }
  ' | sort -u
)

for dependency in "${dependency_paths[@]}"; do
  [[ -e "${dependency}" ]] || continue

  if dpkg-query -S "${dependency}" >/dev/null 2>&1; then
    echo "Skipping already packaged dependency: ${dependency}"
    continue
  fi

  target="${PACKAGE_ROOT}${dependency}"
  mkdir -p "$(dirname "${target}")"
  cp -a "${dependency}" "${target}"
done

dpkg-deb --build "${PACKAGE_ROOT}" "${OUTPUT_PACKAGE}"

echo "Created package: ${OUTPUT_PACKAGE}"

