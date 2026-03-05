/**
 * @file cpu_pct.cpp
 * @brief Computes overall CPU utilization percentage from Linux /proc/stat.
 */

#include "cpu_pct.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

bool read_cpu_line(CpuLine& out) {
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

double calc_cpu_pct(const CpuLine& a, const CpuLine& b) {
  const long long idle1 = a.idle + a.iowait;
  const long long idle2 = b.idle + b.iowait;

  const long long non1 =
      a.user + a.nice + a.system + a.irq + a.softirq + a.steal;
  const long long non2 =
      b.user + b.nice + b.system + b.irq + b.softirq + b.steal;

  const long long total1 = idle1 + non1;
  const long long total2 = idle2 + non2;

  const long long dt    = total2 - total1;
  const long long didle = idle2 - idle1;

  if (dt <= 0) return 0.0;

  return (static_cast<double>(dt - didle) / static_cast<double>(dt)) * 100.0;
}

CpuResult sample_cpu(double interval_sec) {
  if (interval_sec < 0) interval_sec = 0;

  CpuLine a, b;
  if (!read_cpu_line(a)) return {0.0, false};

  const auto sleep_dur = std::chrono::duration<double>(interval_sec);
  if (sleep_dur.count() > 0)
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));

  if (!read_cpu_line(b)) return {0.0, false};

  return {calc_cpu_pct(a, b), true};
}

