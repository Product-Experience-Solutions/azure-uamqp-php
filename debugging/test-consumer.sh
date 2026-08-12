#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="${SCRIPT_DIR}/build/subscription_consumer"

if [[ ! -x "$EXECUTABLE" ]]; then
    echo "Diagnostic executable not found; run: make -C debugging" >&2
    exit 2
fi

exec "$EXECUTABLE" --count "${UAMQP_MESSAGE_COUNT:-1}" \
    --timeout "${UAMQP_TIMEOUT_SECONDS:-60}"
