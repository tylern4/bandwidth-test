#pragma once

#include "config.hpp"

namespace bt {

/// Parse command-line arguments into a Config struct.
/// Returns true on success, false on error (prints usage).
bool parse_cli(int argc, char* argv[], Config& config);

/// Return a human-readable help string.
std::string help_text();

}  // namespace bt
