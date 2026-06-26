#pragma once

#include "benchmark.hpp"
#include <string>
#include <vector>
#include <ostream>

namespace bt {

/// Format a single TestResult as a line of text.
std::string format_result_text(const TestResult& r);

/// Format a table header for multiple results.
std::string format_header_text();

/// Format multiple TestResults as a comparison table.
std::string format_results_table_text(const std::vector<TestResult>& results);

/// Write text output (header + table + per-result detail).
void write_text(std::ostream& os, const std::vector<TestResult>& results);

/// Write CSV output.
void write_csv(std::ostream& os, const std::vector<TestResult>& results);

/// Write JSON output.
void write_json(std::ostream& os, const std::vector<TestResult>& results);

}  // namespace bt
