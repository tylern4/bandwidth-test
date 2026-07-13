# Bandwidth Test - Usage Guide

## Overview

`bandwidth-test` measures network bandwidth between processes using either ZeroMQ (ZMQ) or MPI backends. It supports single-message and chunked transfer modes, multiple MPI topologies, and output in text, CSV, or JSON formats.

## Building

```bash
mkdir build && cd build
cmake .. -DBT_BUILD_TESTS=ON
make -j8
```

### Dependencies

- C++17 compiler (GCC 9+, Clang 10+, AppleClang 12+)
- ZeroMQ 4.x (libzmq) - auto-fetched if not found on system
- MPI (OpenMPI, MPICH) - optional
- clipp (header-only CLI parser) - auto-fetched

### Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `BT_BUILD_TESTS` | Build unit tests | OFF |
| `CMAKE_BUILD_TYPE` | Build type (Debug/Release) | Release |

## Usage

### ZMQ Backend (Default)

The ZMQ backend uses a client-server model. Run the server first, then the client:

```bash
# Terminal 1 - Server
./bandwidth-test -b zmq --zmq-mode server -s 1024 -n 1000

# Terminal 2 - Client
./bandwidth-test -b zmq --zmq-mode client -s 1024 -n 1000
```

For `inproc` transport, run both in a single process:

```bash
./bandwidth-test -b zmq --zmq-mode single --zmq-transport inproc --zmq-addr bw-test -s 1024 -n 1000
```

### MPI Backend

Run with mpirun/mpirun:

```bash
mpirun -np 2 ./bandwidth-test -b mpi --sweep 64 1048576 2.0
```

## Command-Line Options

### General Options

| Option | Description | Default |
|--------|-------------|---------|
| `-n, --num-tests N` | Number of benchmark iterations per message size | 100 |
| `-t, --warmup N` | Warmup iterations (excluded from stats) | 5 |
| `-v, --verbose` | Verbose per-iteration output | off |
| `--format FMT` | Output format: text, csv, json | text |
| `--output FILE` | Output file (default: stdout) | stdout |

### Message Sizes

Choose one:

| Option | Description | Example |
|--------|-------------|---------|
| `-s, --message-size SIZE` | Single message size in bytes | `-s 1024` |
| `--sizes LIST` | Comma-separated list of sizes | `--sizes 64,256,1024,4096` |
| `--sweep MIN MAX FACT` | Logarithmic sweep | `--sweep 64 1048576 2.0` |

### Transfer Mode

| Option | Description | Default |
|--------|-------------|---------|
| `--transfer-mode MODE` | `single` or `chunked` | single |
| `--chunk-count N` | Number of chunks in chunked mode | 100 |

In chunked mode, the total message is split into N smaller chunks sent sequentially.

### Backend Selection

| Option | Description | Default |
|--------|-------------|---------|
| `-b, --backend BACKEND` | `zmq` or `mpi` | zmq |

### ZMQ Options

| Option | Description | Default |
|--------|-------------|---------|
| `--zmq-addr ADDR` | ZMQ address | `tcp://localhost:5555` |
| `--zmq-type TYPE` | Socket type: pair, push/pull, req/rep | pair |
| `--zmq-transport TP` | Transport: tcp, ipc, inproc | tcp |
| `--zmq-mode MODE` | server, client, or single | server |

#### Transports

| Transport | Description | Address Format | Process Model |
|-----------|-------------|----------------|---------------|
| `tcp` (default) | TCP/IP networking | Full address: `tcp://localhost:5555` | Two processes (server + client) |
| `ipc` | Inter-process communication via Unix domain sockets | Path only: `/tmp/bw-test` | Two processes (server + client) |
| `inproc` | In-process communication (fastest, no network overhead) | Name only: `myaddr` | Single process (`--zmq-mode single`) |

**Note**: `ipc` is faster than `tcp` for local communication as it bypasses the TCP stack. `inproc` is the fastest option but requires running server and client in the same process.

#### ZMQ Mode

| Mode | Description | Use Case |
|------|-------------|----------|
| `server` | Run as server (waits for client to connect) | Server process |
| `client` | Run as client (connects to server) | Client process |
| `single` | Run both server and client in one process | `inproc` transport only |

### MPI Options

| Option | Description | Default |
|--------|-------------|---------|
| `--mpi-topology TOPO` | ping-pong, ring, all-to-all, star | ping-pong |
| `--mpi-ranks N` | Number of MPI ranks (0 = auto-detect) | 0 |

## Output Formats

### Text (default)

```
=============================================================================
  Bandwidth Benchmark Results
=============================================================================

  Size        Mean MB/s     Min MB/s    Med lat    P95 lat    P99 lat    Stddev
  --------  ------------  ------------  ----------  ----------  ----------  ----------
  1KB              1234.56       1100.23      0.83      0.91      0.95      0.05
  4KB              3456.78       3200.11      0.23      0.25      0.28      0.02

  Message size: 1KB
  Bandwidth:    1234.56 MB/s (min=1100.23, max=1350.45)
  Latency:      median=0.83us P50=0.83us P95=0.91us P99=0.95us
  Std dev:      0.05us
  Samples:      100
```

### CSV

```csv
message_size,mean_bandwidth_mbps,min_bandwidth_mbps,max_bandwidth_mbps,median_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,stddev_latency_us,num_samples
1024,1234.56,1100.23,1350.45,0.83,0.83,0.91,0.95,0.05,100
```

### JSON

```json
{
  "results": [
    {
      "message_size": 1024,
      "mean_bandwidth_mbps": 1234.56,
      "min_bandwidth_mbps": 1100.23,
      "max_bandwidth_mbps": 1350.45,
      "median_latency_us": 0.83,
      "p50_latency_us": 0.83,
      "p95_latency_us": 0.91,
      "p99_latency_us": 0.95,
      "stddev_latency_us": 0.05,
      "num_samples": 100
    }
  ]
}
```

## Examples

### Single message size test (ZMQ)

```bash
# Server
./bandwidth-test -b zmq --zmq-mode server -s 1024 -n 1000 --verbose

# Client
./bandwidth-test -b zmq --zmq-mode client -s 1024 -n 1000
```

### Size sweep (MPI)

```bash
mpirun -np 2 ./bandwidth-test -b mpi --sweep 64 1048576 2.0 --mpi-topology ring --verbose
```

### Chunked transfer (ZMQ)

```bash
# Server
./bandwidth-test -b zmq --zmq-mode server -s 1048576 --transfer-mode chunked --chunk-count 100

# Client
./bandwidth-test -b zmq --zmq-mode client -s 1048576 --transfer-mode chunked --chunk-count 100
```

### CSV output

```bash
./bandwidth-test -b zmq --zmq-mode server -s 1024 --format csv --output results.csv &
./bandwidth-test -b zmq --zmq-mode client -s 1024
```

### IPC transport (ZMQ, faster than TCP for local)

```bash
# Terminal 1 - Server
./bandwidth-test -b zmq --zmq-mode server --zmq-transport ipc --zmq-addr /tmp/bw-test

# Terminal 2 - Client
./bandwidth-test -b zmq --zmq-mode client --zmq-transport ipc --zmq-addr /tmp/bw-test
```

### inproc transport (fastest, single process)

```bash
./bandwidth-test -b zmq --zmq-mode single --zmq-transport inproc --zmq-addr bw-test -s 1024 -n 1000
```

### All MPI topologies

```bash
# Ring topology
mpirun -np 4 ./bandwidth-test -b mpi --message-size 4096 --mpi-topology ring

# All-to-all
mpirun -np 4 ./bandwidth-test -b mpi --message-size 4096 --mpi-topology all-to-all

# Star topology
mpirun -np 5 ./bandwidth-test -b mpi --message-size 4096 --mpi-topology star
```

## Metrics Explained

| Metric | Description |
|--------|-------------|
| Mean MB/s | Average bandwidth across all iterations |
| Min MB/s | Lowest observed bandwidth |
| Max MB/s | Highest observed bandwidth |
| Median latency | 50th percentile latency (microseconds) |
| P95/P99 latency | 95th/99th percentile latency |
| Stddev | Standard deviation of latency |

## Testing

Run unit tests:

```bash
cmake .. -DBT_BUILD_TESTS=ON
make bandwidth-test-tests
./bandwidth-test-tests
```

Run with verbose output:

```bash
./bandwidth-test-tests --success
```
