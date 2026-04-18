#include "processtable.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

ProcessTable::ProcessTable()
    : previousTotalCpuTicks(0),
      previousTime(std::chrono::steady_clock::now()),
      initialized(false),
      maxRows(50)
{
}

void ProcessTable::setMaxRows(std::size_t rows)
{
    maxRows = rows;
}

bool ProcessTable::isAllDigits(const std::string &s)
{
    return !s.empty() &&
           std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isdigit(c); });
}

std::optional<long long> ProcessTable::readTotalCpuTicks()
{
    std::ifstream in("/proc/stat");
    if (!in)
        return std::nullopt;

    std::string tag;
    if (!(in >> tag))
        return std::nullopt;

    if (tag != "cpu")
        return std::nullopt;

    long long sum = 0;
    long long value = 0;

    std::string lineRest;
    std::getline(in, lineRest);

    std::istringstream iss(lineRest);
    while (iss >> value)
        sum += value;

    return sum;
}

bool ProcessTable::readProcStat(int pid,
                                std::string &outName,
                                char &outState,
                                long long &outUtime,
                                long long &outStime,
                                long long &outThreads,
                                long long &outPriority,
                                long long &outNice,
                                long long &outStartTimeTicks)
{
    std::ifstream in("/proc/" + std::to_string(pid) + "/stat");
    if (!in)
        return false;

    std::string statLine;
    std::getline(in, statLine);
    if (statLine.empty())
        return false;

    std::size_t lpar = statLine.find('(');
    std::size_t rpar = statLine.rfind(')');

    if (lpar == std::string::npos || rpar == std::string::npos || rpar <= lpar)
        return false;

    outName = statLine.substr(lpar + 1, rpar - lpar - 1);

    if (rpar + 2 > statLine.size())
        return false;

    std::string rest = statLine.substr(rpar + 2);
    std::istringstream iss(rest);

    iss >> outState;
    if (iss.fail())
        return false;

    // After the process name:
    // 1  = state
    // 12 = utime
    // 13 = stime
    // 16 = priority
    // 17 = nice
    // 18 = num_threads
    std::string token;
    long long utime = 0;
    long long stime = 0;
    long long priority = 0;
    long long niceValue = 0;
    long long threads = 0;
    long long startTimeTicks = 0;

    for (int field = 2; field <= 20; ++field)
    {
        if (!(iss >> token))
            return false;

        try
        {
            if (field == 12)
                utime = std::stoll(token);
            else if (field == 13)
                stime = std::stoll(token);
            else if (field == 16)
                priority = std::stoll(token);
            else if (field == 17)
                niceValue = std::stoll(token);
            else if (field == 18)
                threads = std::stoll(token);
            else if (field == 20)
                startTimeTicks = std::stoll(token);
        }
        catch (...)
        {
            if (field == 18)
                threads = 0;
            else if (field == 20)
                startTimeTicks = 0;
            else
                return false;
        }
    }

    outUtime = utime;
    outStime = stime;
    outThreads = threads;
    outPriority = priority;
    outNice = niceValue;
    outStartTimeTicks = startTimeTicks;
    return true;
}

void ProcessTable::readMemoryFields(int pid,
                                    long long &outVmRssKB,
                                    long long &outVmSizeKB,
                                    long long &outRssFileKB)
{
    outVmRssKB = 0;
    outVmSizeKB = 0;
    outRssFileKB = 0;

    std::ifstream in("/proc/" + std::to_string(pid) + "/status");
    if (!in)
        return;

    std::string key;
    while (in >> key)
    {
        if (key == "VmRSS:")
        {
            long long kb = 0;
            in >> kb;
            outVmRssKB = std::max<long long>(0, kb);
        }
        else if (key == "VmSize:")
        {
            long long kb = 0;
            in >> kb;
            outVmSizeKB = std::max<long long>(0, kb);
        }
        else if (key == "RssFile:")
        {
            long long kb = 0;
            in >> kb;
            outRssFileKB = std::max<long long>(0, kb);
        }

        std::string dummy;
        std::getline(in, dummy);
    }
}

bool ProcessTable::readProcIo(int pid,
                              unsigned long long &outRead,
                              unsigned long long &outWrite)
{
    std::ifstream in("/proc/" + std::to_string(pid) + "/io");
    if (!in)
        return false;

    outRead = 0;
    outWrite = 0;

    std::string key;
    unsigned long long value = 0;

    while (in >> key >> value)
    {
        if (key == "read_bytes:")
            outRead = value;
        else if (key == "write_bytes:")
            outWrite = value;
    }

    return true;
}

std::unordered_map<int, ProcSnap> ProcessTable::takeSnapshot()
{
    std::unordered_map<int, ProcSnap> snapshot;

    for (const auto &entry : fs::directory_iterator("/proc"))
    {
        if (!entry.is_directory())
            continue;

        const std::string pidStr = entry.path().filename().string();
        if (!isAllDigits(pidStr))
            continue;

        int pid = 0;
        try
        {
            pid = std::stoi(pidStr);
        }
        catch (...)
        {
            continue;
        }

        std::string name;
        char state = '?';
        long long utime = 0;
        long long stime = 0;
        long long threads = 0;
        long long priority = 0;
        long long niceValue = 0;
        long long startTimeTicks = 0;

        if (!readProcStat(pid, name, state, utime, stime, threads, priority, niceValue, startTimeTicks))
            continue;

        ProcSnap snap;
        snap.cpuTicks = utime + stime;
        snap.startTimeTicks = startTimeTicks;

        unsigned long long r = 0;
        unsigned long long w = 0;
        if (readProcIo(pid, r, w))
        {
            snap.readBytes = r;
            snap.writeBytes = w;
        }

        snapshot.emplace(pid, snap);
    }

    return snapshot;
}

std::optional<ProcInfoNow> ProcessTable::readProcInfoNow(int pid)
{
    ProcInfoNow info;

    long long utime = 0;
    long long stime = 0;
    long long threads = 0;
    long long priority = 0;
    long long niceValue = 0;
    long long startTimeTicks = 0;

    if (!readProcStat(pid, info.name, info.state, utime, stime, threads, priority, niceValue, startTimeTicks))
        return std::nullopt;

    info.cpuTicks = utime + stime;
    info.startTimeTicks = startTimeTicks;
    info.threads = threads;
    info.priority = priority;
    info.niceValue = niceValue;

    readMemoryFields(pid, info.rssKB, info.vszKB, info.sharedKB);

    unsigned long long r = 0;
    unsigned long long w = 0;
    if (readProcIo(pid, r, w))
    {
        info.readBytes = r;
        info.writeBytes = w;
    }
    else
    {
        info.readBytes = 0;
        info.writeBytes = 0;
    }

    return info;
}

std::vector<ProcessRow> ProcessTable::readProcesses()
{
    std::vector<ProcessRow> rows;

    const auto totalCpuOpt = readTotalCpuTicks();
    if (!totalCpuOpt)
        return rows;

    const long long currentTotalCpuTicks = *totalCpuOpt;
    const auto currentTime = std::chrono::steady_clock::now();
    const auto currentSnapshot = takeSnapshot();

    if (!initialized)
    {
        previousSnapshot = currentSnapshot;
        previousTotalCpuTicks = currentTotalCpuTicks;
        previousTime = currentTime;
        initialized = true;
        return rows;
    }

    long long totalDelta = currentTotalCpuTicks - previousTotalCpuTicks;
    if (totalDelta <= 0)
        totalDelta = 1;

    std::chrono::duration<double> elapsed = currentTime - previousTime;
    double seconds = elapsed.count();
    if (seconds <= 0.0)
        seconds = 1.0;

    const long long hz = ::sysconf(_SC_CLK_TCK);
    if (hz <= 0)
        return rows;

    for (const auto &kv : previousSnapshot)
    {
        int pid = kv.first;
        const ProcSnap &oldSnap = kv.second;

        const auto infoNowOpt = readProcInfoNow(pid);
        if (!infoNowOpt)
            continue;

        const ProcInfoNow &infoNow = *infoNowOpt;

        long long procDelta = infoNow.cpuTicks - oldSnap.cpuTicks;
        if (procDelta < 0)
            procDelta = 0;

        unsigned long long readDiff = 0;
        unsigned long long writeDiff = 0;

        if (infoNow.readBytes >= oldSnap.readBytes)
            readDiff = infoNow.readBytes - oldSnap.readBytes;

        if (infoNow.writeBytes >= oldSnap.writeBytes)
            writeDiff = infoNow.writeBytes - oldSnap.writeBytes;

        ProcessRow row;
        row.pid = pid;
        row.startTimeTicks = infoNow.startTimeTicks;
        row.name = infoNow.name;
        row.state = infoNow.state;

        row.cpuPercent = (static_cast<double>(procDelta) / static_cast<double>(totalDelta)) * 100.0;
        row.cpuTimeSec = static_cast<double>(infoNow.cpuTicks) / static_cast<double>(hz);
        row.threads = infoNow.threads;

        row.rssMB = static_cast<double>(std::max<long long>(0, infoNow.rssKB)) / 1024.0;
        row.vszMB = static_cast<double>(std::max<long long>(0, infoNow.vszKB)) / 1024.0;
        row.sharedMB = static_cast<double>(std::max<long long>(0, infoNow.sharedKB)) / 1024.0;

        row.readBytesPerSec = static_cast<long long>(static_cast<double>(readDiff) / seconds);
        row.writeBytesPerSec = static_cast<long long>(static_cast<double>(writeDiff) / seconds);
        row.totalReadBytes = infoNow.readBytes;
        row.totalWriteBytes = infoNow.writeBytes;

        row.priority = infoNow.priority;
        row.niceValue = infoNow.niceValue;

        rows.push_back(row);
    }

    std::sort(rows.begin(), rows.end(),
              [](const ProcessRow &a, const ProcessRow &b)
              {
                  if (a.cpuPercent != b.cpuPercent)
                      return a.cpuPercent > b.cpuPercent;

                  if (a.rssMB != b.rssMB)
                      return a.rssMB > b.rssMB;

                  return a.pid < b.pid;
              });

    if (rows.size() > maxRows)
        rows.resize(maxRows);

    previousSnapshot = currentSnapshot;
    previousTotalCpuTicks = currentTotalCpuTicks;
    previousTime = currentTime;

    return rows;
}
