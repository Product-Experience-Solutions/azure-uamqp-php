#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="${SCRIPT_DIR}/../VERSION"

if [[ $# -ne 0 && ( $# -ne 2 || "${1:-}" != "--check-tag" ) ]]; then
  echo "Usage: bash scripts/version.sh [--check-tag vMAJOR.MINOR.PATCH]" >&2
  exit 1
fi

if [[ ! -f "${VERSION_FILE}" ]]; then
  echo "Version file not found: ${VERSION_FILE}" >&2
  exit 1
fi

extension_version="$(<"${VERSION_FILE}")"
if [[ ! "${extension_version}" =~ ^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
  echo "VERSION must contain MAJOR.MINOR.PATCH (for example, 1.2.3), without a v prefix." >&2
  exit 1
fi

if [[ $# -eq 2 && "$2" != "v${extension_version}" ]]; then
  echo "Release tag '$2' does not match VERSION; expected 'v${extension_version}'." >&2
  exit 1
fi

printf '%s\n' "${extension_version}"
