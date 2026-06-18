#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CONSUMER_LOG="${PROJECT_DIR}/scripts/.consumer.log"
PRODUCER_LOG="${PROJECT_DIR}/scripts/.producer.log"
CONSUMER_PID=""

wait_for_health() {
	local host="$1"
	local port="$2"
	local timeout_seconds="$3"
	local paths_csv="$4"
	local start_time="$SECONDS"
	local paths=()
	IFS=',' read -r -a paths <<< "$paths_csv"

	while (( SECONDS - start_time < timeout_seconds )); do
		for path in "${paths[@]}"; do
			local url="http://${host}:${port}${path}"
			local response
			response="$(curl -fsS --max-time 5 "$url" 2>/dev/null || true)"

			if [ -n "$response" ] && printf '%s' "$response" | grep -qiE 'healthy|ok|ready'; then
				echo "Emulator health check passed at ${url}"
				return 0
			fi
		done

		echo "Waiting for emulator health on ${host}:${port}..."
		sleep 2
	done

	echo "Timed out waiting for emulator health on ${host}:${port}" >&2
	return 1
}

cleanup() {
	if [ -n "$CONSUMER_PID" ] && kill -0 "$CONSUMER_PID" >/dev/null 2>&1; then
		kill "$CONSUMER_PID" >/dev/null 2>&1 || true
	fi
}

trap cleanup EXIT

export PHUAMQP_HOST="${PHUAMQP_HOST:-phuamqp-servicebus-emulator}"
export PHUAMQP_PORT="${PHUAMQP_PORT:-5672}"
export PHUAMQP_USE_TLS="${PHUAMQP_USE_TLS:-0}"
export PHUAMQP_KEY_NAME="${PHUAMQP_KEY_NAME:-RootManageSharedAccessKey}"
export PHUAMQP_ACCESS_KEY="${PHUAMQP_ACCESS_KEY:-EmulatorPassword123!}"
export PHUAMQP_QUEUE_NAME="${PHUAMQP_QUEUE_NAME:-test-queue}"
export PHUAMQP_MESSAGE_COUNT="${PHUAMQP_MESSAGE_COUNT:-3}"
export PHUAMQP_CONSUMER_TIMEOUT="${PHUAMQP_CONSUMER_TIMEOUT:-60}"
export PHUAMQP_STARTUP_WAIT="${PHUAMQP_STARTUP_WAIT:-3}"
export PHUAMQP_EMULATOR_TIMEOUT="${PHUAMQP_EMULATOR_TIMEOUT:-180}"
export PHUAMQP_EMULATOR_HEALTH_HOST="${PHUAMQP_EMULATOR_HEALTH_HOST:-${PHUAMQP_HOST}}"
export PHUAMQP_EMULATOR_HEALTH_PORT="${PHUAMQP_EMULATOR_HEALTH_PORT:-5300}"
export PHUAMQP_EMULATOR_HEALTH_PATHS="${PHUAMQP_EMULATOR_HEALTH_PATHS:-/health,/healthz,/api/health}"

rm -f "$CONSUMER_LOG" "$PRODUCER_LOG"

echo "Waiting for emulator health at ${PHUAMQP_EMULATOR_HEALTH_HOST}:${PHUAMQP_EMULATOR_HEALTH_PORT}..."
wait_for_health "$PHUAMQP_EMULATOR_HEALTH_HOST" "$PHUAMQP_EMULATOR_HEALTH_PORT" "$PHUAMQP_EMULATOR_TIMEOUT" "$PHUAMQP_EMULATOR_HEALTH_PATHS"

echo "Starting consumer first..."
php "${PROJECT_DIR}/scripts/test-consumer.php" >"$CONSUMER_LOG" 2>&1 &
CONSUMER_PID=$!

sleep "$PHUAMQP_STARTUP_WAIT"

echo "Starting producer..."
if ! php "${PROJECT_DIR}/scripts/test-producer.php" >"$PRODUCER_LOG" 2>&1; then
	cat "$PRODUCER_LOG" >&2 || true
	cat "$CONSUMER_LOG" >&2 || true
	exit 1
fi

if ! wait "$CONSUMER_PID"; then
	cat "$CONSUMER_LOG" >&2 || true
	cat "$PRODUCER_LOG" >&2 || true
	exit 1
fi

cat "$PRODUCER_LOG"
cat "$CONSUMER_LOG"

rm -f "$CONSUMER_LOG" "$PRODUCER_LOG"

echo "E2E producer/consumer flow completed successfully."

