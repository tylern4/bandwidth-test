#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <functional>
#include <iostream>
#include <sstream>

namespace bt {

namespace {

// Compute percentile from already-sorted data
double percentile_from_sorted(const std::vector<double>& sorted_data, double pct) {
    if (sorted_data.empty()) return 0.0;
    if (sorted_data.size() == 1) return sorted_data[0];
    double idx = (pct / 100.0) * (sorted_data.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(idx));
    size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return sorted_data[lo];
    double frac = idx - lo;
    return sorted_data[lo] * (1.0 - frac) + sorted_data[hi] * frac;
}

double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    return sum / v.size();
}

double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    double m = mean(v);
    double sq_sum = 0.0;
    for (double x : v) {
        double diff = x - m;
        sq_sum += diff * diff;
    }
    return std::sqrt(sq_sum / (v.size() - 1));
}

}  // namespace

TestResult compute_statistics(size_t message_size,
                              const std::vector<double>& latencies_us,
                              double bytes_per_round_trip) {
    TestResult result;
    result.message_size = message_size;
    result.num_samples = latencies_us.size();

    if (latencies_us.empty()) return result;

    // Compute bandwidths from latencies
    // bandwidth = bytes_per_round_trip / latency_us (gives MB/s since latency is in us)
    std::vector<double> bandwidths;
    bandwidths.reserve(latencies_us.size());
    for (double lat : latencies_us) {
        if (lat > 0) {
            bandwidths.push_back(bytes_per_round_trip / lat);
        }
    }

    if (bandwidths.empty()) return result;

    // Bandwidth statistics
    result.mean_bandwidth_mbps = mean(bandwidths);
    result.min_bandwidth_mbps = *std::min_element(bandwidths.begin(), bandwidths.end());
    result.max_bandwidth_mbps = *std::max_element(bandwidths.begin(), bandwidths.end());

    // Sort latencies once for all percentile calculations
    auto sorted_latencies = latencies_us;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());

    // Latency statistics (computed from sorted data)
    result.median_latency_us = percentile_from_sorted(sorted_latencies, 50.0);
    result.p50_latency_us = result.median_latency_us;  // Same as median
    result.p95_latency_us = percentile_from_sorted(sorted_latencies, 95.0);
    result.p99_latency_us = percentile_from_sorted(sorted_latencies, 99.0);
    result.stddev_latency_us = stddev(latencies_us);

    return result;
}

std::vector<double> run_benchmark(
    int warmup,
    int num_tests,
    bool verbose,
    size_t message_size,
    std::function<double(size_t idx)> measure_fn)
{
    std::vector<double> latencies;
    latencies.reserve(static_cast<size_t>(num_tests));

    // Warmup
    for (int i = 0; i < warmup; ++i) {
        measure_fn(static_cast<size_t>(i));
    }

    // Benchmarks
    for (int i = 0; i < num_tests; ++i) {
        double lat = measure_fn(static_cast<size_t>(i + warmup));
        latencies.push_back(lat);
        if (verbose) {
            std::ostringstream oss;
            oss << "  iter " << (i + 1) << "/" << num_tests
                << "  size=" << message_size << "B  latency=" << lat << "us";
            std::cout << oss.str() << "\r" << std::flush;
        }
    }
    if (verbose) std::cout << "\n";

    return latencies;
}

}  // namespace bt
