#include "systemstats.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <sys/statvfs.h>

SystemData SystemStats::readSystemData()
{
    SystemData data;
    data.cpuPercent = readCpuPercent();
    data.memoryPercent = readMemoryPercent();
    data.diskPercent = readDiskPercent();
    data.networkDownloadText = readNetworkDownloadText();
    return data;
}

int SystemStats::readCpuPercent()
{
    auto readCpuLine = [](long long &idle, long long &total) -> bool
    {
        std::ifstream file("/proc/stat");
        if (!file.is_open())
            return false;

        std::string cpu;
        long long user = 0;
        long long nice = 0;
        long long system = 0;
        long long idleTime = 0;
        long long iowait = 0;
        long long irq = 0;
        long long softirq = 0;
        long long steal = 0;

        file >> cpu >> user >> nice >> system >> idleTime >> iowait >> irq >> softirq >> steal;

        idle = idleTime + iowait;
        total = user + nice + system + idleTime + iowait + irq + softirq + steal;
        return true;
    };

    long long idle1 = 0;
    long long total1 = 0;
    long long idle2 = 0;
    long long total2 = 0;

    if (!readCpuLine(idle1, total1))
        return 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (!readCpuLine(idle2, total2))
        return 0;

    long long idleDiff = idle2 - idle1;
    long long totalDiff = total2 - total1;

    if (totalDiff <= 0)
        return 0;

    double usage = 100.0 * (1.0 - static_cast<double>(idleDiff) / static_cast<double>(totalDiff));

    if (usage < 0.0)
        usage = 0.0;
    if (usage > 100.0)
        usage = 100.0;

    return static_cast<int>(usage);
}

int SystemStats::readMemoryPercent()
{
    std::ifstream file("/proc/meminfo");
    if (!file.is_open())
        return 0;

    std::string key;
    long long value = 0;
    std::string unit;

    long long memTotal = 0;
    long long memAvailable = 0;

    while (file >> key >> value >> unit)
    {
        if (key == "MemTotal:")
            memTotal = value;
        else if (key == "MemAvailable:")
            memAvailable = value;

        if (memTotal > 0 && memAvailable > 0)
            break;
    }

    if (memTotal <= 0)
        return 0;

    long long used = memTotal - memAvailable;
    double usage = 100.0 * static_cast<double>(used) / static_cast<double>(memTotal);

    if (usage < 0.0)
        usage = 0.0;
    if (usage > 100.0)
        usage = 100.0;

    return static_cast<int>(usage);
}

int SystemStats::readDiskPercent()
{
    struct statvfs fsInfo;

    if (statvfs("/", &fsInfo) != 0)
        return 0;

    unsigned long long total = static_cast<unsigned long long>(fsInfo.f_blocks) * fsInfo.f_frsize;
    unsigned long long available = static_cast<unsigned long long>(fsInfo.f_bavail) * fsInfo.f_frsize;

    if (total == 0)
        return 0;

    unsigned long long used = total - available;
    double usage = 100.0 * static_cast<double>(used) / static_cast<double>(total);

    if (usage < 0.0)
        usage = 0.0;
    if (usage > 100.0)
        usage = 100.0;

    return static_cast<int>(usage);
}

std::string SystemStats::readNetworkDownloadText()
{
    auto readTotalRxBytes = []() -> unsigned long long
    {
        std::ifstream file("/proc/net/dev");
        if (!file.is_open())
            return 0;

        std::string line;
        unsigned long long totalRx = 0;

        std::getline(file, line);
        std::getline(file, line);

        while (std::getline(file, line))
        {
            std::size_t colonPos = line.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string iface = line.substr(0, colonPos);
            std::string data = line.substr(colonPos + 1);

            std::stringstream ifaceStream(iface);
            ifaceStream >> iface;

            if (iface == "lo")
                continue;

            std::stringstream ss(data);

            unsigned long long rxBytes = 0;
            ss >> rxBytes;

            totalRx += rxBytes;
        }

        return totalRx;
    };

    static bool initialized = false;
    static unsigned long long previousRxBytes = 0;
    static auto previousTime = std::chrono::steady_clock::now();

    unsigned long long currentRxBytes = readTotalRxBytes();
    auto currentTime = std::chrono::steady_clock::now();

    if (!initialized)
    {
        initialized = true;
        previousRxBytes = currentRxBytes;
        previousTime = currentTime;
        return "0 B/s";
    }

    std::chrono::duration<double> elapsed = currentTime - previousTime;
    double seconds = elapsed.count();

    if (seconds <= 0.0)
        return "0 B/s";

    unsigned long long byteDiff = 0;
    if (currentRxBytes >= previousRxBytes)
        byteDiff = currentRxBytes - previousRxBytes;

    double bytesPerSecond = static_cast<double>(byteDiff) / seconds;

    previousRxBytes = currentRxBytes;
    previousTime = currentTime;

    std::ostringstream out;
    out.setf(std::ios::fixed);

    if (bytesPerSecond >= 1024.0 * 1024.0)
    {
        out.precision(1);
        out << (bytesPerSecond / (1024.0 * 1024.0)) << " MB/s";
    }
    else if (bytesPerSecond >= 1024.0)
    {
        out.precision(1);
        out << (bytesPerSecond / 1024.0) << " KB/s";
    }
    else
    {
        out.precision(0);
        out << bytesPerSecond << " B/s";
    }

    return out.str();
}