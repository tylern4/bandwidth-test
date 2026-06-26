#!/bin/bash
# MPI benchmark test
# Requires mpirun and MPI-enabled build
# Usage: ./scripts/mpi-test.sh [num_ranks] [topology]

set -e

RANKS=${1:-2}
TOPOLOGY=${2:-ping-pong}

echo "=== Bandwidth Test - MPI ==="
echo "Ranks: ${RANKS}"
echo "Topology: ${TOPOLOGY}"
echo ""

mpirun -np "$RANKS" ./bandwidth-test -b mpi --sweep 64 1048576 2.0 --mpi-topology "$TOPOLOGY" -n 50 --warmup 5 --verbose
