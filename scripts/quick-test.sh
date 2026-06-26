#!/bin/bash
# Quick benchmark test - ZMQ local TCP
# Usage: ./scripts/quick-test.sh [message_size]

set -e

SIZE=${1:-1024}
NUM_TESTS=${2:-100}

echo "=== Bandwidth Test - Quick Test ==="
echo "Message size: ${SIZE} bytes"
echo "Tests: ${NUM_TESTS}"
echo ""

# Start server in background
./bandwidth-test -b zmq --zmq-mode server -s "$SIZE" -n "$NUM_TESTS" --warmup 5 &
SERVER_PID=$!

# Give server time to start
sleep 1

# Run client
./bandwidth-test -b zmq --zmq-mode client -s "$SIZE" -n "$NUM_TESTS" --warmup 5

# Clean up server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
