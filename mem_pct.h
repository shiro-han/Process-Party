#pragma once

/**
 * @file mem_pct.h
 * @brief Public interface for memory utilization sampling.
 */

/**
 * @brief Result of a memory utilization measurement.
 */
struct MemResult {
    double    pct          = 0.0; ///< Memory usage percentage [0, 100].
    long long total_kb     = 0;   ///< MemTotal in kilobytes.
    long long avail_kb     = 0;   ///< MemAvailable in kilobytes.
    long long used_kb      = 0;   ///< Computed used memory (total - avail) in KB.
    bool      ok           = false;///< True if the measurement succeeded.
};

/**
 * @brief Reads memory statistics from /proc/meminfo.
 * @param[out] total_kb  Filled with MemTotal on success.
 * @param[out] avail_kb  Filled with MemAvailable on success.
 * @return True on success.
 */
bool read_meminfo(long long& total_kb, long long& avail_kb);

/**
 * @brief Convenience function: samples current memory usage.
 *
 * Unlike the CPU/disk/net samplers, memory is a point-in-time read —
 * no interval is needed.
 *
 * @return MemResult with all fields filled, ok=true on success.
 */
MemResult sample_mem();
