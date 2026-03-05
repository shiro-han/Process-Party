/**
 * @file net_rate.cpp
 * @brief Reports receive/transmit throughput for a selected network interface.
 */

#include "net_rate.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

std::string default_route_iface() {
  std::ifstream in("/proc/net/route");
  if (!in) return {};

  std::string line;
  if (!std::getline(in, line)) return {};  // header

  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string iface, dest, gateway, flags;
    if (!(iss >> iface >> dest >> gateway >> flags)) continue;
    if (dest == "00000000") return iface;
  }
  return {};
}

std::string first_non_lo_iface() {
  std::ifstream in("/proc/net/dev");
  if (!in) return {};

  std::string line;
  std::getline(in, line);  // header
  std::getline(in, line);  // header

  while (std::getline(in, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    std::string iface = line.substr(0, colon);
    while (!iface.empty() && (iface.front() == ' ' || iface.front() == '\t')) iface.erase(iface.begin());
    while (!iface.empty() && (iface.back()  == ' ' || iface.back()  == '\t')) iface.pop_back();

    if (iface != "lo" && !iface.empty()) return iface;
  }
  return {};
}

bool read_net_bytes(const std::string& iface,
                    unsigned long long& rx_bytes,
                    unsigned long long& tx_bytes) {
  std::ifstream in("/proc/net/dev");
  if (!in) return false;

  std::string line;
  std::getline(in, line);  // header
  std::getline(in, line);  // header

  while (std::getline(in, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;

    std::string ifname = line.substr(0, colon);
    while (!ifname.empty() && (ifname.front() == ' ' || ifname.front() == '\t')) ifname.erase(ifname.begin());
    while (!ifname.empty() && (ifname.back()  == ' ' || ifname.back()  == '\t')) ifname.pop_back();

    if (ifname != iface) continue;

    std::istringstream iss(line.substr(colon + 1));
    unsigned long long dummy = 0;
    iss >> rx_bytes;
    for (int i = 0; i < 7; ++i) iss >> dummy;
    iss >> tx_bytes;

    return !iss.fail();
  }
  return false;
}

NetResult sample_net(double interval_sec) {
  if (interval_sec < 0) interval_sec = 0;

  std::string iface = default_route_iface();
  if (iface.empty()) iface = first_non_lo_iface();
  if (iface.empty()) return {};

  unsigned long long rx1 = 0, tx1 = 0;
  if (!read_net_bytes(iface, rx1, tx1)) return {iface, 0, 0, 0, 0, false};

  const auto sleep_dur = std::chrono::duration<double>(interval_sec);
  if (sleep_dur.count() > 0)
    std::this_thread::sleep_for(
        std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_dur));

  unsigned long long rx2 = 0, tx2 = 0;
  if (!read_net_bytes(iface, rx2, tx2)) return {iface, 0, 0, 0, 0, false};

  const long long drx = static_cast<long long>(rx2) - static_cast<long long>(rx1);
  const long long dtx = static_cast<long long>(tx2) - static_cast<long long>(tx1);

  NetResult r;
  r.iface = iface;
  r.ok    = true;
  if (interval_sec > 0) {
    r.rx_Bps = static_cast<double>(drx) / interval_sec;
    r.tx_Bps = static_cast<double>(dtx) / interval_sec;
    r.rx_bps = r.rx_Bps * 8.0;
    r.tx_bps = r.tx_Bps * 8.0;
  }
  return r;
}
