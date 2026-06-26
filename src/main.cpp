#include "cli.hpp"
#include "config.hpp"
#include "bandwidth_tester.hpp"
#include "output.hpp"

#include <iostream>
#include <fstream>
#include <memory>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

int main(int argc, char* argv[]) {
    bt::Config config;

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
    auto results = tester->run_many(sizes, config.num_tests, config.warmup, config.verbose);

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
