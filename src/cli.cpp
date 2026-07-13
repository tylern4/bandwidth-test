#include "cli.hpp"

#include <clipp.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace clipp;

namespace bt
{

    namespace
    {

        std::vector<size_t> log_sizes(size_t start, size_t end, double factor)
        {
            std::vector<size_t> sizes;
            sizes.push_back(start);
            size_t current = start;
            while (current < end)
            {
                double next_d = static_cast<double>(current) * factor;
                if (next_d > static_cast<double>(end))
                    break;
                size_t next = static_cast<size_t>(std::round(next_d));
                if (next <= current)
                {
                    next = current + 1;
                }
                sizes.push_back(next);
                current = next;
            }
            return sizes;
        }

        bool parse_sizes(const std::string &str, std::vector<size_t> &out)
        {
            out.clear();
            std::istringstream iss(str);
            std::string token;
            bool has_errors = false;
            while (std::getline(iss, token, ','))
            {
                if (token.empty())
                    continue;
                try
                {
                    size_t val = static_cast<size_t>(std::stoull(token));
                    if (val == 0) {
                        std::cerr << "Warning: skipping invalid message size 0\n";
                        has_errors = true;
                        continue;
                    }
                    out.push_back(val);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Warning: could not parse message size '" << token << "': " << e.what() << "\n";
                    has_errors = true;
                }
            }
            return !has_errors;
        }

    } // namespace

    bool parse_cli(int argc, char *argv[], Config &config)
    {
        size_t sweep_min = 0, sweep_max = 0;
        std::string topo_str;
        std::string format_str;
        std::string transfer_str;
        std::string backend_str;
        std::string zmq_mode_str;
        std::string sizes_str;
        bool show_help = false;

        auto help_option = option("-h", "--help") % "Show this help message";

        auto cli = (
            // general - independent options (comma-separated = join group)
            (option("-n", "--num-tests") & value("N", config.num_tests)) % "Number of benchmark iterations per message size", (option("-t", "--warmup") & value("N", config.warmup)) % "Warmup iterations (excluded from stats)", option("-v", "--verbose") % "Verbose per-iteration output", (option("--format") & value("text|csv|json", format_str)) % "Output format: text, csv, json", (option("--output") & value("FILE", config.output_file)) % "Output file (default: stdout)", help_option.set(show_help, true)

            // message sizes - one required (pipe-separated = exclusive group)
            ,
            ((option("-s", "--message-size") & value("BYTES", config.single_message_size)) % "Single message size in bytes" | (option("--sizes") & value("LIST", sizes_str)) % "Comma-separated list of message sizes" | (option("--sweep") & value("MIN", sweep_min) & value("MAX", sweep_max) & value("FACTOR", config.sweep_factor)) % "Sweep: min,max,factor (logarithmic)")

            // transfer mode options
            ,
            (option("--transfer-mode") & value("single|chunked", transfer_str)) % "single or chunked", (option("--chunk-count") & value("N", config.chunk_count)) % "Number of chunks in chunked mode"

            // backend selection
            ,
            (option("-b", "--backend") & value("zmq|mpi", backend_str)) % "zmq or mpi"

            // zmq options
            ,
            ((option("--zmq-addr") & value("ADDR", config.zmq_addr)) % "ZMQ address (default: tcp://localhost:5555)" , (option("--zmq-type") & value("TYPE", config.zmq_socket_type)) % "Socket type: pair, push/pull, req/rep" , (option("--zmq-transport") & value("TP", config.zmq_transport)) % "Transport: tcp, ipc, inproc" , (option("--zmq-mode") & value("server|client|single", zmq_mode_str)) % "server, client, or single")

            // mpi options - optional exclusive group
            ,
            ((option("--mpi-topology") & value("TOPO", topo_str)) % "Topology: ping-pong, ring, all-to-all, star" | (option("--mpi-ranks") & value("N", config.mpi_ranks)) % "Number of MPI ranks (0 = auto-detect)"));

        auto res = parse(argc, argv, cli);

        if (show_help)
        {
            std::cout << help_text() << std::endl;
            std::exit(0);
        }

        if (!res)
        {
            std::cerr << "Error: invalid command-line arguments.\n"
                      << help_text() << std::endl;
            return false;
        }

        // Parse backend
        if (!backend_str.empty())
        {
            if (backend_str == "zmq")
                config.backend = Backend::ZMQ;
            else if (backend_str == "mpi")
                config.backend = Backend::MPI;
            else
            {
                std::cerr << "Error: unknown backend '" << backend_str << "'. Use 'zmq' or 'mpi'." << std::endl;
                return false;
            }
        }

        // Parse transfer mode
        if (!transfer_str.empty())
        {
            if (transfer_str == "single")
                config.transfer_mode = TransferMode::SINGLE;
            else if (transfer_str == "chunked")
                config.transfer_mode = TransferMode::CHUNKED;
            else
            {
                std::cerr << "Error: unknown transfer mode '" << transfer_str << "'. Use 'single' or 'chunked'." << std::endl;
                return false;
            }
        }

        // Parse output format
        if (!format_str.empty())
        {
            if (format_str == "text" || format_str == "csv" || format_str == "json")
            {
                config.output_format = format_str;
            }
            else
            {
                std::cerr << "Error: unknown format '" << format_str << "'. Use 'text', 'csv', or 'json'." << std::endl;
                return false;
            }
        }

        // Parse MPI topology
        if (!topo_str.empty())
        {
            if (topo_str == "ping-pong")
                config.mpi_topology = MpTopology::PING_PONG;
            else if (topo_str == "ring")
                config.mpi_topology = MpTopology::RING;
            else if (topo_str == "all-to-all")
                config.mpi_topology = MpTopology::ALL_TO_ALL;
            else if (topo_str == "star")
                config.mpi_topology = MpTopology::STAR;
            else
            {
                std::cerr << "Error: unknown MPI topology '" << topo_str << "'." << std::endl;
                return false;
            }
        }

        // Parse ZMQ mode
        if (!zmq_mode_str.empty())
        {
            if (zmq_mode_str == "server")
                config.zmq_mode = ZmqMode::SERVER;
            else if (zmq_mode_str == "client")
                config.zmq_mode = ZmqMode::CLIENT;
            else if (zmq_mode_str == "single")
                config.zmq_mode = ZmqMode::SINGLE;
            else
            {
                std::cerr << "Error: unknown ZMQ mode '" << zmq_mode_str << "'. Use 'server', 'client', or 'single'." << std::endl;
                return false;
            }
        }

        // Parse and validate ZMQ transport
        if (config.zmq_transport != "tcp" && config.zmq_transport != "ipc" && config.zmq_transport != "inproc")
        {
            std::cerr << "Error: unknown ZMQ transport '" << config.zmq_transport << "'. Use 'tcp', 'ipc', or 'inproc'." << std::endl;
            return false;
        }

        // Parse sizes (comma-separated string -> vector)
        if (!sizes_str.empty())
        {
            if (!parse_sizes(sizes_str, config.message_sizes))
            {
                // Warning already printed, but continue with valid sizes
            }
            if (config.message_sizes.empty())
            {
                std::cerr << "Error: no valid message sizes provided.\n";
                return false;
            }
        }

        // Parse message sizes
        if (config.message_sizes.empty() && sweep_min > 0 && sweep_max > 0)
        {
            config.message_sizes = log_sizes(sweep_min, sweep_max, config.sweep_factor);
        }

        // Validate
        if (config.num_tests <= 0)
        {
            std::cerr << "Error: --num-tests must be positive." << std::endl;
            return false;
        }
        if (config.warmup < 0)
        {
            std::cerr << "Error: --warmup must be non-negative." << std::endl;
            return false;
        }
        if (config.message_sizes.empty())
        {
            if (config.single_message_size == 0)
            {
                std::cerr << "Error: must specify --message-size, --sizes, or --sweep." << std::endl;
                return false;
            }
        }

        return true;
    }

    std::string help_text()
    {
        std::ostringstream oss;
        oss << R"(
bandwidth-test - Measure network bandwidth between processes

USAGE:
  bandwidth-test [OPTIONS]

GENERAL OPTIONS:
  -n, --num-tests N         Number of benchmark iterations per message size  (default: 100)
  -t, --warmup N            Warmup iterations (excluded from stats)           (default: 5)
  -v, --verbose             Verbose per-iteration output
      --format FMT          Output format: text, csv, json                    (default: text)
      --output FILE         Output file (default: stdout)

MESSAGE SIZES (choose one):
  -s, --message-size SIZE   Single message size in bytes                      (default: 1024)
      --sizes LIST          Comma-separated list of sizes                     (e.g. 64,256,1024)
      --sweep MIN MAX [F]   Logarithmic sweep: MIN, MAX, and growth factor (default: 2.0)
                           (e.g. --sweep 64 1048576 2.0)

TRANSFER MODE:
      --transfer-mode MODE  single or chunked                                 (default: single)
      --chunk-count N       Number of chunks in chunked mode                  (default: 100)

BACKEND:
  -b, --backend BACKEND     zmq or mpi                                        (default: zmq)

ZMQ OPTIONS:
      --zmq-addr ADDR       ZMQ address                                       (default: tcp://localhost:5555)
      --zmq-type TYPE       Socket type: pair, push/pull, req/rep             (default: pair)
      --zmq-transport TP    Transport: tcp, ipc, inproc                       (default: tcp)
      --zmq-mode MODE       server|client (two processes) or single (one)     (default: server)

        TCP  (default)   Full address required (e.g., tcp://localhost:5555)
        IPC                Path only (e.g., /tmp/bw-test). Faster than TCP for local.
        inproc             Name only (e.g., bw-test). Same-process only, fastest.

      --zmq-mode MODE      server, client, or single
        server/client      Run two separate processes (TCP or IPC transport)
        single             Run both server and client in one process (inproc only)

MPI OPTIONS:
      --mpi-topology TOPO   ping-pong, ring, all-to-all, star                 (default: ping-pong)
      --mpi-ranks N         Number of MPI ranks (0 = auto-detect)

EXAMPLES:
  # TCP transport (default, two processes)
  # Terminal 1 - Server:
  ./bandwidth-test -b zmq --zmq-mode server -s 1024 -n 1000
  # Terminal 2 - Client:
  ./bandwidth-test -b zmq --zmq-mode client -s 1024 -n 1000

  # IPC transport (faster local, two processes)
  # Terminal 1 - Server:
  ./bandwidth-test -b zmq --zmq-mode server --zmq-transport ipc --zmq-addr /tmp/bw-test
  # Terminal 2 - Client:
  ./bandwidth-test -b zmq --zmq-mode client --zmq-transport ipc --zmq-addr /tmp/bw-test

  # inproc transport (fastest, single process)
  ./bandwidth-test -b zmq --zmq-mode single --zmq-transport inproc --zmq-addr bw-test -s 1024 -n 1000

  # Sweep from 64B to 1MB, MPI backend, ring topology
  mpirun -np 2 ./bandwidth-test -b mpi --sweep 64 1048576 2.0 --mpi-topology ring

  # Chunked transfer mode
  ./bandwidth-test -s 1048576 --transfer-mode chunked --chunk-count 100

  # CSV output
  ./bandwidth-test -s 1024 --format csv --output results.csv
)";
        return oss.str();
    }

} // namespace bt
