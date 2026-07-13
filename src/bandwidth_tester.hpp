#pragma once

#include "config.hpp"
#include "benchmark.hpp"
#include <string>
#include <memory>
#include <functional>

namespace bt {

/// Abstract interface for a bandwidth testing backend.
/// Each backend must support setup/run/cleanup lifecycle.
class BandwidthTester {
public:
    virtual ~BandwidthTester() = default;

    /// Initialize the backend (bind/listen, MPI barrier, etc.).
    virtual bool setup() = 0;

    /// Run a single benchmark at the given message size.
    virtual TestResult run_single(size_t message_size,
                                  int num_tests,
                                  int warmup,
                                  bool verbose,
                                  TransferMode mode = TransferMode::SINGLE,
                                  int chunk_count = 100) = 0;

    /// Run multiple benchmarks at different sizes.
    std::vector<TestResult> run_many(const std::vector<size_t>& sizes,
                                     int num_tests,
                                     int warmup,
                                     bool verbose,
                                     TransferMode mode = TransferMode::SINGLE,
                                     int chunk_count = 100);

    /// Cleanup resources.
    virtual void cleanup() = 0;

    /// Return the backend name for logging.
    virtual std::string name() const = 0;

    /// Check if the tester is running in leader mode.
    /// Leaders typically wait for followers to connect first.
    virtual bool is_leader() const = 0;

    /// Return the rank/index of this process.
    virtual int rank() const = 0;
};

/// Factory function to create a tester based on config.
std::unique_ptr<BandwidthTester> create_tester(const Config& config);

}  // namespace bt
