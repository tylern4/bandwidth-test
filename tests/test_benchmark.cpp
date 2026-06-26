#include "catch2/catch_test_macros.hpp"
#include "catch2/catch_approx.hpp"

#include "../src/benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <numeric>
#include <random>
#include <iostream>

namespace bt {

namespace {

double calc_mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    return sum / v.size();
}

double calc_stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    double m = calc_mean(v);
    double sq_sum = 0.0;
    for (double x : v) {
        double diff = x - m;
        sq_sum += diff * diff;
    }
    return std::sqrt(sq_sum / (v.size() - 1));
}

}  // namespace

TEST_CASE("compute_statistics returns zero for empty latencies", "[benchmark][statistics]") {
    std::vector<double> latencies;
    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.message_size == 1024);
    REQUIRE(result.num_samples == 0);
    REQUIRE(result.mean_bandwidth_mbps == 0.0);
    REQUIRE(result.min_bandwidth_mbps == 0.0);
    REQUIRE(result.max_bandwidth_mbps == 0.0);
    REQUIRE(result.median_latency_us == 0.0);
}

TEST_CASE("compute_statistics calculates mean bandwidth correctly", "[benchmark][statistics]") {
    std::vector<double> latencies(10, 1.0);
    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.message_size == 1024);
    REQUIRE(result.num_samples == 10);
    REQUIRE(result.mean_bandwidth_mbps == Catch::Approx(2048.0));
}

TEST_CASE("compute_statistics calculates percentiles correctly", "[benchmark][statistics]") {
    std::vector<double> latencies;
    for (int i = 1; i <= 10; ++i) {
        latencies.push_back(static_cast<double>(i));
    }

    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.p50_latency_us == Catch::Approx(5.5));
    REQUIRE(result.median_latency_us == Catch::Approx(5.5));
    REQUIRE(result.p95_latency_us == Catch::Approx(9.55));
    REQUIRE(result.p99_latency_us == Catch::Approx(9.91));
}

TEST_CASE("compute_statistics min/max bandwidth", "[benchmark][statistics]") {
    std::vector<double> latencies = {1.0, 2.0};
    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.max_bandwidth_mbps == Catch::Approx(2048.0));
    REQUIRE(result.min_bandwidth_mbps == Catch::Approx(1024.0));
}

TEST_CASE("compute_statistics stddev", "[benchmark][statistics]") {
    std::vector<double> latencies = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto result = compute_statistics(1024, latencies, 2048.0);

    double expected_stddev = calc_stddev(latencies);
    REQUIRE(result.stddev_latency_us == Catch::Approx(expected_stddev));
}

TEST_CASE("compute_statistics handles single sample", "[benchmark][statistics]") {
    std::vector<double> latencies = {42.0};
    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.num_samples == 1);
    REQUIRE(result.mean_bandwidth_mbps == Catch::Approx(2048.0 / 42.0));
    REQUIRE(result.stddev_latency_us == Catch::Approx(0.0));
}

TEST_CASE("compute_statistics filters zero latencies", "[benchmark][statistics]") {
    std::vector<double> latencies = {1.0, 0.0, 2.0, 0.0, 3.0};
    auto result = compute_statistics(1024, latencies, 2048.0);

    REQUIRE(result.num_samples == 5);
    REQUIRE(result.mean_bandwidth_mbps > 0.0);
}

TEST_CASE("compute_statistics large message size", "[benchmark][statistics]") {
    std::vector<double> latencies(5, 100.0);
    auto result = compute_statistics(1048576, latencies, 2097152.0);

    REQUIRE(result.message_size == 1048576);
    REQUIRE(result.mean_bandwidth_mbps == Catch::Approx(2097152.0 / 100.0));
}

TEST_CASE("compute_statistics with varied latencies", "[benchmark][statistics]") {
    std::vector<double> latencies = {10.0, 12.0, 11.0, 9.0, 13.0, 8.0, 14.0, 10.5, 11.5, 10.0};
    auto result = compute_statistics(512, latencies, 1024.0);

    REQUIRE(result.num_samples == 10);
    REQUIRE(result.mean_bandwidth_mbps > 0.0);
    REQUIRE(result.min_bandwidth_mbps > 0.0);
    REQUIRE(result.max_bandwidth_mbps > 0.0);
    REQUIRE(result.min_bandwidth_mbps <= result.mean_bandwidth_mbps);
    REQUIRE(result.max_bandwidth_mbps >= result.mean_bandwidth_mbps);
}

TEST_CASE("run_benchmark collects correct number of samples", "[benchmark][runner]") {
    int collected = 0;
    auto measure_fn = [&collected](size_t /*idx*/) -> double {
        collected++;
        return 1.0;
    };

    auto latencies = run_benchmark(3, 7, false, 1024, measure_fn);

    REQUIRE(latencies.size() == 7);
    REQUIRE(collected == 10);
}

TEST_CASE("run_benchmark warmup iterations don't affect results", "[benchmark][runner]") {
    auto measure_fn = [](size_t idx) -> double {
        double val = 10.0 + static_cast<double>(idx);
        return val;
    };

    auto latencies = run_benchmark(5, 3, false, 1024, measure_fn);

    REQUIRE(latencies.size() == 3);
    REQUIRE(latencies[0] == Catch::Approx(15.0));
    REQUIRE(latencies[1] == Catch::Approx(16.0));
    REQUIRE(latencies[2] == Catch::Approx(17.0));
}

TEST_CASE("run_benchmark with zero warmup", "[benchmark][runner]") {
    auto measure_fn = [](size_t idx) -> double {
        return 5.0 + static_cast<double>(idx);
    };

    auto latencies = run_benchmark(0, 4, false, 1024, measure_fn);

    REQUIRE(latencies.size() == 4);
    REQUIRE(latencies[0] == Catch::Approx(5.0));
    REQUIRE(latencies[3] == Catch::Approx(8.0));
}

TEST_CASE("run_benchmark verbose output", "[benchmark][runner]") {
    auto measure_fn = [](size_t /*idx*/) -> double {
        return 1.0;
    };

    auto latencies = run_benchmark(1, 2, true, 1024, measure_fn);
    REQUIRE(latencies.size() == 2);
}

TEST_CASE("run_benchmark with many iterations", "[benchmark][runner]") {
    auto measure_fn = [](size_t /*idx*/) -> double {
        return 0.5;
    };

    auto latencies = run_benchmark(100, 1000, false, 1024, measure_fn);
    REQUIRE(latencies.size() == 1000);

    for (double lat : latencies) {
        REQUIRE(lat == Catch::Approx(0.5));
    }
}

}  // namespace bt
