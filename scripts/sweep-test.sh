#!/bin/bash
# Sweep benchmark - ZMQ
# Tests multiple message sizes with logarithmic spacing
# Usage: ./scripts/sweep-test.sh [start] [end] [factor]

set -e

START=${1:-64}
END=${2:-1048576}
FACTOR=${3:-2.0}

echo "=== Bandwidth Test - Size Sweep ==="
echo "Range: ${START} to ${END} (factor ${FACTOR})"
echo ""

# Start server in background
./bandwidth-test -b zmq --zmq-mode server --sweep "$START" "$END" "$FACTOR" -n 50 --warmup 5 &
SERVER_PID=$!

# Give server time to start
sleep 1

# Run client
./bandwidth-test -b zmq --zmq-mode client --sweep "$START" "$END" "$FACTOR" -n 50 --warmup 5

# Clean up
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
