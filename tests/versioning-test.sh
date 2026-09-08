#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEST_ROOT="$(mktemp -d /tmp/phuamqp-versioning.XXXXXX)"
BUILD_LOG="${TEST_ROOT}/build.log"

cleanup() {
  local test_status=$?
  if [[ ${test_status} -ne 0 && -f "${BUILD_LOG}" ]]; then
    tail -n 80 "${BUILD_LOG}" >&2
  fi
  rm -rf -- "${TEST_ROOT}"
}
trap cleanup EXIT

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_equal() {
  [[ "$1" == "$2" ]] || fail "$3: expected '$1', got '$2'"
  echo "PASS: $3"
}

expect_failure() {
  local expected_message="$1"
  shift
  if "$@" > "${TEST_ROOT}/failure.log" 2>&1; then
    fail "Expected command to fail: $*"
  fi
  if ! grep -Fq "${expected_message}" "${TEST_ROOT}/failure.log"; then
    cat "${TEST_ROOT}/failure.log" >&2
    fail "Missing expected error: ${expected_message}"
  fi
  echo "PASS: rejected ${expected_message}"
}

export PHUAMQP_PHP_MAJOR_VERSION="${PHUAMQP_PHP_MAJOR_VERSION:-$(php -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;')}"
TEST_PHP_BIN="php${PHUAMQP_PHP_MAJOR_VERSION}"
if ! command -v "${TEST_PHP_BIN}" >/dev/null 2>&1; then
  TEST_PHP_BIN=php
fi

read_binary_version() {
  "${TEST_PHP_BIN}" -n -d "extension=$1" -r 'echo phpversion("uamqpphpbinding");'
}

assert_package_version() {
  local expected_version="$1"
  local package_path="${TEST_ROOT}/php${PHUAMQP_PHP_MAJOR_VERSION}-uamqpphpbinding_${expected_version}_amd64.deb"
  local extracted_path="${TEST_ROOT}/unpacked-${expected_version}"
  [[ -f "${package_path}" ]] || fail "Expected package filename: ${package_path}"
  assert_equal "${expected_version}" "$(dpkg-deb -f "${package_path}" Version)" "Debian metadata matches VERSION"
  dpkg-deb -x "${package_path}" "${extracted_path}"
  local packaged_extension
  packaged_extension="$(find "${extracted_path}/usr/lib/php" -type f -name uamqpphpbinding.so)"
  assert_equal "${expected_version}" "$(read_binary_version "${packaged_extension}")" "Packaged extension reports the package version"
}

# Copy only build inputs; never modify the checkout or its installed extension.
mkdir -p "${TEST_ROOT}/scripts" "${TEST_ROOT}/packaging" "${TEST_ROOT}/tests"
cp "${PROJECT_ROOT}/Makefile" "${PROJECT_ROOT}/VERSION" \
  "${PROJECT_ROOT}/"*.cpp "${PROJECT_ROOT}/"*.h "${TEST_ROOT}/"
cp "${PROJECT_ROOT}/scripts/version.sh" "${TEST_ROOT}/scripts/"
cp "${PROJECT_ROOT}/packaging/build-deb.sh" "${TEST_ROOT}/packaging/"
cp "${PROJECT_ROOT}/tests/"*.cpp "${TEST_ROOT}/tests/"

initial_version="$(bash "${TEST_ROOT}/scripts/version.sh")"
assert_equal "${initial_version}" "$(bash "${TEST_ROOT}/scripts/version.sh" --check-tag "v${initial_version}")" "Matching release tag"
expect_failure "does not match VERSION" bash "${TEST_ROOT}/scripts/version.sh" --check-tag "v999.999.999"
expect_failure "Usage:" bash "${TEST_ROOT}/scripts/version.sh" --check-tag

mv "${TEST_ROOT}/VERSION" "${TEST_ROOT}/VERSION.saved"
expect_failure "Version file not found" bash "${TEST_ROOT}/scripts/version.sh"
mv "${TEST_ROOT}/VERSION.saved" "${TEST_ROOT}/VERSION"

for invalid_version in '' v1.2.3 1.2 01.2.3 '../1.2.3' '1.2.3 extra' $'1.2.3\n4.5.6'; do
  printf '%s\n' "${invalid_version}" > "${TEST_ROOT}/VERSION"
  expect_failure "VERSION must contain MAJOR.MINOR.PATCH" bash "${TEST_ROOT}/scripts/version.sh"
done
expect_failure "Could not read a valid extension version" make -s -C "${TEST_ROOT}" all
expect_failure "VERSION must contain MAJOR.MINOR.PATCH" bash "${TEST_ROOT}/packaging/build-deb.sh"
printf '%s\n' "${initial_version}" > "${TEST_ROOT}/VERSION"

# A command-line Make variable must not override the central version.
make -C "${TEST_ROOT}" -j2 PHUAMQP_VERSION=999.999.999 all test > "${BUILD_LOG}" 2>&1
assert_equal "${initial_version}" "$(read_binary_version "${TEST_ROOT}/uamqpphpbinding.so")" "Native extension reads VERSION"
PHUAMQP_PACKAGE_VERSION=999.999.999 GITHUB_REF_NAME=v999.999.999 \
  bash "${TEST_ROOT}/packaging/build-deb.sh" >> "${BUILD_LOG}" 2>&1
assert_package_version "${initial_version}"

next_version=42.7.9
if [[ "${initial_version}" == "${next_version}" ]]; then
  next_version=42.7.10
fi
# Allow filesystems with one-second timestamp resolution to observe the version-only change.
sleep 1
printf '%s\n' "${next_version}" > "${TEST_ROOT}/VERSION"
expect_failure "does not match VERSION" bash "${TEST_ROOT}/packaging/build-deb.sh"
[[ ! -e "${TEST_ROOT}/pkg-${next_version}" ]] || fail "Stale binary check must precede package writes"

make -C "${TEST_ROOT}" -j2 all >> "${BUILD_LOG}" 2>&1
assert_equal "${next_version}" "$(read_binary_version "${TEST_ROOT}/uamqpphpbinding.so")" "Changing only VERSION triggers an incremental rebuild"
bash "${TEST_ROOT}/packaging/build-deb.sh" >> "${BUILD_LOG}" 2>&1
assert_package_version "${next_version}"

echo "Versioning regression tests passed."
