#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace bt {

enum class Backend { ZMQ, MPI, NONE };

enum class TransferMode { SINGLE, CHUNKED };

enum class MpTopology { PING_PONG, RING, ALL_TO_ALL, STAR };

enum class ZmqMode { SERVER, CLIENT };

struct Config {
    Backend backend = Backend::ZMQ;
    int num_tests = 100;
    std::vector<size_t> message_sizes;  // empty means use single_message_size
    size_t single_message_size = 1024;
    size_t sweep_start = 64;
    size_t sweep_end = 1048576;
    double sweep_factor = 2.0;  // logarithmic step factor
    int warmup = 5;
    std::string output_format = "text";
    std::string output_file;
    bool verbose = false;

    // ZMQ-specific
    std::string zmq_addr = "tcp://localhost:5555";
    std::string zmq_socket_type = "pair";
    std::string zmq_transport = "tcp";
    ZmqMode zmq_mode = ZmqMode::SERVER;  // SERVER or CLIENT

    // MPI-specific
    MpTopology mpi_topology = MpTopology::PING_PONG;
    int mpi_ranks = 0;  // 0 = auto-detect

    // Transfer mode
    TransferMode transfer_mode = TransferMode::SINGLE;
    int chunk_count = 100;
};

}  // namespace bt
