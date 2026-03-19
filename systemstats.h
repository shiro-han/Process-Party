#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <string>
#include <vector>

struct CpuBreakdown
{
    double totalPercent = 0.0;
    double userPercent = 0.0;
    double systemPercent = 0.0;
    std::vector<double> perCorePercents;
};

struct MemoryBreakdown
{
    double totalMB = 0.0;
    double usedMB = 0.0;
    double availableMB = 0.0;
    double cachedMB = 0.0;
    double bufferedMB = 0.0;

    double swapTotalMB = 0.0;
    double swapUsedMB = 0.0;
    double swapFreeMB = 0.0;
    double swapPercent = 0.0;
};

struct DiskRates
{
    double readBytesPerSec = 0.0;
    double writeBytesPerSec = 0.0;

    double totalGB = 0.0;
    double usedGB = 0.0;
    double freeGB = 0.0;
};

struct InterfaceRate
{
    std::string name;
    double downloadBytesPerSec = 0.0;
    double uploadBytesPerSec = 0.0;
};

struct NetworkRates
{
    double downloadBytesPerSec = 0.0;
    double uploadBytesPerSec = 0.0;

    double totalDownloadedBytes = 0.0;
    double totalUploadedBytes = 0.0;

    std::string downloadText;
    std::string uploadText;
    std::vector<InterfaceRate> interfaces;
};

struct SystemData
{
    int cpuPercent = 0;
    int memoryPercent = 0;
    int diskPercent = 0;

    std::string networkDownloadText;
    double networkDownloadBytesPerSec = 0.0;

    CpuBreakdown cpu;
    MemoryBreakdown memory;
    DiskRates disk;
    NetworkRates network;
};

class SystemStats
{
public:
    static SystemData readSystemData();
    static std::string formatNetworkRate(double bytesPerSecond);
    static std::string formatBytes(double bytes);

private:
    struct CpuTimes
    {
        long long user = 0;
        long long nice = 0;
        long long system = 0;
        long long idle = 0;
        long long iowait = 0;
        long long irq = 0;
        long long softirq = 0;
        long long steal = 0;

        long long total() const
        {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }

        long long idleTotal() const
        {
            return idle + iowait;
        }

        long long userTotal() const
        {
            return user + nice;
        }

        long long systemTotal() const
        {
            return system + irq + softirq;
        }
    };

    struct CpuSnapshot
    {
        CpuTimes total;
        std::vector<CpuTimes> perCore;
    };

    static int readCpuPercent();
    static int readMemoryPercent();
    static int readDiskPercent();

    static CpuBreakdown readCpuBreakdown();
    static MemoryBreakdown readMemoryBreakdown();
    static DiskRates readDiskRates();

    static double readNetworkDownloadBytesPerSec();
    static NetworkRates readNetworkRates();

    static bool readCpuSnapshot(CpuSnapshot &snapshot);
};

#endif
