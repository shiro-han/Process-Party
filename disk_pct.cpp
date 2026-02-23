/**
 * @file disk_pct.cpp
 * @brief Estimates disk busy percentage for the device backing the root filesystem.
 *
 * This utility determines which block device underlies the "/" mount, samples the
 * kernel's cumulative "time spent doing I/Os" counter from /proc/diskstats twice
 * (separated by a configurable interval), and reports the fraction of time the
 * device was busy during that interval as a percentage.
 *
 * Output formats:
 *  - Human: "<pct>%"
 *  - Raw:   "disk_pct=<pct> disk_dev=<device>"
 *
 * @note Linux-specific: relies on /proc/self/mountinfo, /sys/dev/block, and /proc/diskstats.
 */

#include <chrono>
#include <filesystem>
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
 * @brief Locates the major/minor device numbers for the filesystem mounted at "/".
 *
 * Parses /proc/self/mountinfo and returns the major:minor pair for the entry
 * whose mount point is "/".
 *
 * @return A pair {major, minor} on success; std::nullopt if the root mount entry
 *         cannot be found or parsed.
 */
static std::optional<std::pair<int, int>> root_major_minor() {
  std::ifstream in("/proc/self/mountinfo");
  if (!in) return std::nullopt;

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string mount_id, parent_id, majmin, root, mount_point;
    if (!(iss >> mount_id >> parent_id >> majmin >> root >> mount_point)) continue;
    if (mount_point != "/") continue;

    const auto pos = majmin.find(':');
    if (pos == std::string::npos) return std::nullopt;

    int major = 0;
    int minor = 0;
    try {
      major = std::stoi(majmin.substr(0, pos));
      minor = std::stoi(majmin.substr(pos + 1));
    } catch (...) {
      return std::nullopt;
    }
    return std::make_pair(major, minor);
  }
  return std::nullopt;
}

/**
 * @brief Resolves a major/minor pair to a base block device name.
 *
 * Uses /sys/dev/block/<major>:<minor> to locate the associated sysfs node. For
 * partitions, this resolves to the parent disk under the sysfs "block" tree
 * (e.g., "sda" for "sda1", "nvme0n1" for "nvme0n1p2").
 *
 * @param major Block device major number.
 * @param minor Block device minor number.
 * @return Base device name on success (e.g., "sda", "nvme0n1"); std::nullopt on
 *         lookup or parsing failure.
 *
 * @note This function derives the base device by locating the "block" directory
 *       component in the sysfs path and taking the subsequent component name.
 */
static std::optional<std::string> base_device_name_from_majmin(int major, int minor) {
  namespace fs = std::filesystem;
  const fs::path link =
      fs::path("/sys/dev/block") / (std::to_string(major) + ":" + std::to_string(minor));

  std::error_code ec;
  const fs::path target = fs::read_symlink(link, ec);
  if (ec) return std::nullopt;

  // Resolve symlink targets that are relative to /sys/dev/block.
  const fs::path abs_target = (link.parent_path() / target).lexically_normal();

  fs::path base;
  bool saw_block = false;
  for (auto it = abs_target.begin(); it != abs_target.end(); ++it) {
    if (*it == "block") {
      saw_block = true;
      auto next = it;
      ++next;
      if (next != abs_target.end()) base = *next;
      break;
    }
  }

  if (!saw_block || base.empty()) return std::nullopt;
  return base.string();
}

/**
 * @brief Reads the cumulative "time spent doing I/Os" counter for a block device.
 *
 * Scans /proc/diskstats for the given device name and extracts the field
 * commonly described as "time_spent_doing_ios_ms" (ms spent with I/Os in flight).
 *
 * @param dev Block device name as it appears in /proc/diskstats (e.g., "sda").
 * @return Milliseconds spent doing I/Os since boot for the device; std::nullopt
 *         if the device entry is not found or the line cannot be parsed.
 *
 * @note /proc/diskstats format may vary slightly across kernel versions; this
 *       parser assumes the standard Linux layout where the desired value is the
 *       10th numeric field following the device name (commonly field 13 overall).
 */
static std::optional<long long> read_time_doing_ios_ms(const std::string& dev) {
  std::ifstream in("/proc/diskstats");
  if (!in) return std::nullopt;

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    int major = 0, minor = 0;
    std::string name;
    if (!(iss >> major >> minor >> name)) continue;
    if (name != dev) continue;

    long long f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
    if (!(iss >> f4 >> f5 >> f6 >> f7 >> f8 >> f9 >> f10 >> f11 >> f12 >> f13)) {
      return std::nullopt;
    }
    return f13;
  }
  return std::nullopt;
}

/**
 * @brief Entry point.
 *
 * Command-line options:
 *  - --interval N : Sampling interval in seconds (double). Values < 0 are clamped to 0.
 *  - --raw        : Emit "disk_pct=<pct> disk_dev=<device>" instead of "<pct>%".
 *
 * Exit codes:
 *  - 0: Success.
 *  - 1: Failed to resolve root device or read kernel stats.
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

  const auto mm = root_major_minor();
  if (!mm) {
    std::cerr << "Error: could not determine root mount major:minor\n";
    return 1;
  }

  const auto base_dev = base_device_name_from_majmin(mm->first, mm->second);
  if (!base_dev) {
    std::cerr << "Error: could not determine root disk device\n";
    return 1;
  }

  const auto io1 = read_time_doing_ios_ms(*base_dev);
  if (!io1) {
    std::cerr << "Error: could not read diskstats for device '" << *base_dev << "'\n";
    return 1;
  }

  const auto sleep_dur = std::chrono::duration<double>(interval);
  if (sleep_dur.count() > 0) {
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));
  }

  const auto io2 = read_time_doing_ios_ms(*base_dev);
  if (!io2) {
    std::cerr << "Error: could not read diskstats for device '" << *base_dev
              << "' (second sample)\n";
    return 1;
  }

  const long long dio_ms = *io2 - *io1;

  double pct = 0.0;
  if (interval > 0) {
    pct = (static_cast<double>(dio_ms) / (interval * 1000.0)) * 100.0;
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;
  }

  std::cout << std::fixed << std::setprecision(1);
  if (raw) {
    std::cout << "disk_pct=" << pct << " disk_dev=" << *base_dev << "\n";
  } else {
    std::cout << pct << "%\n";
  }

  return 0;
}