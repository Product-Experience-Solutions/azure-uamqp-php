#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
extension_dir="$(cd "${script_dir}/.." && pwd)"
source_dir="${1:-${PHUAMQP_LIBS_BUILD_DIR:-${extension_dir}/libs-build}/azure-uamqp-c}"
patch_path="${extension_dir}/patches/azure-uamqp-c-manual-receiver-credit.patch"

if [[ $# -gt 1 || ! -f "${source_dir}/src/link.c" ]]; then
    echo "Usage: $0 [azure-uamqp-c-source-directory]" >&2
    exit 1
fi

# --check never edits the dependency. Refuse incompatible or partially applied
# changes so a build cannot silently restore unlimited receive credit.
if git -C "${source_dir}" apply --reverse --check "${patch_path}" 2>/dev/null; then
    echo "uAMQP manual receiver credit patch is already applied."
elif git -C "${source_dir}" apply --check "${patch_path}"; then
    git -C "${source_dir}" apply "${patch_path}"
    echo "Applied uAMQP manual receiver credit patch."
else
    echo "Cannot apply the required uAMQP receiver-credit patch to ${source_dir}." >&2
    echo "Use a compatible clean dependency checkout; see patches/README.md." >&2
    exit 1
fi
