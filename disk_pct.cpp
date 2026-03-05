/**
 * @file disk_pct.cpp
 * @brief Estimates disk busy percentage for the device backing the root filesystem.
 */

#include "disk_pct.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

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

    int major = 0, minor = 0;
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

static std::optional<std::string> base_device_name_from_majmin(int major, int minor) {
  namespace fs = std::filesystem;
  const fs::path link =
      fs::path("/sys/dev/block") / (std::to_string(major) + ":" + std::to_string(minor));

  std::error_code ec;
  const fs::path target = fs::read_symlink(link, ec);
  if (ec) return std::nullopt;

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

std::string root_disk_device() {
  const auto mm = root_major_minor();
  if (!mm) return {};
  const auto dev = base_device_name_from_majmin(mm->first, mm->second);
  return dev ? *dev : std::string{};
}

long long read_disk_io_ms(const std::string& dev) {
  std::ifstream in("/proc/diskstats");
  if (!in) return -1;

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    int major = 0, minor = 0;
    std::string name;
    if (!(iss >> major >> minor >> name)) continue;
    if (name != dev) continue;

    long long f4, f5, f6, f7, f8, f9, f10, f11, f12, f13;
    if (!(iss >> f4 >> f5 >> f6 >> f7 >> f8 >> f9 >> f10 >> f11 >> f12 >> f13))
      return -1;
    return f13;
  }
  return -1;
}

DiskResult sample_disk(double interval_sec) {
  if (interval_sec < 0) interval_sec = 0;

  const std::string dev = root_disk_device();
  if (dev.empty()) return {0.0, {}, false};

  const long long io1 = read_disk_io_ms(dev);
  if (io1 < 0) return {0.0, dev, false};

  const auto sleep_dur = std::chrono::duration<double>(interval_sec);
  if (sleep_dur.count() > 0)
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));

  const long long io2 = read_disk_io_ms(dev);
  if (io2 < 0) return {0.0, dev, false};

  double pct = 0.0;
  if (interval_sec > 0) {
    pct = (static_cast<double>(io2 - io1) / (interval_sec * 1000.0)) * 100.0;
    if (pct < 0.0)   pct = 0.0;
    if (pct > 100.0) pct = 100.0;
  }

  return {pct, dev, true};
}
