#pragma once

/**
 * @file disk_pct.h
 * @brief Public interface for disk I/O busy-percentage sampling.
 */

#include <string>

/**
 * @brief Result of a disk utilization measurement.
 */
struct DiskResult {
    double      pct     = 0.0;  ///< Disk busy percentage [0, 100].
    std::string dev;            ///< Block device name (e.g. "sda", "nvme0n1").
    bool        ok      = false;///< True if the measurement succeeded.
};

/**
 * @brief Resolves the block device name backing the root filesystem ("/").
 * @return Device name (e.g. "sda") on success, empty string on failure.
 */
std::string root_disk_device();

/**
 * @brief Reads the cumulative "time spent doing I/Os" counter (ms) for a device.
 * @param dev Device name as it appears in /proc/diskstats.
 * @return Milliseconds value, or -1 on failure.
 */
long long read_disk_io_ms(const std::string& dev);

/**
 * @brief Convenience function: samples disk busy % over the given interval.
 * @param interval_sec Sampling interval in seconds (clamped to >= 0).
 * @return DiskResult with pct and dev filled, ok=true on success.
 */
DiskResult sample_disk(double interval_sec = 1.0);
