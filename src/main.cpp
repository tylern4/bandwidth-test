#include "cli.hpp"
#include "config.hpp"
#include "bandwidth_tester.hpp"
#include "output.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <signal.h>
#include <atomic>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

// Global state for signal handling
static std::atomic<bool> g_interrupted(false);
static bt::BandwidthTester* g_tester = nullptr;

void signal_handler(int signum) {
    g_interrupted = true;
    if (g_tester) {
        g_tester->cleanup();
        g_tester = nullptr;
    }
}

int main(int argc, char* argv[]) {
    bt::Config config;

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // MPI initialization
#ifdef HAVE_MPI
    bool mpi_initialized = false;
    MPI_Init(&argc, &argv);
    mpi_initialized = true;
#endif

    // Parse CLI
    if (!bt::parse_cli(argc, argv, config)) {
#ifdef HAVE_MPI
        if (mpi_initialized) MPI_Finalize();
#endif
        return 1;
    }

    // Create tester
    auto tester = bt::create_tester(config);
    if (!tester) {
#ifdef HAVE_MPI
        if (mpi_initialized) MPI_Finalize();
#endif
        return 1;
    }

    // Register tester for signal handling
    g_tester = tester.get();

    // Setup
    if (!tester->setup()) {
        std::cerr << "Error: failed to initialize " << tester->name() << " backend.\n";
#ifdef HAVE_MPI
        if (mpi_initialized) MPI_Finalize();
#endif
        return 1;
    }

    // Determine message sizes
    std::vector<size_t> sizes;
    if (config.message_sizes.empty()) {
        sizes.push_back(config.single_message_size);
    } else {
        sizes = config.message_sizes;
    }

    // Run benchmarks
    auto results = tester->run_many(sizes, config.num_tests, config.warmup, config.verbose,
                                    config.transfer_mode, config.chunk_count);

    // Cleanup
    tester->cleanup();

    // Output results
    std::ostream* os = &std::cout;
    std::ofstream file_stream;

    if (!config.output_file.empty()) {
        file_stream.open(config.output_file);
        if (!file_stream.is_open()) {
            std::cerr << "Error: could not open output file '" << config.output_file << "'\n";
#ifdef HAVE_MPI
            if (mpi_initialized) MPI_Finalize();
#endif
            return 1;
        }
        os = &file_stream;
    }

    if (config.output_format == "csv") {
        bt::write_csv(*os, results);
    } else if (config.output_format == "json") {
        bt::write_json(*os, results);
    } else {
        bt::write_text(*os, results);
    }

#ifdef HAVE_MPI
    if (mpi_initialized) MPI_Finalize();
#endif

    return 0;
}
