/**
 * @file net_rate.cpp
 * @brief Reports receive/transmit throughput for a selected network interface.
 *
 * The program selects a network interface (preferably the interface associated
 * with the default route), samples its RX/TX byte counters from /proc/net/dev
 * twice separated by a configurable interval, and reports average throughput
 * over that interval in bytes per second (and bits per second in raw mode).
 *
 * Output formats:
 *  - Human: "<iface>: RX <B/s>B/s TX <B/s>B/s"
 *  - Raw:   "net_if=<iface> rx_Bps=<...> tx_Bps=<...> rx_bps=<...> tx_bps=<...>"
 *
 * @note Linux-specific: relies on /proc/net/route and /proc/net/dev.
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

/**
 * @brief Prints program usage information to standard error.
 *
 * @param prog Program name (typically argv[0]).
 */
static void usage(const char* prog) {
  std::cerr << "Usage: " << prog << " [--interval N] [--raw]\n";
}

/**
 * @brief Attempts to identify the interface used for the default route.
 *
 * Parses /proc/net/route and returns the interface name for the entry whose
 * Destination field is "00000000" (the default route in this file's encoding).
 *
 * @return Interface name on success; std::nullopt if no default route entry is
 *         present or the file cannot be read.
 */
static std::optional<std::string> default_route_iface() {
  std::ifstream in("/proc/net/route");
  if (!in) return std::nullopt;

  std::string line;
  if (!std::getline(in, line)) return std::nullopt;  // header

  // Columns: Iface Destination Gateway Flags RefCnt Use Metric Mask ...
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string iface, dest, gateway, flags;
    if (!(iss >> iface >> dest >> gateway >> flags)) continue;

    if (dest == "00000000") {
      return iface;
    }
  }
  return std::nullopt;
}

/**
 * @brief Fallback interface selection when a default route cannot be determined.
 *
 * Scans /proc/net/dev and returns the first interface name that is not the
 * loopback device ("lo").
 *
 * @return Interface name on success; std::nullopt if no suitable interface is found.
 */
static std::optional<std::string> first_non_lo_iface() {
  std::ifstream in("/proc/net/dev");
  if (!in) return std::nullopt;

  std::string line;
  std::getline(in, line);  // header
  std::getline(in, line);  // header

  while (std::getline(in, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    std::string iface = line.substr(0, colon);

    // Trim leading/trailing whitespace.
    while (!iface.empty() && (iface.front() == ' ' || iface.front() == '\t')) iface.erase(iface.begin());
    while (!iface.empty() && (iface.back() == ' ' || iface.back() == '\t')) iface.pop_back();

    if (iface != "lo" && !iface.empty()) return iface;
  }
  return std::nullopt;
}

/**
 * @brief Reads cumulative RX/TX byte counters for a network interface.
 *
 * Parses /proc/net/dev and extracts the first receive field (rx_bytes) and
 * first transmit field (tx_bytes) for the requested interface.
 *
 * @param iface Interface name (e.g., "eth0", "wlan0").
 * @return Pair {rx_bytes, tx_bytes} on success; std::nullopt if the interface
 *         entry is not found or the line cannot be parsed.
 */
static std::optional<std::pair<unsigned long long, unsigned long long>>
read_net_bytes(const std::string& iface) {
  std::ifstream in("/proc/net/dev");
  if (!in) return std::nullopt;

  std::string line;
  std::getline(in, line);  // header
  std::getline(in, line);  // header

  while (std::getline(in, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    std::string ifname = line.substr(0, colon);

    // Trim leading/trailing whitespace.
    while (!ifname.empty() && (ifname.front() == ' ' || ifname.front() == '\t')) ifname.erase(ifname.begin());
    while (!ifname.empty() && (ifname.back() == ' ' || ifname.back() == '\t')) ifname.pop_back();

    if (ifname != iface) continue;

    // After colon:
    //   rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame rx_compressed rx_multicast
    //   tx_bytes tx_packets tx_errs tx_drop tx_fifo tx_colls tx_carrier tx_compressed
    std::istringstream iss(line.substr(colon + 1));
    unsigned long long rx_bytes = 0;
    unsigned long long tx_bytes = 0;
    unsigned long long dummy = 0;

    iss >> rx_bytes;
    for (int i = 0; i < 7; ++i) iss >> dummy;

    iss >> tx_bytes;

    if (iss.fail()) return std::nullopt;
    return std::make_pair(rx_bytes, tx_bytes);
  }

  return std::nullopt;
}

/**
 * @brief Entry point.
 *
 * Command-line options:
 *  - --interval N : Sampling interval in seconds (double). Values < 0 are clamped to 0.
 *  - --raw        : Emit detailed key/value output including bits-per-second.
 *
 * Exit codes:
 *  - 0: Success.
 *  - 1: Failed to select an interface or read kernel counters.
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

  std::string iface;
  if (const auto d = default_route_iface(); d) {
    iface = *d;
  } else if (const auto f = first_non_lo_iface(); f) {
    iface = *f;
  } else {
    std::cerr << "Error: could not determine network interface\n";
    return 1;
  }

  const auto b1 = read_net_bytes(iface);
  if (!b1) {
    std::cerr << "Error: could not read /proc/net/dev for interface '" << iface << "'\n";
    return 1;
  }

  const auto sleep_dur = std::chrono::duration<double>(interval);
  if (sleep_dur.count() > 0) {
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));
  }

  const auto b2 = read_net_bytes(iface);
  if (!b2) {
    std::cerr << "Error: could not read /proc/net/dev for interface '" << iface
              << "' (second sample)\n";
    return 1;
  }

  const long long drx =
      static_cast<long long>(b2->first) - static_cast<long long>(b1->first);
  const long long dtx =
      static_cast<long long>(b2->second) - static_cast<long long>(b1->second);

  double rx_Bps = 0.0, tx_Bps = 0.0, rx_bps = 0.0, tx_bps = 0.0;
  if (interval > 0) {
    rx_Bps = static_cast<double>(drx) / interval;
    tx_Bps = static_cast<double>(dtx) / interval;
    rx_bps = (static_cast<double>(drx) * 8.0) / interval;
    tx_bps = (static_cast<double>(dtx) * 8.0) / interval;
  }

  const auto as_int_str = [](double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << v;
    return oss.str();
  };

  if (raw) {
    std::cout << "net_if=" << iface
              << " rx_Bps=" << as_int_str(rx_Bps)
              << " tx_Bps=" << as_int_str(tx_Bps)
              << " rx_bps=" << as_int_str(rx_bps)
              << " tx_bps=" << as_int_str(tx_bps)
              << "\n";
  } else {
    std::cout << iface << ": RX " << as_int_str(rx_Bps) << "B/s"
              << " TX " << as_int_str(tx_Bps) << "B/s\n";
  }

  return 0;
}