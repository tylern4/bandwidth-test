#include "output.hpp"

#include <iomanip>
#include <sstream>
#include <iostream>
#include <cmath>

namespace bt {

namespace {

std::string fmt_mbps(double val) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << val;
    return oss.str();
}

std::string fmt_us(double val) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << val;
    return oss.str();
}

std::string fmt_bytes(size_t val) {
    if (val >= 1048576) {
        double mb = static_cast<double>(val) / 1048576.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << mb << "MB";
        return oss.str();
    }
    if (val >= 1024) {
        double kb = static_cast<double>(val) / 1024.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << kb << "KB";
        return oss.str();
    }
    return std::to_string(val) + "B";
}

}  // namespace

std::string format_result_text(const TestResult& r) {
    std::ostringstream oss;
    oss << "  Message size: " << fmt_bytes(r.message_size) << "\n"
        << "  Bandwidth:    " << fmt_mbps(r.mean_bandwidth_mbps) << " MB/s "
        << "(min=" << fmt_mbps(r.min_bandwidth_mbps) << ", max=" << fmt_mbps(r.max_bandwidth_mbps) << ")\n"
        << "  Latency:      median=" << fmt_us(r.median_latency_us) << "us "
        << "P50=" << fmt_us(r.p50_latency_us) << "us "
        << "P95=" << fmt_us(r.p95_latency_us) << "us "
        << "P99=" << fmt_us(r.p99_latency_us) << "us\n"
        << "  Std dev:      " << fmt_us(r.stddev_latency_us) << "us\n"
        << "  Samples:      " << r.num_samples << "\n";
    return oss.str();
}

std::string format_header_text() {
    return (
        "  %-8s  %12s  %12s  %10s  %10s  %10s  %10s\n"
        "  %-8s  %12s  %12s  %10s  %10s  %10s  %10s\n"
    "  --------  ------------  ------------  ----------  ----------  ----------  ----------\n"
    );
}

std::string format_results_table_text(const std::vector<TestResult>& results) {
    std::ostringstream oss;
    oss << std::string(78, '=') << "\n";
    oss << "  Bandwidth Benchmark Results\n";
    oss << std::string(78, '=') << "\n\n";

    if (results.empty()) {
        oss << "  No results to display.\n";
        return oss.str();
    }

    // Table header
    oss << std::string(78, '-') << "\n";
    oss << "  "
        << std::left << std::setw(8) << "Size"
        << std::right << std::setw(12) << "Mean MB/s"
        << std::setw(12) << "Min MB/s"
        << std::setw(10) << "Med lat"
        << std::setw(10) << "P95 lat"
        << std::setw(10) << "P99 lat"
        << std::setw(10) << "Stddev"
        << "\n";
    oss << std::string(78, '-') << "\n";

    for (const auto& r : results) {
        oss << "  "
            << std::left << std::setw(8) << fmt_bytes(r.message_size)
            << std::right << std::setw(12) << fmt_mbps(r.mean_bandwidth_mbps)
            << std::setw(12) << fmt_mbps(r.min_bandwidth_mbps)
            << std::setw(10) << fmt_us(r.median_latency_us)
            << std::setw(10) << fmt_us(r.p95_latency_us)
            << std::setw(10) << fmt_us(r.p99_latency_us)
            << std::setw(10) << fmt_us(r.stddev_latency_us)
            << "\n";
    }
    oss << std::string(78, '-') << "\n";

    // Per-result detail
    for (const auto& r : results) {
        oss << "\n" << format_result_text(r) << "\n";
    }

    return oss.str();
}

void write_text(std::ostream& os, const std::vector<TestResult>& results) {
    os << format_results_table_text(results);
}

void write_csv(std::ostream& os, const std::vector<TestResult>& results) {
    os << "message_size,mean_bandwidth_mbps,min_bandwidth_mbps,max_bandwidth_mbps,"
       << "median_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,"
       << "stddev_latency_us,num_samples\n";
    for (const auto& r : results) {
        os << r.message_size << ","
           << std::fixed << std::setprecision(6) << r.mean_bandwidth_mbps << ","
           << r.min_bandwidth_mbps << ","
           << r.max_bandwidth_mbps << ","
           << r.median_latency_us << ","
           << r.p50_latency_us << ","
           << r.p95_latency_us << ","
           << r.p99_latency_us << ","
           << r.stddev_latency_us << ","
           << r.num_samples << "\n";
    }
}

void write_json(std::ostream& os, const std::vector<TestResult>& results) {
    os << "{\n";
    os << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        os << "    {\n";
        os << "      \"message_size\": " << r.message_size << ",\n";
        os << "      \"mean_bandwidth_mbps\": " << std::fixed << std::setprecision(6) << r.mean_bandwidth_mbps << ",\n";
        os << "      \"min_bandwidth_mbps\": " << r.min_bandwidth_mbps << ",\n";
        os << "      \"max_bandwidth_mbps\": " << r.max_bandwidth_mbps << ",\n";
        os << "      \"median_latency_us\": " << r.median_latency_us << ",\n";
        os << "      \"p50_latency_us\": " << r.p50_latency_us << ",\n";
        os << "      \"p95_latency_us\": " << r.p95_latency_us << ",\n";
        os << "      \"p99_latency_us\": " << r.p99_latency_us << ",\n";
        os << "      \"stddev_latency_us\": " << r.stddev_latency_us << ",\n";
        os << "      \"num_samples\": " << r.num_samples << "\n";
        os << "    }";
        if (i + 1 < results.size()) os << ",";
        os << "\n";
    }
    os << "  ]\n";
    os << "}\n";
}

}  // namespace bt
