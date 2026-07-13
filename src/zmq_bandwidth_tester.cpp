#include "zmq_bandwidth_tester.hpp"

#ifdef HAVE_ZMQ

#include "benchmark.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <functional>

namespace bt {

namespace {

auto chrono_clock() {
    return std::chrono::high_resolution_clock::now();
}

double chrono_elapsed(std::chrono::high_resolution_clock::time_point start) {
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count()
    ) / 1000.0;  // Convert to microseconds
}

zmq::socket_type to_socket_type(const std::string& type) {
    if (type == "pair") return zmq::socket_type::pair;
    if (type == "push") return zmq::socket_type::push;
    if (type == "pull") return zmq::socket_type::pull;
    if (type == "req")  return zmq::socket_type::req;
    if (type == "rep")  return zmq::socket_type::rep;
    return zmq::socket_type::pair;
}

}  // namespace

ZmqTester::ZmqTester(const std::string& addr,
                     const std::string& socket_type,
                     const std::string& transport,
                     ZmqMode mode)
    : addr_(addr), socket_type_(socket_type), transport_(transport), mode_(mode) {
}

std::string ZmqTester::full_addr(const std::string& addr) {
    if (transport_ == "ipc") {
        return "ipc://" + addr;
    }
    if (transport_ == "inproc") {
        return "inproc://" + addr;
    }
    return addr;
}

zmq::socket_t& ZmqTester::server_sock() {
    if (socket_) return *socket_;
    return *server_socket_;
}

zmq::socket_t& ZmqTester::client_sock() {
    if (socket_) return *socket_;
    return *client_socket_;
}

zmq::socket_t& ZmqTester::active_sock() {
    if (mode_ == ZmqMode::SERVER) return *socket_;
    if (mode_ == ZmqMode::CLIENT) return *socket_;
    return *server_socket_;  // SINGLE mode uses server socket as primary
}

void ZmqTester::send_data(zmq::socket_t& sock, const char* data, size_t size) {
    zmq::message_t msg(size);
    std::memcpy(msg.data(), data, size);
    sock.send(msg, zmq::send_flags::none);
}

void ZmqTester::recv_data(zmq::socket_t& sock, char* data, size_t size) {
    zmq::message_t msg;
    auto result = sock.recv(msg, zmq::recv_flags::none);
    if (!result) {
        std::cerr << "ZMQ recv failed\n";
        return;
    }
    size_t actual = msg.size();
    if (actual > size) actual = size;
    std::memcpy(data, msg.data(), actual);
}

void ZmqTester::send_chunked(zmq::socket_t& sock, const char* data, 
                               size_t total_size, size_t chunk_size) {
    size_t remaining = total_size;
    const char* ptr = data;
    while (remaining > 0) {
        size_t to_send = std::min(chunk_size, remaining);
        send_data(sock, ptr, to_send);
        ptr += to_send;
        remaining -= to_send;
    }
}

void ZmqTester::recv_chunked(zmq::socket_t& sock, char* data, 
                               size_t total_size, size_t chunk_size) {
    size_t remaining = total_size;
    char* ptr = data;
    while (remaining > 0) {
        size_t to_recv = std::min(chunk_size, remaining);
        zmq::message_t msg;
        auto result = sock.recv(msg, zmq::recv_flags::none);
        if (!result) {
            std::cerr << "ZMQ recv_chunked failed\n";
            break;
        }
        size_t actual = msg.size();
        if (actual > to_recv) actual = to_recv;
        std::memcpy(ptr, msg.data(), actual);
        ptr += actual;
        remaining -= actual;
    }
}

bool ZmqTester::setup() {
    context_ = std::make_unique<zmq::context_t>(1);
    auto type = to_socket_type(socket_type_);
    std::string fa = full_addr(addr_);

    if (mode_ == ZmqMode::SINGLE) {
        server_socket_ = std::make_unique<zmq::socket_t>(*context_, type);
        client_socket_ = std::make_unique<zmq::socket_t>(*context_, type);

        server_socket_->bind(fa);
        client_socket_->set(zmq::sockopt::connect_timeout, ZMQ_CONNECT_TIMEOUT_MS);
        client_socket_->connect(fa);
        std::this_thread::sleep_for(ZMQ_CONNECT_SETTLE_TIME);

        if (socket_type_ == "pair") {
            // Handshake to ensure connection is established
            zmq::message_t msg(1);
            std::memset(msg.data(), 1, 1);
            client_socket_->send(msg, zmq::send_flags::none);

            zmq::message_t recv_msg;
            auto result = server_socket_->recv(recv_msg, zmq::recv_flags::none);
            if (!result) {
                std::cerr << "ZMQ single mode: failed to receive handshake response\n";
                return false;
            }
        }
    } else if (mode_ == ZmqMode::SERVER) {
        socket_ = std::make_unique<zmq::socket_t>(*context_, type);
        socket_->bind(fa);

        if (socket_type_ == "pair") {
            zmq::pollitem_t items[] = { { socket_->handle(), 0, ZMQ_POLLIN, 0 } };
            int rc = zmq::poll(items, 1, ZMQ_SERVER_TIMEOUT);
            if (rc <= 0) {
                std::cerr << "ZMQ server: timeout waiting for client\n";
                return false;
            }
            zmq::message_t msg;
            auto result = socket_->recv(msg, zmq::recv_flags::none);
            if (!result) {
                std::cerr << "ZMQ server: failed to receive handshake\n";
                return false;
            }
        }
    } else {
        socket_ = std::make_unique<zmq::socket_t>(*context_, type);
        socket_->set(zmq::sockopt::connect_timeout, ZMQ_CONNECT_TIMEOUT_MS);
        socket_->connect(fa);
        std::this_thread::sleep_for(ZMQ_CONNECT_SETTLE_TIME);

        if (socket_type_ == "pair") {
            zmq::message_t msg(1);
            std::memset(msg.data(), 1, 1);
            socket_->send(msg, zmq::send_flags::none);
        }
    }

    return true;
}

TestResult ZmqTester::run_single(size_t message_size,
                                   int num_tests,
                                   int warmup,
                                   bool verbose,
                                   TransferMode mode,
                                   int chunk_count)
{
    // Ensure buffers are large enough
    if (send_buffer_.size() < message_size) {
        send_buffer_.resize(message_size);
    }
    if (recv_buffer_.size() < message_size) {
        recv_buffer_.resize(message_size);
    }
    
    // Fill send buffer with pattern data
    std::fill(send_buffer_.begin(), send_buffer_.begin() + message_size, 
              static_cast<char>(message_size & 0xFF));

    if (mode == TransferMode::CHUNKED) {
        size_t chunk_size = message_size / static_cast<size_t>(chunk_count);
        if (chunk_size == 0) chunk_size = 1;
        double bytes_per_rt = 2.0 * static_cast<double>(message_size);

        auto do_chunked = [&](size_t /*idx*/) -> double {
            auto start = chrono_clock();

            if (mode_ == ZmqMode::SINGLE) {
                // Full ping-pong in single process using two sockets
                // Server sends chunks to client
                send_chunked(server_sock(), send_buffer_.data(), message_size, chunk_size);
                // Client receives chunks from server
                recv_chunked(client_sock(), recv_buffer_.data(), message_size, chunk_size);
                // Client sends chunks back to server
                send_chunked(client_sock(), send_buffer_.data(), message_size, chunk_size);
                // Server receives chunks from client
                recv_chunked(server_sock(), recv_buffer_.data(), message_size, chunk_size);
            } else if (mode_ == ZmqMode::SERVER) {
                // Server sends, waits for echo
                send_chunked(active_sock(), send_buffer_.data(), message_size, chunk_size);
                recv_chunked(active_sock(), recv_buffer_.data(), message_size, chunk_size);
            } else {
                // Client waits for data, echoes back
                recv_chunked(active_sock(), recv_buffer_.data(), message_size, chunk_size);
                send_chunked(active_sock(), send_buffer_.data(), message_size, chunk_size);
            }

            auto end = chrono_clock();
            return chrono_elapsed(start);
        };

        std::vector<double> latencies;
        latencies = run_benchmark(warmup, num_tests, verbose, message_size, do_chunked);
        return compute_statistics(message_size, latencies, bytes_per_rt);
    }

    // Single message mode (ping-pong)
    auto do_ping_pong = [&](size_t /*idx*/) -> double {
        auto start = chrono_clock();

        if (mode_ == ZmqMode::SINGLE) {
            // Full ping-pong in single process
            send_data(server_sock(), send_buffer_.data(), message_size);
            recv_data(client_sock(), recv_buffer_.data(), message_size);
            send_data(client_sock(), send_buffer_.data(), message_size);
            recv_data(server_sock(), recv_buffer_.data(), message_size);
        } else if (mode_ == ZmqMode::SERVER) {
            // Server sends, waits for echo
            send_data(active_sock(), send_buffer_.data(), message_size);
            recv_data(active_sock(), recv_buffer_.data(), message_size);
        } else {
            // Client waits for data, echoes back
            recv_data(active_sock(), recv_buffer_.data(), message_size);
            send_data(active_sock(), send_buffer_.data(), message_size);
        }

        auto end = chrono_clock();
        return chrono_elapsed(start);
    };

    std::vector<double> latencies = run_benchmark(warmup, num_tests, verbose, message_size, do_ping_pong);

    double bytes_per_rt = 2.0 * static_cast<double>(message_size);
    return compute_statistics(message_size, latencies, bytes_per_rt);
}

void ZmqTester::cleanup() {
    server_socket_.reset();
    client_socket_.reset();
    socket_.reset();
    context_.reset();
    send_buffer_.clear();
    send_buffer_.shrink_to_fit();
    recv_buffer_.clear();
    recv_buffer_.shrink_to_fit();
}

}  // namespace bt

#endif  // HAVE_ZMQ
