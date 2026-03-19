#include "systemstats.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/statvfs.h>

SystemData SystemStats::readSystemData()
{
    SystemData data;

    data.cpu = readCpuBreakdown();
    data.cpuPercent = static_cast<int>(data.cpu.totalPercent);

    data.memory = readMemoryBreakdown();
    data.memoryPercent = static_cast<int>(
        std::clamp(
            (data.memory.totalMB > 0.0)
                ? (100.0 * data.memory.usedMB / data.memory.totalMB)
                : 0.0,
            0.0,
            100.0
            )
        );

    data.diskPercent = readDiskPercent();
    data.disk = readDiskRates();

    data.network = readNetworkRates();
    data.networkDownloadBytesPerSec = data.network.downloadBytesPerSec;
    data.networkDownloadText = data.network.downloadText;

    return data;
}

bool SystemStats::readCpuSnapshot(CpuSnapshot &snapshot)
{
    snapshot = CpuSnapshot{};

    std::ifstream file("/proc/stat");
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::istringstream iss(line);
        std::string label;
        iss >> label;

        if (label == "cpu")
        {
            CpuTimes t;
            iss >> t.user
                >> t.nice
                >> t.system
                >> t.idle
                >> t.iowait
                >> t.irq
                >> t.softirq
                >> t.steal;

            if (iss.fail())
                return false;

            snapshot.total = t;
        }
        else if (label.rfind("cpu", 0) == 0 && label.size() > 3)
        {
            CpuTimes t;
            iss >> t.user
                >> t.nice
                >> t.system
                >> t.idle
                >> t.iowait
                >> t.irq
                >> t.softirq
                >> t.steal;

            if (!iss.fail())
                snapshot.perCore.push_back(t);
        }
    }

    return true;
}

CpuBreakdown SystemStats::readCpuBreakdown()
{
    static bool initialized = false;
    static CpuSnapshot previousSnapshot;

    CpuBreakdown result;

    CpuSnapshot currentSnapshot;
    if (!readCpuSnapshot(currentSnapshot))
        return result;

    if (!initialized)
    {
        previousSnapshot = currentSnapshot;
        initialized = true;
        return result;
    }

    auto computePercent = [](long long partDelta, long long totalDelta) -> double
    {
        if (totalDelta <= 0)
            return 0.0;

        double percent = 100.0 * static_cast<double>(partDelta) / static_cast<double>(totalDelta);
        return std::clamp(percent, 0.0, 100.0);
    };

    {
        long long totalDelta = currentSnapshot.total.total() - previousSnapshot.total.total();
        long long idleDelta = currentSnapshot.total.idleTotal() - previousSnapshot.total.idleTotal();
        long long userDelta = currentSnapshot.total.userTotal() - previousSnapshot.total.userTotal();
        long long systemDelta = currentSnapshot.total.systemTotal() - previousSnapshot.total.systemTotal();

        if (totalDelta > 0)
        {
            double totalPercent =
                100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta));

            result.totalPercent = std::clamp(totalPercent, 0.0, 100.0);
            result.userPercent = computePercent(userDelta, totalDelta);
            result.systemPercent = computePercent(systemDelta, totalDelta);
        }
    }

    std::size_t coreCount = std::min(currentSnapshot.perCore.size(), previousSnapshot.perCore.size());
    result.perCorePercents.reserve(coreCount);

    for (std::size_t i = 0; i < coreCount; ++i)
    {
        const CpuTimes &curr = currentSnapshot.perCore[i];
        const CpuTimes &prev = previousSnapshot.perCore[i];

        long long totalDelta = curr.total() - prev.total();
        long long idleDelta = curr.idleTotal() - prev.idleTotal();

        double percent = 0.0;
        if (totalDelta > 0)
        {
            percent = 100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta));
            percent = std::clamp(percent, 0.0, 100.0);
        }

        result.perCorePercents.push_back(percent);
    }

    previousSnapshot = currentSnapshot;
    return result;
}

MemoryBreakdown SystemStats::readMemoryBreakdown()
{
    MemoryBreakdown result;

    std::ifstream file("/proc/meminfo");
    if (!file.is_open())
        return result;

    long long memTotalKB = 0;
    long long memAvailableKB = 0;
    long long cachedKB = 0;
    long long buffersKB = 0;
    long long swapTotalKB = 0;
    long long swapFreeKB = 0;

    std::string key;
    long long value = 0;
    std::string unit;

    while (file >> key >> value >> unit)
    {
        if (key == "MemTotal:")
            memTotalKB = value;
        else if (key == "MemAvailable:")
            memAvailableKB = value;
        else if (key == "Cached:")
            cachedKB = value;
        else if (key == "Buffers:")
            buffersKB = value;
        else if (key == "SwapTotal:")
            swapTotalKB = value;
        else if (key == "SwapFree:")
            swapFreeKB = value;
    }

    if (memTotalKB > 0)
    {
        long long usedKB = memTotalKB - memAvailableKB;
        if (usedKB < 0)
            usedKB = 0;

        result.totalMB = static_cast<double>(memTotalKB) / 1024.0;
        result.usedMB = static_cast<double>(usedKB) / 1024.0;
        result.availableMB = static_cast<double>(std::max<long long>(0, memAvailableKB)) / 1024.0;
        result.cachedMB = static_cast<double>(std::max<long long>(0, cachedKB)) / 1024.0;
        result.bufferedMB = static_cast<double>(std::max<long long>(0, buffersKB)) / 1024.0;
    }

    if (swapTotalKB > 0)
    {
        long long swapUsedKB = swapTotalKB - swapFreeKB;
        if (swapUsedKB < 0)
            swapUsedKB = 0;

        result.swapTotalMB = static_cast<double>(swapTotalKB) / 1024.0;
        result.swapUsedMB = static_cast<double>(swapUsedKB) / 1024.0;
        result.swapFreeMB = static_cast<double>(std::max<long long>(0, swapFreeKB)) / 1024.0;
        result.swapPercent = std::clamp(
            100.0 * static_cast<double>(swapUsedKB) / static_cast<double>(swapTotalKB),
            0.0,
            100.0
            );
    }

    return result;
}

DiskRates SystemStats::readDiskRates()
{
    struct DiskCounters
    {
        unsigned long long readSectors = 0;
        unsigned long long writeSectors = 0;
    };

    auto readCounters = []() -> DiskCounters
    {
        std::ifstream file("/proc/diskstats");
        DiskCounters total;

        if (!file.is_open())
            return total;

        std::set<std::string> ignoredPrefixes = {
            "loop", "ram", "fd", "sr", "dm-", "md"
        };

        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream iss(line);

            int major = 0;
            int minor = 0;
            std::string name;

            unsigned long long readsCompleted = 0;
            unsigned long long readsMerged = 0;
            unsigned long long sectorsRead = 0;
            unsigned long long timeReadingMs = 0;
            unsigned long long writesCompleted = 0;
            unsigned long long writesMerged = 0;
            unsigned long long sectorsWritten = 0;
            unsigned long long timeWritingMs = 0;

            iss >> major
                >> minor
                >> name
                >> readsCompleted
                >> readsMerged
                >> sectorsRead
                >> timeReadingMs
                >> writesCompleted
                >> writesMerged
                >> sectorsWritten
                >> timeWritingMs;

            if (iss.fail())
                continue;

            bool ignore = false;
            for (const std::string &prefix : ignoredPrefixes)
            {
                if (name.rfind(prefix, 0) == 0)
                {
                    ignore = true;
                    break;
                }
            }

            if (ignore)
                continue;

            if (!name.empty() && std::isdigit(static_cast<unsigned char>(name.back())))
                continue;

            total.readSectors += sectorsRead;
            total.writeSectors += sectorsWritten;
        }

        return total;
    };

    static bool initialized = false;
    static DiskCounters previous;
    static auto previousTime = std::chrono::steady_clock::now();

    DiskRates rates;

    DiskCounters current = readCounters();
    auto currentTime = std::chrono::steady_clock::now();

    if (!initialized)
    {
        initialized = true;
        previous = current;
        previousTime = currentTime;
    }
    else
    {
        std::chrono::duration<double> elapsed = currentTime - previousTime;
        double seconds = elapsed.count();

        if (seconds > 0.0)
        {
            unsigned long long readDiffSectors = 0;
            unsigned long long writeDiffSectors = 0;

            if (current.readSectors >= previous.readSectors)
                readDiffSectors = current.readSectors - previous.readSectors;

            if (current.writeSectors >= previous.writeSectors)
                writeDiffSectors = current.writeSectors - previous.writeSectors;

            constexpr double bytesPerSector = 512.0;

            rates.readBytesPerSec =
                (static_cast<double>(readDiffSectors) * bytesPerSector) / seconds;

            rates.writeBytesPerSec =
                (static_cast<double>(writeDiffSectors) * bytesPerSector) / seconds;
        }

        previous = current;
        previousTime = currentTime;
    }

    struct statvfs fsInfo;
    if (statvfs("/", &fsInfo) == 0)
    {
        unsigned long long totalBytes =
            static_cast<unsigned long long>(fsInfo.f_blocks) * fsInfo.f_frsize;

        unsigned long long freeBytes =
            static_cast<unsigned long long>(fsInfo.f_bavail) * fsInfo.f_frsize;

        unsigned long long usedBytes = totalBytes - freeBytes;

        rates.totalGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
        rates.usedGB  = static_cast<double>(usedBytes) / (1024.0 * 1024.0 * 1024.0);
        rates.freeGB  = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
    }

    return rates;
}

int SystemStats::readCpuPercent()
{
    CpuBreakdown cpu = readCpuBreakdown();
    return static_cast<int>(cpu.totalPercent);
}

int SystemStats::readMemoryPercent()
{
    MemoryBreakdown memory = readMemoryBreakdown();
    if (memory.totalMB <= 0.0)
        return 0;

    double usage = 100.0 * memory.usedMB / memory.totalMB;
    usage = std::clamp(usage, 0.0, 100.0);
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
    usage = std::clamp(usage, 0.0, 100.0);

    return static_cast<int>(usage);
}

double SystemStats::readNetworkDownloadBytesPerSec()
{
    NetworkRates rates = readNetworkRates();
    return rates.downloadBytesPerSec;
}

std::string SystemStats::formatNetworkRate(double bytesPerSecond)
{
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

std::string SystemStats::formatBytes(double bytes)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);

    if (bytes >= 1024.0 * 1024.0 * 1024.0)
    {
        out.precision(1);
        out << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    }
    else if (bytes >= 1024.0 * 1024.0)
    {
        out.precision(1);
        out << (bytes / (1024.0 * 1024.0)) << " MB";
    }
    else if (bytes >= 1024.0)
    {
        out.precision(1);
        out << (bytes / 1024.0) << " KB";
    }
    else
    {
        out.precision(0);
        out << bytes << " B";
    }

    return out.str();
}

NetworkRates SystemStats::readNetworkRates()
{
    struct NetTotals
    {
        unsigned long long rxBytes = 0;
        unsigned long long txBytes = 0;
    };

    auto readTotals = []() -> std::map<std::string, NetTotals>
    {
        std::ifstream file("/proc/net/dev");
        std::map<std::string, NetTotals> totals;

        if (!file.is_open())
            return totals;

        std::string line;
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
            unsigned long long rxPackets = 0;
            unsigned long long rxErrs = 0;
            unsigned long long rxDrop = 0;
            unsigned long long rxFifo = 0;
            unsigned long long rxFrame = 0;
            unsigned long long rxCompressed = 0;
            unsigned long long rxMulticast = 0;

            unsigned long long txBytes = 0;
            unsigned long long txPackets = 0;
            unsigned long long txErrs = 0;
            unsigned long long txDrop = 0;
            unsigned long long txFifo = 0;
            unsigned long long txColls = 0;
            unsigned long long txCarrier = 0;
            unsigned long long txCompressed = 0;

            ss >> rxBytes
                >> rxPackets
                >> rxErrs
                >> rxDrop
                >> rxFifo
                >> rxFrame
                >> rxCompressed
                >> rxMulticast
                >> txBytes
                >> txPackets
                >> txErrs
                >> txDrop
                >> txFifo
                >> txColls
                >> txCarrier
                >> txCompressed;

            totals[iface] = {rxBytes, txBytes};
        }

        return totals;
    };

    static bool initialized = false;
    static std::map<std::string, NetTotals> previousTotals;
    static auto previousTime = std::chrono::steady_clock::now();

    NetworkRates rates;

    std::map<std::string, NetTotals> currentTotals = readTotals();
    auto currentTime = std::chrono::steady_clock::now();

    unsigned long long absoluteRxTotal = 0;
    unsigned long long absoluteTxTotal = 0;

    for (const auto &[iface, totals] : currentTotals)
    {
        absoluteRxTotal += totals.rxBytes;
        absoluteTxTotal += totals.txBytes;
    }

    rates.totalDownloadedBytes = static_cast<double>(absoluteRxTotal);
    rates.totalUploadedBytes = static_cast<double>(absoluteTxTotal);

    if (!initialized)
    {
        initialized = true;
        previousTotals = currentTotals;
        previousTime = currentTime;
        rates.downloadText = "0 B/s";
        rates.uploadText = "0 B/s";
        return rates;
    }

    std::chrono::duration<double> elapsed = currentTime - previousTime;
    double seconds = elapsed.count();
    if (seconds <= 0.0)
    {
        rates.downloadText = "0 B/s";
        rates.uploadText = "0 B/s";
        return rates;
    }

    unsigned long long totalRxDiff = 0;
    unsigned long long totalTxDiff = 0;

    for (const auto &[iface, current] : currentTotals)
    {
        auto it = previousTotals.find(iface);
        unsigned long long prevRx = 0;
        unsigned long long prevTx = 0;

        if (it != previousTotals.end())
        {
            prevRx = it->second.rxBytes;
            prevTx = it->second.txBytes;
        }

        unsigned long long rxDiff = 0;
        unsigned long long txDiff = 0;

        if (current.rxBytes >= prevRx)
            rxDiff = current.rxBytes - prevRx;
        if (current.txBytes >= prevTx)
            txDiff = current.txBytes - prevTx;

        totalRxDiff += rxDiff;
        totalTxDiff += txDiff;

        InterfaceRate ifaceRate;
        ifaceRate.name = iface;
        ifaceRate.downloadBytesPerSec = static_cast<double>(rxDiff) / seconds;
        ifaceRate.uploadBytesPerSec = static_cast<double>(txDiff) / seconds;
        rates.interfaces.push_back(ifaceRate);
    }

    std::sort(rates.interfaces.begin(), rates.interfaces.end(),
              [](const InterfaceRate &a, const InterfaceRate &b)
              {
                  return (a.downloadBytesPerSec + a.uploadBytesPerSec) >
                         (b.downloadBytesPerSec + b.uploadBytesPerSec);
              });

    rates.downloadBytesPerSec = static_cast<double>(totalRxDiff) / seconds;
    rates.uploadBytesPerSec = static_cast<double>(totalTxDiff) / seconds;
    rates.downloadText = formatNetworkRate(rates.downloadBytesPerSec);
    rates.uploadText = formatNetworkRate(rates.uploadBytesPerSec);

    previousTotals = currentTotals;
    previousTime = currentTime;

    return rates;
}
