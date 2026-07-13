#include "bandwidth_tester.hpp"
#include "config.hpp"
#include "cli.hpp"

#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef HAVE_ZMQ
#include "zmq_bandwidth_tester.hpp"
#endif

#ifdef HAVE_MPI
#include "mpi_bandwidth_tester.hpp"
#endif

namespace bt {

namespace {

std::string format_duration(double seconds) {
    if (seconds < 60.0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << seconds << "s";
        return oss.str();
    }
    int mins = static_cast<int>(seconds) / 60;
    double secs = seconds - mins * 60;
    std::ostringstream oss;
    oss << mins << "m " << std::fixed << std::setprecision(0) << secs << "s";
    return oss.str();
}

std::string format_size(size_t bytes) {
    if (bytes >= 1048576) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (bytes / 1048576.0) << " MB";
        return oss.str();
    }
    if (bytes >= 1024) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return oss.str();
    }
    return std::to_string(bytes) + " B";
}

}  // namespace

std::vector<TestResult> BandwidthTester::run_many(const std::vector<size_t>& sizes,
                                                   int num_tests,
                                                   int warmup,
                                                   bool verbose,
                                                   TransferMode mode,
                                                   int chunk_count)
{
    std::vector<TestResult> results;
    results.reserve(sizes.size());

    bool show_progress = (rank() == 0);
    
    if (!sizes.empty() && show_progress) {
        std::cout << "\nRunning " << name() << " benchmark:\n";
        std::cout << "  Sizes: " << sizes.size() << " points";
        if (sizes.size() > 1) {
            std::cout << " (" << format_size(sizes.front()) << " to " << format_size(sizes.back()) << ")";
        }
        std::cout << "\n";
        std::cout << "  Warmup: " << warmup << ", Tests: " << num_tests << "\n";
        if (mode == TransferMode::CHUNKED) {
            std::cout << "  Transfer mode: chunked (" << chunk_count << " chunks)\n";
        }
        std::cout << "\n";
    }

    auto total_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < sizes.size(); ++i) {
        auto iter_start = std::chrono::high_resolution_clock::now();
        
        if (show_progress) {
            std::cout << "  [" << (i + 1) << "/" << sizes.size() << "] "
                      << format_size(sizes[i]) << "... ";
            std::cout.flush();
        }
        
        auto result = run_single(sizes[i], num_tests, warmup, verbose,
                                 mode, chunk_count);
        results.push_back(result);
        
        if (show_progress) {
            auto iter_end = std::chrono::high_resolution_clock::now();
            double iter_secs = std::chrono::duration<double>(iter_end - iter_start).count();
            
            // Calculate progress
            double elapsed = std::chrono::duration<double>(iter_end - total_start).count();
            double estimated_total = (elapsed / (i + 1)) * sizes.size();
            double remaining = estimated_total - elapsed;
            
            std::cout << result.mean_bandwidth_mbps << " MB/s";
            if (sizes.size() > 1 && i < sizes.size() - 1) {
                std::cout << " (" << format_duration(iter_secs) 
                          << ", ETA: " << format_duration(remaining) << ")";
            } else {
                std::cout << " (" << format_duration(iter_secs) << ")";
            }
            std::cout << "\n";
            std::cout.flush();
        }
    }
    
    if (show_progress) {
        auto total_end = std::chrono::high_resolution_clock::now();
        double total_secs = std::chrono::duration<double>(total_end - total_start).count();
        std::cout << "\n  Total time: " << format_duration(total_secs) << "\n";
    }

    return results;
}

std::unique_ptr<BandwidthTester> create_tester(const Config& config) {
    switch (config.backend) {
#ifdef HAVE_ZMQ
        case Backend::ZMQ: {
            return std::make_unique<ZmqTester>(
                config.zmq_addr,
                config.zmq_socket_type,
                config.zmq_transport,
                config.zmq_mode
            );
        }
#endif
#ifdef HAVE_MPI
        case Backend::MPI: {
            return std::make_unique<MpTester>(config.mpi_topology, config.mpi_ranks);
        }
#endif
        default:
            std::cerr << "Error: no backend available. Compile with HAVE_ZMQ or HAVE_MPI.\n";
            return nullptr;
    }
}

}  // namespace bt
