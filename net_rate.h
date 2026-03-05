#pragma once

/**
 * @file net_rate.h
 * @brief Public interface for network throughput sampling.
 */

#include <string>

/**
 * @brief Result of a network throughput measurement.
 */
struct NetResult {
    std::string iface;          ///< Interface name (e.g. "eth0", "wlan0").
    double rx_Bps  = 0.0;      ///< Receive throughput in bytes/sec.
    double tx_Bps  = 0.0;      ///< Transmit throughput in bytes/sec.
    double rx_bps  = 0.0;      ///< Receive throughput in bits/sec.
    double tx_bps  = 0.0;      ///< Transmit throughput in bits/sec.
    bool   ok      = false;    ///< True if the measurement succeeded.
};

/**
 * @brief Attempts to find the interface used for the default route.
 * @return Interface name, or empty string if not found.
 */
std::string default_route_iface();

/**
 * @brief Returns the first non-loopback interface listed in /proc/net/dev.
 * @return Interface name, or empty string if not found.
 */
std::string first_non_lo_iface();

/**
 * @brief Reads cumulative RX and TX byte counters for an interface.
 * @param iface Interface name.
 * @param[out] rx_bytes Receive byte counter.
 * @param[out] tx_bytes Transmit byte counter.
 * @return True on success.
 */
bool read_net_bytes(const std::string& iface,
                    unsigned long long& rx_bytes,
                    unsigned long long& tx_bytes);

/**
 * @brief Convenience function: samples RX/TX throughput over the given interval.
 * @param interval_sec Sampling interval in seconds (clamped to >= 0).
 * @return NetResult with all fields filled, ok=true on success.
 */
NetResult sample_net(double interval_sec = 1.0);
