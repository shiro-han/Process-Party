#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <string>

struct SystemData
{
    int cpuPercent = 0;
    int memoryPercent = 0;
    int diskPercent = 0;
    std::string networkDownloadText;
};

class SystemStats
{
public:
    static SystemData readSystemData();

private:
    static int readCpuPercent();
    static int readMemoryPercent();
    static int readDiskPercent();
    static std::string readNetworkDownloadText();
};

#endif