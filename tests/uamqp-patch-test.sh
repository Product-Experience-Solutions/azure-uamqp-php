#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
extension_dir="$(cd "${script_dir}/.." && pwd)"
source_dir="${UAMQP_SOURCE_DIR:-${PHUAMQP_LIBS_BUILD_DIR:-${extension_dir}/libs-build}/azure-uamqp-c}"
test_dir="$(mktemp -d /tmp/phuamqp-credit-patch.XXXXXX)"
trap 'rm -rf -- "${test_dir}"' EXIT

# Extract immutable upstream inputs so this test never reverses the working copy.
git -C "${source_dir}" archive HEAD src/link.c inc/azure_uamqp_c/link.h | tar -x -C "${test_dir}"
bash "${extension_dir}/scripts/apply-uamqp-patches.sh" "${test_dir}"
first_hash="$(sha256sum "${test_dir}/src/link.c" "${test_dir}/inc/azure_uamqp_c/link.h")"
bash "${extension_dir}/scripts/apply-uamqp-patches.sh" "${test_dir}"
[[ "${first_hash}" == "$(sha256sum "${test_dir}/src/link.c" "${test_dir}/inc/azure_uamqp_c/link.h")" ]]
echo "PASS: patch applies to upstream sources and a second application changes nothing"

git -C "${test_dir}" apply --reverse --include=inc/azure_uamqp_c/link.h \
    "${extension_dir}/patches/azure-uamqp-c-manual-receiver-credit.patch"
partial_hash="$(sha256sum "${test_dir}/src/link.c" "${test_dir}/inc/azure_uamqp_c/link.h")"
if bash "${extension_dir}/scripts/apply-uamqp-patches.sh" "${test_dir}"; then
    echo "FAIL: partially applied patch was accepted" >&2
    exit 1
fi
[[ "${partial_hash}" == "$(sha256sum "${test_dir}/src/link.c" "${test_dir}/inc/azure_uamqp_c/link.h")" ]]
echo "PASS: partial patch fails without modifying any dependency source"
