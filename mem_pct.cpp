/**
 * @file mem_pct.cpp
 * @brief Computes memory utilization percentage from /proc/meminfo.
 */

#include "mem_pct.h"

#include <fstream>
#include <string>

bool read_meminfo(long long& total_kb, long long& avail_kb) {
  std::ifstream file("/proc/meminfo");
  if (!file) return false;

  std::string key, unit;
  long long value = 0;
  while (file >> key >> value >> unit) {
    if (key == "MemTotal:")     total_kb = value;
    else if (key == "MemAvailable:") avail_kb = value;
  }

  return (total_kb > 0 && avail_kb >= 0);
}

MemResult sample_mem() {
  MemResult r;
  if (!read_meminfo(r.total_kb, r.avail_kb)) return r;

  r.used_kb = r.total_kb - r.avail_kb;
  r.pct     = (static_cast<double>(r.used_kb) /
               static_cast<double>(r.total_kb)) * 100.0;
  r.ok      = true;
  return r;
}
