/**
 * @file cpu_pct.cpp
 * @brief Computes overall CPU utilization percentage from Linux /proc/stat.
 *
 * Samples the aggregate CPU counters from the first line of /proc/stat ("cpu")
 * twice, separated by a configurable interval, then reports the non-idle
 * fraction of time between the two samples as a percentage.
 *
 * Output formats:
 *  - Human: "<pct>%"
 *  - Raw:   "cpu_pct=<pct>"
 *
 * @note This utility is Linux-specific and requires access to /proc/stat.
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

/**
 * @brief Snapshot of the aggregate CPU time fields reported by /proc/stat.
 *
 * Values are cumulative counters (typically in jiffies) since boot for the
 * following states: user, nice, system, idle, iowait, irq, softirq, steal.
 */
struct CpuLine {
  long long user = 0;     ///< Time spent in user mode.
  long long nice = 0;     ///< Time spent in user mode with low priority (nice).
  long long system = 0;   ///< Time spent in system mode.
  long long idle = 0;     ///< Time spent idle.
  long long iowait = 0;   ///< Time spent waiting for I/O.
  long long irq = 0;      ///< Time servicing hardware interrupts.
  long long softirq = 0;  ///< Time servicing software interrupts.
  long long steal = 0;    ///< Time stolen by the hypervisor (virtualized systems).
};

/**
 * @brief Prints program usage information to standard error.
 *
 * @param prog Program name (typically argv[0]).
 */
static void usage(const char* prog) {
  std::cerr << "Usage: " << prog << " [--interval N] [--raw]\n";
}

/**
 * @brief Reads the aggregate CPU counters from /proc/stat.
 *
 * Parses the first line of /proc/stat and expects the tag "cpu" followed by the
 * standard fields: user, nice, system, idle, iowait, irq, softirq, steal.
 *
 * @param[out] out Filled with the parsed CPU counters on success.
 * @return True if the line was read and parsed successfully; false otherwise.
 */
static bool read_cpu_line(CpuLine& out) {
  std::ifstream in("/proc/stat");
  if (!in) return false;

  std::string line;
  if (!std::getline(in, line)) return false;

  std::istringstream iss(line);
  std::string tag;
  iss >> tag;
  if (tag != "cpu") return false;

  iss >> out.user >> out.nice >> out.system >> out.idle
      >> out.iowait >> out.irq >> out.softirq >> out.steal;

  return !iss.fail();
}

/**
 * @brief Computes CPU utilization percentage between two samples.
 *
 * Utilization is calculated as:
 *   (delta_total - delta_idle) / delta_total * 100
 *
 * Where idle is defined as (idle + iowait) and total is (idle + non-idle).
 *
 * @param a First sample (earlier).
 * @param b Second sample (later).
 * @return Utilization percentage in the range [0, 100] for normal counter
 *         progression. Returns 0.0 if the total delta is non-positive.
 */
static double calc_pct(const CpuLine& a, const CpuLine& b) {
  const long long idle1 = a.idle + a.iowait;
  const long long idle2 = b.idle + b.iowait;

  const long long non1 =
      a.user + a.nice + a.system + a.irq + a.softirq + a.steal;
  const long long non2 =
      b.user + b.nice + b.system + b.irq + b.softirq + b.steal;

  const long long total1 = idle1 + non1;
  const long long total2 = idle2 + non2;

  const long long dt = total2 - total1;
  const long long didle = idle2 - idle1;

  if (dt <= 0) return 0.0;

  return (static_cast<double>(dt - didle) / static_cast<double>(dt)) * 100.0;
}

/**
 * @brief Entry point.
 *
 * Command-line options:
 *  - --interval N : Sampling interval in seconds (double). Values < 0 are clamped to 0.
 *  - --raw        : Emit "cpu_pct=<pct>" instead of "<pct>%".
 *
 * Exit codes:
 *  - 0: Success.
 *  - 1: Failed to read /proc/stat.
 *  - 2: Invalid command-line arguments.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Process exit code.
 */
int main(int argc, char** argv) {
  double interval = 1.0;
  bool raw = false;

  for (int i = 1; i < argc;) {
    std::string arg = argv[i];
    if (arg == "--interval") {
      if (i + 1 >= argc) {
        usage(argv[0]);
        return 2;
      }
      try {
        interval = std::stod(argv[i + 1]);
      } catch (...) {
        usage(argv[0]);
        return 2;
      }
      if (interval < 0) interval = 0;
      i += 2;
    } else if (arg == "--raw") {
      raw = true;
      i += 1;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  CpuLine a, b;
  if (!read_cpu_line(a)) {
    std::cerr << "Error: cannot read /proc/stat\n";
    return 1;
  }

  const auto sleep_dur = std::chrono::duration<double>(interval);
  if (sleep_dur.count() > 0) {
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));
  }

  if (!read_cpu_line(b)) {
    std::cerr << "Error: cannot read /proc/stat (second sample)\n";
    return 1;
  }

  const double pct = calc_pct(a, b);

  std::cout << std::fixed << std::setprecision(1);
  if (raw) {
    std::cout << "cpu_pct=" << pct << "\n";
  } else {
    std::cout << pct << "%\n";
  }

  return 0;
}