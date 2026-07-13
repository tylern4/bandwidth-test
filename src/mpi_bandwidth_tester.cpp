#include "mpi_bandwidth_tester.hpp"

#ifdef HAVE_MPI

#include "benchmark.hpp"
#include <iostream>
#include <cstring>
#include <functional>
#include <algorithm>

namespace bt {

namespace {

double mpi_wtime() {
    return MPI_Wtime() * 1e6;  // Convert seconds to microseconds
}

}  // namespace

MpTester::MpTester(MpTopology topology, int num_ranks)
    : topology_(topology) {
    // MPI is already initialized by MPI_Init in main.cpp
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank_);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size_);
    if (num_ranks > 0) {
        world_size_ = num_ranks;
    }
}

bool MpTester::setup() {
    // Allocate buffers using unique_ptr (will grow as needed)
    send_buf_ = std::make_unique<std::vector<char>>(MPI_INITIAL_BUFFER_SIZE);
    recv_buf_ = std::make_unique<std::vector<char>>(MPI_INITIAL_BUFFER_SIZE);

    // MPI barrier to synchronize all ranks
    MPI_Barrier(MPI_COMM_WORLD);
    return true;
}

TestResult MpTester::run_single(size_t message_size,
                                   int num_tests,
                                   int warmup,
                                   bool verbose,
                                   TransferMode mode,
                                   int chunk_count)
{
    if (message_size > send_buf_->size()) {
        send_buf_->resize(message_size);
    }
    if (message_size > recv_buf_->size()) {
        recv_buf_->resize(message_size);
    }

    if (mode == TransferMode::CHUNKED) {
        size_t chunk_size = message_size / static_cast<size_t>(chunk_count);
        if (chunk_size == 0) chunk_size = 1;
        double bytes_per_rt = 2.0 * static_cast<double>(message_size);

        // Warmup
        for (int w = 0; w < warmup; ++w) {
            if (world_rank_ == 0) {
                for (int c = 0; c < chunk_count; ++c) {
                    MPI_Send(send_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                    MPI_Recv(recv_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
            } else if (world_rank_ == 1) {
                for (int c = 0; c < chunk_count; ++c) {
                    MPI_Recv(recv_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Send(send_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
                }
            }
        }

        // Measured iterations - collect per-iteration latencies
        std::vector<double> my_latencies;
        my_latencies.reserve(num_tests);

        for (int i = 0; i < num_tests; ++i) {
            auto start = mpi_wtime();

            if (world_rank_ == 0) {
                for (int c = 0; c < chunk_count; ++c) {
                    MPI_Send(send_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                    MPI_Recv(recv_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
            } else if (world_rank_ == 1) {
                for (int c = 0; c < chunk_count; ++c) {
                    MPI_Recv(recv_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Send(send_buf_->data(), static_cast<int>(chunk_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
                }
            }

            auto end = mpi_wtime();
            double latency_us = end - start;  // Already in microseconds
            my_latencies.push_back(latency_us);
        }

        // Gather latencies from all ranks to rank 0
        std::vector<double> all_latencies;
        
        if (world_rank_ == 0) {
            all_latencies = my_latencies;
            
            // Gather from other ranks
            int size = world_size_;
            for (int i = 1; i < size; ++i) {
                int count = 0;
                MPI_Recv(&count, 1, MPI_INT, i, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if (count > 0) {
                    std::vector<double> remote_lats(count);
                    MPI_Recv(remote_lats.data(), count, MPI_DOUBLE, i, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    all_latencies.insert(all_latencies.end(), remote_lats.begin(), remote_lats.end());
                }
            }
        } else {
            // Send latencies to rank 0
            int count = static_cast<int>(my_latencies.size());
            MPI_Send(&count, 1, MPI_INT, 0, 100, MPI_COMM_WORLD);
            if (count > 0) {
                MPI_Send(my_latencies.data(), count, MPI_DOUBLE, 0, 101, MPI_COMM_WORLD);
            }
            
            // Return empty result for non-rank-0
            return compute_statistics(message_size, std::vector<double>(), bytes_per_rt);
        }

        return compute_statistics(message_size, all_latencies, bytes_per_rt);
    }

    std::vector<double> latencies;

    switch (topology_) {
        case MpTopology::PING_PONG:
            compute_ping_pong_latency(message_size, latencies, num_tests);
            break;
        case MpTopology::RING:
            compute_ring_latency(message_size, latencies, num_tests);
            break;
        case MpTopology::ALL_TO_ALL:
            compute_all_to_all_latency(message_size, latencies, num_tests);
            break;
        case MpTopology::STAR:
            compute_star_latency(message_size, latencies, num_tests);
            break;
        default:
            break;
    }

    double bytes_per_rt;
    switch (topology_) {
        case MpTopology::PING_PONG:
            bytes_per_rt = 2.0 * static_cast<double>(message_size);
            break;
        case MpTopology::RING:
            bytes_per_rt = static_cast<double>(message_size);
            break;
        case MpTopology::ALL_TO_ALL:
            bytes_per_rt = static_cast<double>(message_size) * (world_size_ - 1);
            break;
        case MpTopology::STAR:
            bytes_per_rt = 2.0 * static_cast<double>(message_size) * (world_size_ - 1);
            break;
        default:
            bytes_per_rt = 2.0 * static_cast<double>(message_size);
            break;
    }

    return compute_statistics(message_size, latencies, bytes_per_rt);
}

void MpTester::compute_ping_pong_latency(size_t msg_size,
                                          std::vector<double>& latencies,
                                          int num_tests)
{
    latencies.reserve(num_tests);

    // Only rank 0 and rank 1 participate in ping-pong
    // Rank 0 = sender, Rank 1 = receiver/echo
    if (world_rank_ == 0) {
        std::vector<double> my_lats;
        my_lats.reserve(num_tests);

        for (int i = 0; i < num_tests; ++i) {
            // Rank 0 sends to rank 1
            MPI_Send(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD);

            // Rank 1 echoes back to rank 0
            MPI_Recv(recv_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            auto start = mpi_wtime();

            // Measure round trip: 0->1->0
            MPI_Send(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 1, 1, MPI_COMM_WORLD);
            MPI_Recv(recv_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            auto end = mpi_wtime();
            double latency = (end - start) / 2.0;  // One-way latency
            my_lats.push_back(latency);
        }

        latencies = std::move(my_lats);
    }
    else if (world_rank_ == 1) {
        for (int i = 0; i < num_tests; ++i) {
            // Receive from rank 0 (first exchange, non-measured warmup for sync)
            MPI_Recv(recv_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Echo back to rank 0
            MPI_Send(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 0, 0, MPI_COMM_WORLD);

            // Receive measured ping
            MPI_Recv(recv_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Echo back
            MPI_Send(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE, 0, 1, MPI_COMM_WORLD);
        }
    }

    // Only rank 0 has the data, broadcast results
    if (world_rank_ == 0) {
        int count = static_cast<int>(latencies.size());
        MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(latencies.data(), count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    } else {
        int count = 0;
        MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
        latencies.resize(static_cast<size_t>(count));
        MPI_Bcast(latencies.data(), count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
}

void MpTester::compute_ring_latency(size_t msg_size,
                                     std::vector<double>& latencies,
                                     int num_tests)
{
    latencies.reserve(num_tests);

    int rank = world_rank_;
    int size = world_size_;

    std::vector<double> my_lats;
    my_lats.reserve(num_tests);

    for (int i = 0; i < num_tests; ++i) {
        int next_rank = (rank + 1) % size;
        int prev_rank = (rank - 1 + size) % size;

        auto start = mpi_wtime();

        // Send to next, receive from previous
        MPI_Sendrecv(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE,
                     next_rank, 0,
                     recv_buf_->data(), static_cast<int>(msg_size), MPI_BYTE,
                     prev_rank, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        auto end = mpi_wtime();
        my_lats.push_back(end - start);
    }

    latencies = std::move(my_lats);
}

void MpTester::compute_all_to_all_latency(size_t msg_size,
                                           std::vector<double>& latencies,
                                           int num_tests)
{
    latencies.reserve(num_tests);

    std::vector<double> my_lats;
    my_lats.reserve(num_tests);

    int rank = world_rank_;
    int size = world_size_;

    // Use MPI_Alltoall for proper all-to-all communication
    std::vector<char> send_buf(msg_size * size);
    std::vector<char> recv_buf(msg_size * size);
    
    // Fill send buffer with data for each rank
    for (int i = 0; i < size; ++i) {
        std::fill(send_buf.begin() + i * msg_size, 
                  send_buf.begin() + (i + 1) * msg_size, 
                  static_cast<char>(rank & 0xFF));
    }

    for (int i = 0; i < num_tests; ++i) {
        auto start = mpi_wtime();

        MPI_Alltoall(send_buf.data(), static_cast<int>(msg_size), MPI_BYTE,
                     recv_buf.data(), static_cast<int>(msg_size), MPI_BYTE,
                     MPI_COMM_WORLD);

        auto end = mpi_wtime();
        my_lats.push_back(end - start);
    }

    latencies = std::move(my_lats);
}

void MpTester::compute_star_latency(size_t msg_size,
                                      std::vector<double>& latencies,
                                      int num_tests)
{
    latencies.reserve(num_tests);

    std::vector<double> my_lats;
    my_lats.reserve(num_tests);

    int rank = world_rank_;
    int size = world_size_;

    // Star topology: rank 0 is the hub, all others connect to it
    if (rank == 0) {
        // Hub: communicate with each leaf
        std::vector<std::vector<char>> leaf_bufs(size - 1, std::vector<char>(msg_size));

        for (int i = 0; i < num_tests; ++i) {
            auto start = mpi_wtime();

            // Send to all leaves and receive from all leaves
            for (int leaf = 1; leaf < size; ++leaf) {
                MPI_Sendrecv(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE,
                             leaf, 0,
                             leaf_bufs[leaf - 1].data(), static_cast<int>(msg_size), MPI_BYTE,
                             leaf, 0,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            auto end = mpi_wtime();
            my_lats.push_back(end - start);
        }
    } else {
        // Leaf: communicate with hub (rank 0)
        std::vector<char> hub_buf(msg_size);

        for (int i = 0; i < num_tests; ++i) {
            auto start = mpi_wtime();

            MPI_Sendrecv(send_buf_->data(), static_cast<int>(msg_size), MPI_BYTE,
                         0, 0,
                         hub_buf.data(), static_cast<int>(msg_size), MPI_BYTE,
                         0, 0,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            auto end = mpi_wtime();
            my_lats.push_back(end - start);
        }
    }

    latencies = std::move(my_lats);
}

void MpTester::cleanup() {
    // unique_ptr handles cleanup automatically
    send_buf_.reset();
    recv_buf_.reset();
}

bool MpTester::is_leader() const {
    return world_rank_ == 0;
}

}  // namespace bt

#endif  // HAVE_MPI
