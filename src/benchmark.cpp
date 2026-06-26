#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <numeric>
#include <functional>
#include <iostream>
#include <sstream>

namespace bt {

namespace {

double percentile(std::vector<double> data, double pct) {
    std::sort(data.begin(), data.end());
    if (data.empty()) return 0.0;
    if (data.size() == 1) return data[0];
    double idx = (pct / 100.0) * (data.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(idx));
    size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return data[lo];
    double frac = idx - lo;
    return data[lo] * (1.0 - frac) + data[hi] * frac;
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

    // Bandwidth = bytes / time, convert to MB/s
    // latency_us is in microseconds, bytes_per_round_trip is bytes
    // bandwidth = bytes_per_round_trip / (latency_us * 1e-6) / 1e6 = bytes_per_round_trip / latency_us
    std::vector<double> bandwidths;
    bandwidths.reserve(latencies_us.size());
    for (double lat : latencies_us) {
        if (lat > 0) {
            bandwidths.push_back(bytes_per_round_trip / lat);
        }
    }

    if (bandwidths.empty()) return result;

    double m = mean(bandwidths);
    result.mean_bandwidth_mbps = m;
    result.min_bandwidth_mbps = *std::min_element(bandwidths.begin(), bandwidths.end());
    result.max_bandwidth_mbps = *std::max_element(bandwidths.begin(), bandwidths.end());

    // Latency stats
    auto sorted = latencies_us;
    result.median_latency_us = percentile(sorted, 50.0);
    result.p50_latency_us = percentile(sorted, 50.0);
    result.p95_latency_us = percentile(sorted, 95.0);
    result.p99_latency_us = percentile(sorted, 99.0);
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
