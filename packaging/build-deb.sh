#!/usr/bin/env bash
set -euo pipefail
PHUAMQP_PACKAGE_VERSION="v0.1.3"
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
PACKAGE_VERSION="${PACKAGE_VERSION:-0.2.0}"

EXTENSION_NAME="uamqpphpbinding.so"
MODULE_NAME="uamqpphpbinding"
PACKAGE_ROOT="${PROJECT_ROOT}/pkg-${PACKAGE_VERSION}"
OUTPUT_PACKAGE="${PROJECT_ROOT}/php${PHP_MAJOR_VERSION}-uamqpphpbinding_${PACKAGE_VERSION}_amd64.deb"
REQUIRED_DEBIAN_PACKAGES=(
  "libc6"
  "libcom-err2"
  "libgcc-s1"
  "libkeyutils1"
  "php${PHP_MAJOR_VERSION}-common"
  "zlib1g"
)

locate_extension() {
  local search_roots=(
    "${PROJECT_ROOT}"
    "$(${PHP_CONFIG_BIN} --extension-dir 2>/dev/null || true)"
    "/usr/lib/php"
    "/usr/local/lib/php"
    "/usr/lib"
    "/usr/local/lib"
  )
  local root candidate

  for root in "${search_roots[@]}"; do
    [[ -n "${root}" && -d "${root}" ]] || continue

    if [[ -f "${root}/${EXTENSION_NAME}" ]]; then
      printf '%s\n' "${root}/${EXTENSION_NAME}"
      return 0
    fi

    candidate="$(find "${root}" -maxdepth 4 -type f -name "${EXTENSION_NAME}" -print -quit 2>/dev/null || true)"
    if [[ -n "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

BUILD_OUTPUT="$(locate_extension || true)"

if [[ -z "${BUILD_OUTPUT}" || ! -f "${BUILD_OUTPUT}" ]]; then
  echo "Built extension not found in the filesystem: ${EXTENSION_NAME}" >&2
  echo "Run ./setup.sh first, or pass a filesystem location by placing the file in a standard extension path." >&2
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
Depends: $(IFS=,; echo "${REQUIRED_DEBIAN_PACKAGES[*]}")
Description: Azure AMQP 1.0 extension for PHP
EOF

cat > "${PACKAGE_ROOT}/DEBIAN/postinst" <<EOF
#!/bin/sh
set -e
phpenmod -v "${PHP_MAJOR_VERSION}" "${MODULE_NAME}" >/dev/null 2>&1 || true
ldconfig >/dev/null 2>&1 || true
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
  echo "Processing dependency: ${dependency}"
  dependency_name="$(basename "${dependency}")"

  case "${dependency_name}" in
    libaziotsharedutil.so|libc_logging_v2.so|libuamqp.so)
      if [[ "${dependency_name}" == "libuamqp.so" ]]; then
        echo "Bundling libuamqp family from /usr/local/lib/libuamqp* and /lib/libuamqp*"
        for uamqp_library in /usr/local/lib/libuamqp* /lib/libuamqp*; do
          [[ -e "${uamqp_library}" ]] || continue
          target="${PACKAGE_ROOT}${uamqp_library}"
          mkdir -p "$(dirname "${target}")"
          cp -a "${uamqp_library}" "${target}"
        done
        continue
      fi
      ;;
    libphpcpp.so.2.4)
      echo "Bundling libphpcpp family from /lib/libphpcpp*"
      for phpcpp_library in /lib/libphpcpp*; do
        [[ -e "${phpcpp_library}" ]] || continue
        target="${PACKAGE_ROOT}${phpcpp_library}"
        mkdir -p "$(dirname "${target}")"
        cp -a "${phpcpp_library}" "${target}"
      done
      continue
      ;;
    *)
      echo "Skipping non-bundled dependency: ${dependency}"
      continue
      ;;
  esac

  if dpkg-query -S "${dependency}" >/dev/null 2>&1; then
    echo "Skipping already packaged dependency: ${dependency}"
    continue
  fi

  echo "Bundling dependency: ${dependency}"
  target="${PACKAGE_ROOT}${dependency}"
  mkdir -p "$(dirname "${target}")"
  cp -a "${dependency}" "${target}"
done

echo "Building Debian package..."
dpkg-deb --build "${PACKAGE_ROOT}" "${OUTPUT_PACKAGE}"

echo "Created package: ${OUTPUT_PACKAGE}"
