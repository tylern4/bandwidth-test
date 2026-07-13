#pragma once

#include "bandwidth_tester.hpp"

#ifdef HAVE_ZMQ

#include <zmq.hpp>
#include <vector>
#include <string>
#include <memory>
#include <chrono>

namespace bt {

// ZMQ timing constants
constexpr auto ZMQ_CONNECT_SETTLE_TIME = std::chrono::milliseconds(200);
constexpr auto ZMQ_SERVER_TIMEOUT = std::chrono::milliseconds(10000);
constexpr int ZMQ_CONNECT_TIMEOUT_MS = 10000;

class ZmqTester : public BandwidthTester {
public:
    ZmqTester(const std::string& addr,
              const std::string& socket_type,
              const std::string& transport,
              ZmqMode mode);

    bool setup() override;
    TestResult run_single(size_t message_size,
                          int num_tests,
                          int warmup,
                          bool verbose,
                          TransferMode mode = TransferMode::SINGLE,
                          int chunk_count = 100) override;
    void cleanup() override;
    std::string name() const override { return "ZMQ"; }
    bool is_leader() const override { return mode_ == ZmqMode::SERVER || mode_ == ZmqMode::SINGLE; }
    int rank() const override { return mode_ == ZmqMode::CLIENT ? 1 : 0; }

private:
    std::string full_addr(const std::string& addr);
    zmq::socket_t& server_sock();
    zmq::socket_t& client_sock();
    zmq::socket_t& active_sock();
    
    void send_data(zmq::socket_t& sock, const char* data, size_t size);
    void recv_data(zmq::socket_t& sock, char* data, size_t size);
    void send_chunked(zmq::socket_t& sock, const char* data, size_t total_size, size_t chunk_size);
    void recv_chunked(zmq::socket_t& sock, char* data, size_t total_size, size_t chunk_size);

    std::string addr_;
    std::string socket_type_;
    std::string transport_;
    ZmqMode mode_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> server_socket_;
    std::unique_ptr<zmq::socket_t> client_socket_;
    std::unique_ptr<zmq::socket_t> socket_;
    
    // Pre-allocated buffer for reuse across iterations
    std::vector<char> send_buffer_;
    std::vector<char> recv_buffer_;
};

}  // namespace bt

#endif  // HAVE_ZMQ
