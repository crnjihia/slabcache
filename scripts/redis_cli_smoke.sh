#!/usr/bin/env bash

# Simple smoke test using redis-cli -3 protocol against the slabcache server.
# Assumes the server binary is built as slabcache_server and listens on port 6380.
# This script starts the server in the background, runs a SET command, and prints the output.

set -e

# Build the server if not already built.
if [ ! -f build/release/slabcache_server ]; then
    cmake --preset release
    cmake --build --preset release
fi

# Start the server in background.
./build/release/slabcache_server &
SERVER_PID=$!

# Give it a moment to start.
sleep 1

# Run a basic SET command.
redis-cli -3 -p 6380 SET foo bar

# Cleanup.
kill $SERVER_PID || true
