#ifndef PROCESSTABLE_H
#define PROCESSTABLE_H

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

struct ProcSnap
{
    long long cpuTicks = 0;
    long long startTimeTicks = 0;
    unsigned long long readBytes = 0;
    unsigned long long writeBytes = 0;
};

struct ProcessRow
{
    int pid = 0;
    long long startTimeTicks = 0;
    std::string name;
    char state = '?';

    double cpuPercent = 0.0;
    double cpuTimeSec = 0.0;
    long long threads = 0;

    double rssMB = 0.0;
    double vszMB = 0.0;
    double sharedMB = 0.0;

    long long readBytesPerSec = 0;
    long long writeBytesPerSec = 0;
    unsigned long long totalReadBytes = 0;
    unsigned long long totalWriteBytes = 0;

    long long priority = 0;
    long long niceValue = 0;
};

struct ProcInfoNow
{
    std::string name;
    char state = '?';
    long long cpuTicks = 0;
    long long startTimeTicks = 0;
    long long threads = 0;

    long long rssKB = 0;
    long long vszKB = 0;
    long long sharedKB = 0;

    unsigned long long readBytes = 0;
    unsigned long long writeBytes = 0;

    long long priority = 0;
    long long niceValue = 0;
};

class ProcessTable
{
public:
    ProcessTable();

    std::vector<ProcessRow> readProcesses();
    void setMaxRows(std::size_t maxRows);

private:
    std::unordered_map<int, ProcSnap> previousSnapshot;
    long long previousTotalCpuTicks;
    std::chrono::steady_clock::time_point previousTime;
    bool initialized;
    std::size_t maxRows;

    static bool isAllDigits(const std::string &s);
    static std::optional<long long> readTotalCpuTicks();

    static bool readProcStat(int pid,
                         std::string &outName,
                         char &outState,
                         long long &outUtime,
                         long long &outStime,
                         long long &outThreads,
                         long long &outPriority,
                         long long &outNice,
                         long long &outStartTimeTicks);

    static void readMemoryFields(int pid,
                                 long long &outVmRssKB,
                                 long long &outVmSizeKB,
                                 long long &outRssFileKB);

    static bool readProcIo(int pid,
                           unsigned long long &outRead,
                           unsigned long long &outWrite);

    static std::unordered_map<int, ProcSnap> takeSnapshot();
    static std::optional<ProcInfoNow> readProcInfoNow(int pid);
};

#endif
