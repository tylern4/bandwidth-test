#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <cstddef>
#include <functional>

namespace bt {

struct TestResult {
    size_t message_size = 0;
    double mean_bandwidth_mbps = 0.0;
    double min_bandwidth_mbps = 0.0;
    double max_bandwidth_mbps = 0.0;
    double median_latency_us = 0.0;
    double p50_latency_us = 0.0;
    double p95_latency_us = 0.0;
    double p99_latency_us = 0.0;
    double stddev_latency_us = 0.0;
    size_t num_samples = 0;
};

/// Calculate statistics from a vector of latency values (in microseconds).
/// Returns a TestResult with computed metrics.
TestResult compute_statistics(size_t message_size,
                              const std::vector<double>& latencies_us,
                              double bytes_per_round_trip);

/// Run a benchmark loop: warmup + N iterations, collecting latencies.
/// The caller provides send/recv callbacks and a measurement function.
std::vector<double> run_benchmark(
    int warmup,
    int num_tests,
    bool verbose,
    size_t message_size,
    std::function<double(size_t idx)> measure_fn);

}  // namespace bt
