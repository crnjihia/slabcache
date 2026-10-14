#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

if [ ! -f build/release/slabcache_server ]; then
    cmake --preset release
    cmake --build --preset release
fi

PORT=6380
echo "Starting slabcache_server on port ${PORT}..."
./build/release/slabcache_server -p ${PORT} &
SERVER_PID=$!

trap "kill -9 ${SERVER_PID} 2>/dev/null || true" EXIT

sleep 0.5

echo "--- 1. PING smoke test ---"
redis-cli -3 -p ${PORT} PING

echo "--- 2. SET foo bar ---"
redis-cli -3 -p ${PORT} SET foo bar

echo "--- 3. GET foo ---"
redis-cli -3 -p ${PORT} GET foo

echo "--- 4. EXPIRE & TTL ---"
redis-cli -3 -p ${PORT} EXPIRE foo 60
redis-cli -3 -p ${PORT} TTL foo

echo "--- 5. STATS fragmentation ---"
redis-cli -3 -p ${PORT} STATS

echo "--- 6. DEL & GET (RESP3 null) ---"
redis-cli -3 -p ${PORT} DEL foo
redis-cli -3 -p ${PORT} GET foo

echo "=== Smoke test passed successfully! ==="
