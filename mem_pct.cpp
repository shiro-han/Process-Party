/**
 * @file mem_pct.cpp
 * @brief Computes memory utilization percentage from /proc/meminfo.
 *
 * This utility reads MemTotal and MemAvailable from /proc/meminfo and reports
 * the percentage of memory currently in use:
 *
 *   used = MemTotal - MemAvailable
 *   pct  = (used / MemTotal) * 100
 *
 * Output formats:
 *  - Human: "<pct>%"
 *  - Raw:   "mem_pct=<pct> mem_total_kb=<total> mem_avail_kb=<avail>"
 *
 * @note Linux-specific: relies on /proc/meminfo.
 * @note Uses MemAvailable instead of MemFree to better reflect reclaimable memory.
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @brief Prints program usage information to standard error.
 *
 * @param prog Program name (typically argv[0]).
 */
static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--raw]\n";
}

/**
 * @brief Entry point.
 *
 * Command-line options:
 *  - --raw : Emit detailed key/value output instead of a formatted percentage.
 *
 * Exit codes:
 *  - 0: Success.
 *  - 1: Failed to read or parse /proc/meminfo.
 *  - 2: Invalid command-line arguments.
 *
 * @param argc Argument count.
 *  @param argv Argument vector.
 * @return Process exit code.
 */
int main(int argc, char** argv) {
    bool raw = false;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--raw") {
            raw = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    std::ifstream file("/proc/meminfo");
    if (!file) {
        std::cerr << "Error: could not open /proc/meminfo\n";
        return 1;
    }

    long long mem_total_kb = 0;   ///< Total physical memory in kilobytes.
    long long mem_avail_kb = 0;   ///< Available memory in kilobytes.

    std::string key;
    long long value;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") {
            mem_total_kb = value;
        } else if (key == "MemAvailable:") {
            mem_avail_kb = value;
        }
    }

    if (mem_total_kb <= 0 || mem_avail_kb < 0) {
        std::cerr << "Error: could not read /proc/meminfo\n";
        return 1;
    }

    const long long mem_used_kb = mem_total_kb - mem_avail_kb;

    const double mem_pct =
        (static_cast<double>(mem_used_kb) /
         static_cast<double>(mem_total_kb)) * 100.0;

    std::cout << std::fixed << std::setprecision(1);

    if (raw) {
        std::cout << "mem_pct=" << mem_pct
                  << " mem_total_kb=" << mem_total_kb
                  << " mem_avail_kb=" << mem_avail_kb
                  << "\n";
    } else {
        std::cout << mem_pct << "%\n";
    }

    return 0;
}