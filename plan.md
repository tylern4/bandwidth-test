# Bandwidth Test Program - Plan

## Overview

A modern C++ program to measure network bandwidth between processes/nodes using either:
- **ZeroMQ** (ZMQ) - for TCP/IPC transports
- **MPI** - for HPC/cluster environments (OpenMPI/MPICH)

## Architecture

```
bandwidth-test/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # Entry point + CLI parsing with clipp
│   ├── cli.hpp / cli.cpp     # Command-line interface definitions
│   ├── bandwidth_tester.hpp  # Abstract interface
│   ├── zmq_bandwidth_tester.hpp / .cpp  # ZMQ backend
│   ├── mpi_bandwidth_tester.hpp / .cpp  # MPI backend
│   ├── benchmark.hpp / .cpp  # Core benchmarking logic (shared)
│   └── output.hpp / .cpp     # Result formatting/output
├── tests/
│   └── test_*.cpp
├── deps/                       # Dependencies (clipp, ZMQ headers, etc.)
├── docs/
│   └── usage.md
└── README.md
```

## CLI Options (via clipp)

### General
| Option | Description | Default |
|--------|-------------|---------|
| `--backend` | Transport backend: `zmq` or `mpi` | `zmq` |
| `--num-tests` | Number of iterations/rounds per message size | 100 |
| `--message-size` | Single message size in bytes | 1024 |
| `--message-sizes` | Comma-separated list of message sizes for sweep | (none) |
| `--size-range` | Sweep: `min,max,step` e.g. `64,1048576,1024` | (none) |
| `--worker` | Worker rank (MPI) or worker index (ZMQ) | 0 |

### ZMQ-specific
| Option | Description | Default |
|--------|-------------|---------|
| `--zmq-addr` | ZMQ connect address | `tcp://localhost:5555` |
| `--zmq-type` | ZMQ socket type (`pair`, `push/pull`, `req/rep`) | `pair` |
| `--zmq-transport` | Transport protocol (`tcp`, `ipc`, `inproc`) | `tcp` |

### MPI-specific
| Option | Description | Default |
|--------|-------------|---------|
| `--mpi-topology` | Communication pattern: `ping-pong`, `ring`, `all-to-all`, `star` | `ping-pong` |
| `--mpi-ranks` | Number of MPI ranks | (auto-detect) |
| `--mpi-comm` | MPI communicator name | `MPI_COMM_WORLD` |

### Output
| Option | Description | Default |
|--------|-------------|---------|
| `--format` | Output format: `text`, `csv`, `json` | `text` |
| `--output` | Output file path | stdout |
| `--warmup` | Warmup iterations (excluded from stats) | 5 |
| `--verbose` | Verbose output per iteration | off |
| `--transfer-mode` | How to send data: `single` (one large msg) or `chunked` (N smaller msgs) | `single` |
| `--chunk-count` | Number of chunks in chunked mode | 100 |

## Benchmark Methodology

### Test Pattern: Ping-Pong (Bidirectional)
1. **Sender** sends a message to **Receiver**
2. **Receiver** echoes it back
3. Measure round-trip time (RTT)
4. Bandwidth = (2 × message_size) / RTT

### Metrics Collected Per Message Size
- Mean bandwidth (MB/s)
- Min/Max bandwidth (MB/s)
- Median latency (μs)
- P50/P95/P99 latency percentiles (μs)
- Standard deviation

### Sweep Mode
When `--size-range` or `--message-sizes` is specified:
- Sizes are always generated logarithmically (powers of ~2): e.g., 64, 256, 1K, 4K, 16K, 64K, 256K, 1M, 4M, 16M
- Run full benchmark at each size
- Generate comparison table across all sizes
- Output aggregated summary

## Implementation Steps

### Phase 1: Project Setup
- [x] Create CMakeLists.txt with options for ZMQ/MPI
- [x] Fetch clipp (header-only, trivial dependency) via FetchContent
- [x] Set up build system with backend selection flag
- [x] Create basic project structure
- [x] Fetch cppzmq for C++ bindings

### Phase 2: CLI Layer
- [x] Implement clipp-based CLI parser
- [x] Validate inputs (e.g., message sizes > 0, valid backend)
- [x] Handle help/error messages
- [x] Parse message size arguments (single, list, range)
- [x] Add `--transfer-mode` (single/chunked) and `--chunk-count`
- [x] Add `--mpi-topology` (ping-pong/ring/all-to-all/star)

### Phase 3: Benchmark Engine (Backend-Agnostic)
- [x] Define abstract `BandwidthTester` interface
- [x] Implement benchmark runner with warmup + N iterations
- [x] Implement statistics calculator (mean, percentiles, etc.)
- [x] Implement output formatters (text, CSV, JSON)

### Phase 4: ZMQ Backend
- [x] Implement ZMQ pair socket setup (server/client roles)
- [x] Implement send/recv with byte-sized messages
- [x] Wire into benchmark engine
- [x] Support TCP transport
- [x] Use cppzmq C++ API

### Phase 5: MPI Backend
- [x] Implement MPI communicator-based ping-pong
- [x] Use `MPI_Send`/`MPI_Recv`/`MPI_Sendrecv`
- [x] Support multiple rank patterns (ping-pong, ring, all-to-all, star)
- [x] Wire into benchmark engine

### Phase 6: Integration & Polish
- [x] Add error handling for both backends
- [x] Add unit tests for benchmark engine
- [x] Write usage documentation
- [x] Add example scripts
- [x] Implement chunked transfer mode

## Resolved Decisions

1. ✅ **Sweep scaling**: Logarithmic (powers of ~2)
2. ✅ **MPI topologies**: Ping-pong, ring, all-to-all, star
3. ✅ **Transfer mode**: Both `single` (one large message) and `chunked` (N smaller messages)
4. ✅ **Build system**: CMake with smart detection — build both backends if available; if only MPI found, auto-fetch+build ZMQ via FetchContent; if only ZMQ found, skip MPI entirely
