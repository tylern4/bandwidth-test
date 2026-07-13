#pragma once

#include "bandwidth_tester.hpp"

#ifdef HAVE_ZMQ

#include <zmq.hpp>
#include <vector>
#include <string>
#include <memory>

namespace bt {

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
    bool is_server() const override { return mode_ == ZmqMode::SERVER || mode_ == ZmqMode::SINGLE; }
    int rank() const override { return mode_ == ZmqMode::CLIENT ? 1 : 0; }

private:
    std::string zmq_type_string(const std::string& type);
    std::string full_addr(const std::string& addr);

    std::string addr_;
    std::string socket_type_;
    std::string transport_;
    ZmqMode mode_;
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> server_socket_;
    std::unique_ptr<zmq::socket_t> client_socket_;
    std::unique_ptr<zmq::socket_t> socket_;
};

}  // namespace bt

#endif  // HAVE_ZMQ
