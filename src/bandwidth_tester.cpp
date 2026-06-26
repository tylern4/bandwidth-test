#include "bandwidth_tester.hpp"
#include "config.hpp"
#include "cli.hpp"

#include <iostream>
#include <memory>

#ifdef HAVE_ZMQ
#include "zmq_bandwidth_tester.hpp"
#endif

#ifdef HAVE_MPI
#include "mpi_bandwidth_tester.hpp"
#endif

namespace bt {

std::vector<TestResult> BandwidthTester::run_many(const std::vector<size_t>& sizes,
                                                   int num_tests,
                                                   int warmup,
                                                   bool verbose)
{
    std::vector<TestResult> results;
    results.reserve(sizes.size());

    if (!sizes.empty() && verbose && rank() == 0) {
        std::cout << "\nRunning " << name() << " benchmark:\n";
        std::cout << "  Sizes: " << sizes.size() << " points\n";
        std::cout << "  Warmup: " << warmup << ", Tests: " << num_tests << "\n\n";
    }

    for (size_t i = 0; i < sizes.size(); ++i) {
        if (verbose && rank() == 0) {
            std::cout << "  [" << (i + 1) << "/" << sizes.size() << "] "
                      << sizes[i] << " bytes...\n";
        }
        auto result = run_single(sizes[i], num_tests, warmup, verbose,
                                 TransferMode::SINGLE, 100);
        results.push_back(result);
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
