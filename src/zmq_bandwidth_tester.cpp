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
    ) / 1000.0;
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
    return addr;
}

bool ZmqTester::setup() {
    context_ = std::make_unique<zmq::context_t>(1);
    auto type = to_socket_type(socket_type_);
    socket_ = std::make_unique<zmq::socket_t>(*context_, type);

    std::string fa = full_addr(addr_);

    if (mode_ == ZmqMode::SERVER) {
        socket_->bind(fa);

        if (socket_type_ == "pair") {
            zmq::pollitem_t items[] = { { socket_->handle(), 0, ZMQ_POLLIN, 0 } };
            auto dur = std::chrono::milliseconds(10000);
            int rc = zmq::poll(items, 1, dur);
            if (rc <= 0) {
                std::cerr << "ZMQ server: timeout waiting for client\n";
                return false;
            }
            zmq::message_t msg;
            socket_->recv(msg, zmq::recv_flags::none);
        }
    } else {
        socket_->set(zmq::sockopt::connect_timeout, 10000);
        socket_->connect(fa);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

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
    if (mode == TransferMode::CHUNKED) {
        size_t chunk_size = message_size / static_cast<size_t>(chunk_count);
        if (chunk_size == 0) chunk_size = 1;
        double bytes_per_rt = 2.0 * static_cast<double>(message_size);

        auto do_chunked = [this, chunk_size, chunk_count, message_size](size_t /*idx*/) -> double {
            std::vector<char> buf(message_size);
            std::memset(buf.data(), static_cast<char>(message_size & 0xFF), message_size);

            auto start = chrono_clock();

            if (mode_ == ZmqMode::SERVER) {
                size_t remaining = message_size;
                const char* data = buf.data();
                while (remaining > 0) {
                    size_t to_send = std::min(chunk_size, remaining);
                    zmq::message_t send_msg(to_send);
                    std::memcpy(send_msg.data(), data, to_send);
                    socket_->send(send_msg, zmq::send_flags::none);
                    data += to_send;
                    remaining -= to_send;
                }

                remaining = message_size;
                while (remaining > 0) {
                    size_t to_recv = std::min(chunk_size, remaining);
                    zmq::message_t recv_msg;
                    socket_->recv(recv_msg, zmq::recv_flags::none);
                    size_t actual = recv_msg.size();
                    if (actual > to_recv) actual = to_recv;
                    remaining -= actual;
                }
            } else {
                size_t remaining = message_size;
                while (remaining > 0) {
                    size_t to_recv = std::min(chunk_size, remaining);
                    zmq::message_t recv_msg;
                    socket_->recv(recv_msg, zmq::recv_flags::none);
                    size_t actual = recv_msg.size();
                    if (actual > to_recv) actual = to_recv;
                    remaining -= actual;
                }

                remaining = message_size;
                const char* data = buf.data();
                while (remaining > 0) {
                    size_t to_send = std::min(chunk_size, remaining);
                    zmq::message_t send_msg(to_send);
                    std::memcpy(send_msg.data(), data, to_send);
                    socket_->send(send_msg, zmq::send_flags::none);
                    data += to_send;
                    remaining -= to_send;
                }
            }

            auto end = chrono_clock();
            return chrono_elapsed(start);
        };

        std::vector<double> latencies;
        latencies = run_benchmark(warmup, num_tests, verbose, message_size, do_chunked);
        return compute_statistics(message_size, latencies, bytes_per_rt);
    }

    std::vector<double> latencies;

    auto do_ping_pong = [this, message_size](size_t /*idx*/) -> double {
        std::vector<char> buf(message_size);
        std::memset(buf.data(), static_cast<char>(message_size & 0xFF), message_size);

        auto start = chrono_clock();

        if (mode_ == ZmqMode::SERVER) {
            zmq::message_t send_msg(message_size);
            std::memcpy(send_msg.data(), buf.data(), message_size);
            socket_->send(send_msg, zmq::send_flags::none);

            zmq::message_t recv_msg;
            socket_->recv(recv_msg, zmq::recv_flags::none);
        } else {
            zmq::message_t recv_msg;
            socket_->recv(recv_msg, zmq::recv_flags::none);

            zmq::message_t send_msg(message_size);
            std::memcpy(send_msg.data(), buf.data(), message_size);
            socket_->send(send_msg, zmq::send_flags::none);
        }

        auto end = chrono_clock();
        return chrono_elapsed(start);
    };

    latencies = run_benchmark(warmup, num_tests, verbose, message_size, do_ping_pong);

    double bytes_per_rt = 2.0 * static_cast<double>(message_size);
    return compute_statistics(message_size, latencies, bytes_per_rt);
}

void ZmqTester::cleanup() {
    socket_.reset();
    context_.reset();
}

}  // namespace bt

#endif  // HAVE_ZMQ
