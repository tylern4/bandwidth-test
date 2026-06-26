#!/bin/bash
set -e

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BENCH="$BASE_DIR/build/bandwidth-test"
ANALYSIS_DIR="$BASE_DIR/analysis"
RESULTS_DIR="$BASE_DIR/results"
BENCH_RESULTS_DIR="$RESULTS_DIR/bench"

START=${1:-64}
END=${2:-20971520}
FACTOR=${3:-2.0}

echo "=== Bandwidth Sweep & Plot ==="
echo "Range: $START to $END (factor $FACTOR)"
echo ""

mkdir -p "$RESULTS_DIR" "$BENCH_RESULTS_DIR"

# ------------------------------------------------------------------
# 1. Run MPI sweep
# ------------------------------------------------------------------
echo "--- Running MPI sweep ($START -> $END, factor $FACTOR) ---"

MPI_OUTPUT="$BENCH_RESULTS_DIR/mpi_sweep.json"

mpirun -np 2 "$BENCH" \
    -b mpi \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$MPI_OUTPUT" \
    --warmup 3 \
    -n 50 2>&1 | tail -5

echo "MPI output saved to: $MPI_OUTPUT"
echo ""

# ------------------------------------------------------------------
# 2. Run ZMQ sweep
# ------------------------------------------------------------------
echo "--- Running ZMQ sweep ($START -> $END, factor $FACTOR) ---"

ZMQ_SERVER_OUTPUT="$BENCH_RESULTS_DIR/zmq_server.json"
ZMQ_CLIENT_OUTPUT="$BENCH_RESULTS_DIR/zmq_client.json"

# Start server in background
"$BENCH" -b zmq --zmq-mode server \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$ZMQ_SERVER_OUTPUT" \
    --warmup 3 \
    -n 50 &
SERVER_PID=$!
sleep 2

# Run client and capture output
"$BENCH" -b zmq --zmq-mode client \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$ZMQ_CLIENT_OUTPUT" \
    --warmup 3 \
    -n 50 2>&1 | tail -5

# Clean up server
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true

echo "ZMQ output saved to: $ZMQ_CLIENT_OUTPUT"
echo ""

# ------------------------------------------------------------------
# 3. Plot results
# ------------------------------------------------------------------
echo "--- Plotting ---"

cd "$ANALYSIS_DIR"

# Source the venv
if [ -f ".venv/bin/activate" ]; then
    source .venv/bin/activate
fi

PLOT_OUTPUT="$RESULTS_DIR/plots"
"$ANALYSIS_DIR/.venv/bin/python" "$ANALYSIS_DIR/plot_results.py" \
    "$MPI_OUTPUT" "$ZMQ_CLIENT_OUTPUT" "$ZMQ_SERVER_OUTPUT" \
    --output "$PLOT_OUTPUT"

echo ""
echo "=== Done ==="
echo "Results: $BENCH_RESULTS_DIR/"
echo "Plots:   $PLOT_OUTPUT/"
ls "$PLOT_OUTPUT"/
