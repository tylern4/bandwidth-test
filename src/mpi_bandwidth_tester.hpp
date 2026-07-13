#pragma once

#include "bandwidth_tester.hpp"

#ifdef HAVE_MPI

#include <mpi.h>
#include <vector>
#include <string>
#include <memory>

namespace bt {

// MPI buffer constants
constexpr size_t MPI_INITIAL_BUFFER_SIZE = 65536;

class MpTester : public BandwidthTester {
public:
    explicit MpTester(MpTopology topology, int num_ranks = 0);
    ~MpTester() override = default;

    bool setup() override;
    TestResult run_single(size_t message_size,
                          int num_tests,
                          int warmup,
                          bool verbose,
                          TransferMode mode = TransferMode::SINGLE,
                          int chunk_count = 100) override;
    void cleanup() override;
    std::string name() const override { return "MPI"; }
    bool is_leader() const override;
    int rank() const override { return world_rank_; }

private:
    void compute_ping_pong_latency(size_t msg_size,
                                    std::vector<double>& latencies,
                                    int num_tests);
    void compute_ring_latency(size_t msg_size,
                              std::vector<double>& latencies,
                              int num_tests);
    void compute_all_to_all_latency(size_t msg_size,
                                     std::vector<double>& latencies,
                                     int num_tests);
    void compute_star_latency(size_t msg_size,
                               std::vector<double>& latencies,
                               int num_tests);

    MpTopology topology_;
    int world_size_ = 1;
    int world_rank_ = 0;
    std::unique_ptr<std::vector<char>> send_buf_;
    std::unique_ptr<std::vector<char>> recv_buf_;
};

}  // namespace bt

#endif  // HAVE_MPI
