#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QThread>
#include <QObject>
#include <QPoint>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <vector>

#include "processtable.h"
#include "systemstats.h"
#include "linegraphwidget.h"
#include "bargraphwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum class MonitorPage
{
    Dashboard,
    CPU,
    Memory,
    Disk,
    Network
};

enum class ProcessColumn
{
    PID,
    Name,
    State,
    CpuPercent,
    CpuTimeSec,
    Threads,
    RssMB,
    VszMB,
    SharedMB,
    ReadPerSec,
    WritePerSec,
    TotalReadBytes,
    TotalWriteBytes,
    Priority,
    NiceValue
};

enum class SortState
{
    Normal,
    Descending,
    Ascending
};

class StatsWorker : public QObject
{
    Q_OBJECT

public slots:
    void run()
    {
        SystemData data = SystemStats::readSystemData();
        std::vector<ProcessRow> rows = processTable.readProcesses();
        emit result(data, rows);
    }

signals:
    void result(SystemData data, std::vector<ProcessRow> rows);

private:
    ProcessTable processTable;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setRefreshInterval(int milliseconds);

signals:
    void requestStats();

private slots:

    void showProcessContextMenu(const QPoint &pos);
    void onStatsResult(SystemData data, std::vector<ProcessRow> rows);
    void onProcessHeaderClicked(int logicalIndex);
    void showProcessHeaderMenu(const QPoint &pos);

    void showDashboardPage();
    void showCpuPage();
    void showMemoryPage();
    void showDiskPage();
    void showNetworkPage();

    void toggleSidebar();
    void toggleCpuUsageSection();
    void toggleCpuPerCoreSection();

    void toggleMemoryUsageSection();
    void toggleMemoryUsedAvailableSection();
    void toggleMemoryCacheBuffersSection();
    void toggleMemorySwapSection();

    void toggleDiskUsageSection();
    void toggleDiskActivitySection();

    void toggleNetworkTrafficSection();
    void toggleNetworkInterfacesSection();

private:
    void sendSignalToSelectedProcess(int signal);
    void onSearchTextChanged(const QString &text);
    void setupProcessTable();
    void rebuildProcessTableColumns();
    void populateProcessTable(const std::vector<ProcessRow> &rows);

    QString columnTitle(ProcessColumn column) const;
    QString columnText(const ProcessRow &row, ProcessColumn column) const;

    std::vector<ProcessColumn> defaultColumnsForPage(MonitorPage page) const;
    bool compareRows(const ProcessRow &a,
                     const ProcessRow &b,
                     ProcessColumn column,
                     bool descending) const;

    std::vector<ProcessRow> applySorting(const std::vector<ProcessRow> &rows) const;

    std::vector<QProgressBar *> interfaceTrafficBars;
    std::vector<QLabel *> interfaceTrafficValueLabels;
    void setupInterfaceTrafficBars(const std::vector<InterfaceRate> &interfaces);

    void setCurrentPage(MonitorPage page);

    void applyDefaultSplitterSizes();

    void setupGraphs();
    void updateGraphs(const SystemData &data);
    void updatePageHeader();
    void updateCurrentPageHeight();

    void updateSidebarAppearance();
    void updateSidebarHighlight();
    void setNavButtonActive(QPushButton *button, bool active);

    void setupPerCoreGraphs(int coreCount);
    void updateCpuStats(const SystemData &data);
    void updateMemoryStats(const SystemData &data);
    void updateDiskStats(const SystemData &data);
    void updateNetworkStats(const SystemData &data);

    double averageOf(const std::vector<double> &values) const;
    double peakOf(const std::vector<double> &values) const;

    Ui::MainWindow *ui;
    QTimer *timer;
    QThread *workerThread;
    StatsWorker *statsWorker;

    MonitorPage currentPage;
    std::vector<ProcessColumn> visibleColumns;
    std::vector<ProcessRow> baseRows;

    ProcessColumn currentSortColumn;
    SortState currentSortState;

    LineGraphWidget *dashboardCpuGraph;
    LineGraphWidget *dashboardMemoryGraph;
    LineGraphWidget *dashboardDiskGraph;
    LineGraphWidget *dashboardNetworkGraph;

    LineGraphWidget *cpuGraph;
    LineGraphWidget *memoryGraph;
    LineGraphWidget *diskGraph;
    LineGraphWidget *networkGraph;

    LineGraphWidget *memoryUsedGraph;
    LineGraphWidget *memoryCacheGraph;
    QProgressBar *memorySwapBar;
    QLabel *memorySwapPercentLabel;

    BarGraphWidget *diskBarGraph;
    BarGraphWidget *networkInterfacesBarGraph;

    std::vector<QProgressBar *> perCoreBars;
    std::vector<QLabel *> perCoreValueLabels;

    std::vector<double> cpuHistory;
    std::vector<double> cpuUserHistory;
    std::vector<double> cpuSystemHistory;

    bool sidebarExpanded;
    bool cpuUsageExpanded;
    bool cpuPerCoreExpanded;

    bool memoryUsageExpanded;
    bool memoryUsedAvailableExpanded;
    bool memoryCacheBuffersExpanded;
    bool memorySwapExpanded;

    bool diskUsageExpanded;
    bool diskActivityExpanded;

    bool networkTrafficExpanded;
    bool networkInterfacesExpanded;
};

#endif
