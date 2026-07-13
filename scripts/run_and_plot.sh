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

MPI_OUTPUT_NP2="$BENCH_RESULTS_DIR/mpi_sweep_np2.json"

mpirun -np 2 "$BENCH" \
    -b mpi \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$MPI_OUTPUT_NP2" \
    --warmup 3 \
    -n 50 2>&1 | tail -5

echo "MPI output saved to: $MPI_OUTPUT_NP2"
echo ""


echo "--- Running MPI sweep ($START -> $END, factor $FACTOR) ---"

MPI_OUTPUT_NP8="$BENCH_RESULTS_DIR/mpi_sweep_np8.json"

mpirun -np 8 "$BENCH" \
    -b mpi \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$MPI_OUTPUT_NP8" \
    --warmup 3 \
    -n 50 2>&1 | tail -5

echo "MPI output saved to: $MPI_OUTPUT_NP8"
echo ""

# ------------------------------------------------------------------
# 2. Run ZMQ sweep
# ------------------------------------------------------------------
# echo "--- Running ZMQ sweep ($START -> $END, factor $FACTOR) ---"

# ZMQ_SERVER_OUTPUT="$BENCH_RESULTS_DIR/zmq_server.json"
# ZMQ_CLIENT_OUTPUT="$BENCH_RESULTS_DIR/zmq_client.json"

# # Start server in background
# "$BENCH" -b zmq --zmq-mode server \
#     --sweep "$START" "$END" "$FACTOR" \
#     --format json \
#     --output "$ZMQ_SERVER_OUTPUT" \
#     --warmup 3 \
#     -n 50 &
# SERVER_PID=$!
# sleep 2

# # Run client and capture output
# "$BENCH" -b zmq --zmq-mode client \
#     --sweep "$START" "$END" "$FACTOR" \
#     --format json \
#     --output "$ZMQ_CLIENT_OUTPUT" \
#     --warmup 3 \
#     -n 50 2>&1 | tail -5

# # Clean up server
# kill "$SERVER_PID" 2>/dev/null || true
# wait "$SERVER_PID" 2>/dev/null || true

echo "ZMQ output saved to: $ZMQ_CLIENT_OUTPUT"
echo ""

echo "--- Running ZMQ sweep inproc ($START -> $END, factor $FACTOR) ---"

ZMQ_INPROC_OUTPUT="$BENCH_RESULTS_DIR/zmq_inproc.json"
"$BENCH" -b zmq -n 1000 \
    --zmq-mode single --zmq-transport inproc --zmq-addr bw-test \
    --sweep "$START" "$END" "$FACTOR" \
    --format json \
    --output "$ZMQ_INPROC_OUTPUT" \
    -n 50 

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
    "$MPI_OUTPUT_NP2" "$MPI_OUTPUT_NP8" "$ZMQ_INPROC_OUTPUT" \
    --output "$PLOT_OUTPUT"

echo ""
echo "=== Done ==="
echo "Results: $BENCH_RESULTS_DIR/"
echo "Plots:   $PLOT_OUTPUT/"
ls "$PLOT_OUTPUT"/
