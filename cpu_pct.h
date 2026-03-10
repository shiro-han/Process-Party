#pragma once

/**
 * @file cpu_pct.h
 * @brief Public interface for CPU utilization sampling.
 */

/**
 * @brief Result of a CPU utilization measurement.
 */
struct CpuResult {
    double pct = 0.0;   ///< CPU utilization percentage [0, 100].
    bool ok  = false;   ///< True if the measurement succeeded.
};

/**
 * @brief Snapshot of aggregate CPU time fields from /proc/stat.
 */
struct CpuLine {
    long long user    = 0;
    long long nice    = 0;
    long long system  = 0;
    long long idle    = 0;
    long long iowait  = 0;
    long long irq     = 0;
    long long softirq = 0;
    long long steal   = 0;
};

/**
 * @brief Reads the aggregate CPU counters from /proc/stat.
 * @param[out] out Filled with parsed CPU counters on success.
 * @return True on success.
 */
bool read_cpu_line(CpuLine& out);

/**
 * @brief Computes CPU utilization percentage between two CpuLine samples.
 * @param a First (earlier) sample.
 * @param b Second (later) sample.
 * @return Utilization percentage in [0, 100], or 0.0 if delta is non-positive.
 */
double calc_cpu_pct(const CpuLine& a, const CpuLine& b);

/**
 * @brief Convenience function: samples CPU usage over the given interval.
 * @param interval_sec Sampling interval in seconds (clamped to >= 0).
 * @return CpuResult with pct filled and ok=true on success.
 */
CpuResult sample_cpu(double interval_sec = 1.0);
