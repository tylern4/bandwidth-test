#!/bin/bash
# Chunked transfer benchmark
# Tests chunked vs single transfer mode
# Usage: ./scripts/chunked-test.sh [message_size] [chunk_count]

set -e

SIZE=${1:-1048576}
CHUNKS=${2:-100}

echo "=== Bandwidth Test - Chunked Transfer ==="
echo "Message size: ${SIZE} bytes"
echo "Chunk count: ${CHUNKS}"
echo "Chunk size: $((SIZE / CHUNKS)) bytes"
echo ""

echo "--- Single mode ---"
./bandwidth-test -b zmq --zmq-mode server -s "$SIZE" --transfer-mode single -n 50 --warmup 5 &
SERVER_PID=$!
sleep 1
./bandwidth-test -b zmq --zmq-mode client -s "$SIZE" --transfer-mode single -n 50 --warmup 5
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

echo ""
echo "--- Chunked mode ---"
./bandwidth-test -b zmq --zmq-mode server -s "$SIZE" --transfer-mode chunked --chunk-count "$CHUNKS" -n 50 --warmup 5 &
SERVER_PID=$!
sleep 1
./bandwidth-test -b zmq --zmq-mode client -s "$SIZE" --transfer-mode chunked --chunk-count "$CHUNKS" -n 50 --warmup 5
