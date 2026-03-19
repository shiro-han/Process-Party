#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QHeaderView>
#include <QTableWidgetItem>
#include <QString>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QProgressBar>
#include <QScrollBar>
#include <QTimer>

#include <algorithm>

static const std::vector<ProcessColumn> columnOrder = {
    ProcessColumn::Name,
    ProcessColumn::PID,
    ProcessColumn::State,
    ProcessColumn::CpuPercent,
    ProcessColumn::CpuTimeSec,
    ProcessColumn::Threads,
    ProcessColumn::RssMB,
    ProcessColumn::VszMB,
    ProcessColumn::SharedMB,
    ProcessColumn::ReadPerSec,
    ProcessColumn::WritePerSec,
    ProcessColumn::TotalReadBytes,
    ProcessColumn::TotalWriteBytes,
    ProcessColumn::Priority,
    ProcessColumn::NiceValue
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    timer(new QTimer(this)),
    workerThread(new QThread(this)),
    statsWorker(new StatsWorker),
    currentPage(MonitorPage::Dashboard),
    currentSortColumn(ProcessColumn::CpuPercent),
    currentSortState(SortState::Normal),
    dashboardCpuGraph(nullptr),
    dashboardMemoryGraph(nullptr),
    dashboardDiskGraph(nullptr),
    dashboardNetworkGraph(nullptr),
    cpuGraph(nullptr),
    memoryGraph(nullptr),
    diskGraph(nullptr),
    networkGraph(nullptr),
    memoryUsedGraph(nullptr),
    memoryCacheGraph(nullptr),
    memorySwapBar(nullptr),
    memorySwapPercentLabel(nullptr),
    diskBarGraph(nullptr),
    networkInterfacesBarGraph(nullptr),
    sidebarExpanded(true),
    cpuUsageExpanded(true),
    cpuPerCoreExpanded(true),
    memoryUsageExpanded(true),
    memoryUsedAvailableExpanded(true),
    memoryCacheBuffersExpanded(true),
    memorySwapExpanded(true),
    diskUsageExpanded(true),
    diskActivityExpanded(true),
    networkTrafficExpanded(true),
    networkInterfacesExpanded(true)
{
    ui->setupUi(this);

    ui->scrollContentLayout->setAlignment(Qt::AlignTop);
    ui->dashboardPageLayout->setAlignment(Qt::AlignTop);
    ui->cpuPageLayout->setAlignment(Qt::AlignTop);
    ui->memoryPageLayout->setAlignment(Qt::AlignTop);
    ui->diskPageLayout->setAlignment(Qt::AlignTop);
    ui->networkPageLayout->setAlignment(Qt::AlignTop);

    ui->scrollContentLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    ui->dashboardPageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    ui->cpuPageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    ui->memoryPageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    ui->diskPageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    ui->networkPageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    ui->mainScrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->stackedWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->dashboardPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->cpuPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->memoryPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->diskPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->networkPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    visibleColumns = defaultColumnsForPage(currentPage);
    ui->mainVerticalSplitter->setHandleWidth(10);

    setupGraphs();
    setupProcessTable();
    updatePageHeader();
    updateSidebarAppearance();
    ui->sidebarFrame->updateGeometry();
    ui->centralwidget->updateGeometry();

    updateSidebarHighlight();
    applyDefaultSplitterSizes();

    connect(ui->sidebarToggleButton, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    connect(ui->dashboardButton, &QPushButton::clicked, this, &MainWindow::showDashboardPage);
    connect(ui->cpuButton, &QPushButton::clicked, this, &MainWindow::showCpuPage);
    connect(ui->memoryButton, &QPushButton::clicked, this, &MainWindow::showMemoryPage);
    connect(ui->diskButton, &QPushButton::clicked, this, &MainWindow::showDiskPage);
    connect(ui->networkButton, &QPushButton::clicked, this, &MainWindow::showNetworkPage);

    connect(ui->cpuUsageToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleCpuUsageSection);
    connect(ui->cpuPerCoreToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleCpuPerCoreSection);

    connect(ui->memoryUsageToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleMemoryUsageSection);
    connect(ui->memoryUsedAvailableToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleMemoryUsedAvailableSection);
    connect(ui->memoryCacheBuffersToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleMemoryCacheBuffersSection);
    connect(ui->memorySwapToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleMemorySwapSection);

    connect(ui->diskUsageToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleDiskUsageSection);
    connect(ui->diskActivityToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleDiskActivitySection);

    connect(ui->networkTrafficToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleNetworkTrafficSection);
    connect(ui->networkInterfacesToggleButton, &QPushButton::clicked,
            this, &MainWindow::toggleNetworkInterfacesSection);

    statsWorker->moveToThread(workerThread);

    connect(timer, &QTimer::timeout, this, &MainWindow::requestStats);
    connect(this, &MainWindow::requestStats, statsWorker, &StatsWorker::run);
    connect(statsWorker, &StatsWorker::result, this, &MainWindow::onStatsResult);
    connect(workerThread, &QThread::finished, statsWorker, &QObject::deleteLater);

    workerThread->start();
    timer->start(1000);

    emit requestStats();
}

MainWindow::~MainWindow()
{
    timer->stop();
    workerThread->quit();
    workerThread->wait();
    delete ui;
}

void MainWindow::setRefreshInterval(int milliseconds)
{
    timer->setInterval(milliseconds);
}

void MainWindow::setupGraphs()
{
    dashboardCpuGraph = new LineGraphWidget(this);
    dashboardCpuGraph->setTitle("CPU");
    dashboardCpuGraph->setUnitSuffix("%");
    dashboardCpuGraph->setFixedRange(0.0, 100.0);
    dashboardCpuGraph->setMaxSamples(40);

    dashboardMemoryGraph = new LineGraphWidget(this);
    dashboardMemoryGraph->setTitle("Memory");
    dashboardMemoryGraph->setUnitSuffix("%");
    dashboardMemoryGraph->setFixedRange(0.0, 100.0);
    dashboardMemoryGraph->setMaxSamples(40);

    dashboardDiskGraph = new LineGraphWidget(this);
    dashboardDiskGraph->setTitle("Disk");
    dashboardDiskGraph->setUnitSuffix("%");
    dashboardDiskGraph->setFixedRange(0.0, 100.0);
    dashboardDiskGraph->setMaxSamples(40);

    dashboardNetworkGraph = new LineGraphWidget(this);
    dashboardNetworkGraph->setTitle("Network");
    dashboardNetworkGraph->setUnitSuffix(" B/s");
    dashboardNetworkGraph->setAutoScale(true);
    dashboardNetworkGraph->setMaxSamples(40);

    cpuGraph = new LineGraphWidget(this);
    cpuGraph->setTitle("CPU Usage");
    cpuGraph->setUnitSuffix("%");
    cpuGraph->setFixedRange(0.0, 100.0);
    cpuGraph->setMaxSamples(120);
    cpuGraph->setSeriesNames({"Total", "User", "System"});
    cpuGraph->setShowTitle(false);
    cpuGraph->setShowSummaryText(false);

    memoryGraph = new LineGraphWidget(this);
    memoryGraph->setTitle("Memory Usage");
    memoryGraph->setUnitSuffix("%");
    memoryGraph->setFixedRange(0.0, 100.0);
    memoryGraph->setMaxSamples(120);
    memoryGraph->setShowTitle(false);
    memoryGraph->setShowSummaryText(false);
    memoryGraph->setMinimumHeight(140);
    memoryGraph->setMaximumHeight(170);

    memoryUsedGraph = new LineGraphWidget(this);
    memoryUsedGraph->setTitle("Used vs Available Memory");
    memoryUsedGraph->setUnitSuffix(" MB");
    memoryUsedGraph->setAutoScale(true);
    memoryUsedGraph->setMaxSamples(120);
    memoryUsedGraph->setSeriesNames({"Used", "Available"});
    memoryUsedGraph->setShowTitle(false);
    memoryUsedGraph->setShowSummaryText(false);
    memoryUsedGraph->setMinimumHeight(120);
    memoryUsedGraph->setMaximumHeight(140);

    memoryCacheGraph = new LineGraphWidget(this);
    memoryCacheGraph->setTitle("Cache / Buffers");
    memoryCacheGraph->setUnitSuffix(" MB");
    memoryCacheGraph->setAutoScale(true);
    memoryCacheGraph->setMaxSamples(120);
    memoryCacheGraph->setSeriesNames({"Cache", "Buffers"});
    memoryCacheGraph->setShowTitle(false);
    memoryCacheGraph->setShowSummaryText(false);
    memoryCacheGraph->setMinimumHeight(120);
    memoryCacheGraph->setMaximumHeight(140);

    memorySwapBar = new QProgressBar(this);
    memorySwapBar->setRange(0, 1000);
    memorySwapBar->setValue(0);
    memorySwapBar->setTextVisible(false);
    memorySwapBar->setMinimumHeight(18);
    memorySwapBar->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid #cfcfcf;"
        "  border-radius: 4px;"
        "  background: #f3f3f3;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #f97316;"
        "  border-radius: 3px;"
        "}"
        );

    memorySwapPercentLabel = new QLabel("0.0%", this);
    memorySwapPercentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QWidget *swapWidget = new QWidget(this);
    QHBoxLayout *swapLayout = new QHBoxLayout(swapWidget);
    swapLayout->setContentsMargins(0, 0, 0, 0);
    swapLayout->setSpacing(8);
    swapLayout->addWidget(memorySwapBar, 1);
    swapLayout->addWidget(memorySwapPercentLabel);

    diskBarGraph = new BarGraphWidget(this);
    diskBarGraph->setTitle("");
    diskBarGraph->setUnitSuffix("%");
    diskBarGraph->setRange(0.0, 100.0);
    diskBarGraph->setMinimumHeight(50);
    diskBarGraph->setMaximumHeight(70);

    diskGraph = new LineGraphWidget(this);
    diskGraph->setTitle("Disk Activity");
    diskGraph->setUnitSuffix(" B/s");
    diskGraph->setAutoScale(true);
    diskGraph->setMaxSamples(120);
    diskGraph->setSeriesNames({"Read", "Write"});
    diskGraph->setShowTitle(false);
    diskGraph->setShowSummaryText(false);
    diskGraph->setMinimumHeight(140);
    diskGraph->setMaximumHeight(170);

    networkGraph = new LineGraphWidget(this);
    networkGraph->setTitle("Network Traffic");
    networkGraph->setUnitSuffix(" B/s");
    networkGraph->setAutoScale(true);
    networkGraph->setMaxSamples(120);
    networkGraph->setSeriesNames({"Download", "Upload"});
    networkGraph->setShowTitle(false);
    networkGraph->setShowSummaryText(false);
    networkGraph->setMinimumHeight(140);
    networkGraph->setMaximumHeight(170);

    auto attachGraph = [](QWidget *container, QWidget *graph)
    {
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(graph);
    };

    attachGraph(ui->dashboardCpuGraphContainer, dashboardCpuGraph);
    attachGraph(ui->dashboardMemoryGraphContainer, dashboardMemoryGraph);
    attachGraph(ui->dashboardDiskGraphContainer, dashboardDiskGraph);
    attachGraph(ui->dashboardNetworkGraphContainer, dashboardNetworkGraph);

    ui->dashboardCpuGraphContainer->setMinimumHeight(150);
    ui->dashboardCpuGraphContainer->setMaximumHeight(170);
    ui->dashboardMemoryGraphContainer->setMinimumHeight(150);
    ui->dashboardMemoryGraphContainer->setMaximumHeight(170);
    ui->dashboardDiskGraphContainer->setMinimumHeight(150);
    ui->dashboardDiskGraphContainer->setMaximumHeight(170);
    ui->dashboardNetworkGraphContainer->setMinimumHeight(150);
    ui->dashboardNetworkGraphContainer->setMaximumHeight(170);

    ui->dashboardCpuGraphContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->dashboardMemoryGraphContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->dashboardDiskGraphContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    ui->dashboardNetworkGraphContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    attachGraph(ui->cpuGraphContainer, cpuGraph);
    attachGraph(ui->memoryGraphContainer, memoryGraph);
    attachGraph(ui->memoryUsedGraphContainer, memoryUsedGraph);
    attachGraph(ui->memoryCacheGraphContainer, memoryCacheGraph);
    attachGraph(ui->memorySwapGraphContainer, swapWidget);
    attachGraph(ui->diskBarGraphContainer, diskBarGraph);
    attachGraph(ui->diskGraphContainer, diskGraph);
    attachGraph(ui->networkGraphContainer, networkGraph);

    updateCurrentPageHeight();
}

void MainWindow::toggleCpuUsageSection()
{
    cpuUsageExpanded = !cpuUsageExpanded;
    ui->cpuUsageSection->setVisible(cpuUsageExpanded);
    ui->cpuUsageToggleButton->setText(cpuUsageExpanded ? "CPU Usage ▼" : "CPU Usage ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleCpuPerCoreSection()
{
    cpuPerCoreExpanded = !cpuPerCoreExpanded;
    ui->cpuPerCoreSection->setVisible(cpuPerCoreExpanded);

    int coreCount = static_cast<int>(perCoreBars.size());
    ui->cpuPerCoreToggleButton->setText(
        cpuPerCoreExpanded
            ? QString("Per-Core CPU (%1 cores) ▼").arg(coreCount)
            : QString("Per-Core CPU (%1 cores) ►").arg(coreCount)
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleMemoryUsageSection()
{
    memoryUsageExpanded = !memoryUsageExpanded;
    ui->memoryUsageSection->setVisible(memoryUsageExpanded);
    ui->memoryUsageToggleButton->setText(
        memoryUsageExpanded ? "Memory Usage ▼" : "Memory Usage ►"
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleMemoryUsedAvailableSection()
{
    memoryUsedAvailableExpanded = !memoryUsedAvailableExpanded;
    ui->memoryUsedAvailableSection->setVisible(memoryUsedAvailableExpanded);
    ui->memoryUsedAvailableToggleButton->setText(
        memoryUsedAvailableExpanded ? "Used vs Available ▼" : "Used vs Available ►"
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleMemoryCacheBuffersSection()
{
    memoryCacheBuffersExpanded = !memoryCacheBuffersExpanded;
    ui->memoryCacheBuffersSection->setVisible(memoryCacheBuffersExpanded);
    ui->memoryCacheBuffersToggleButton->setText(
        memoryCacheBuffersExpanded ? "Cache / Buffers ▼" : "Cache / Buffers ►"
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleMemorySwapSection()
{
    memorySwapExpanded = !memorySwapExpanded;
    ui->memorySwapSection->setVisible(memorySwapExpanded);
    ui->memorySwapToggleButton->setText(memorySwapExpanded ? "Swap ▼" : "Swap ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleDiskUsageSection()
{
    diskUsageExpanded = !diskUsageExpanded;
    ui->diskUsageSection->setVisible(diskUsageExpanded);
    ui->diskUsageToggleButton->setText(diskUsageExpanded ? "Disk Usage ▼" : "Disk Usage ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleDiskActivitySection()
{
    diskActivityExpanded = !diskActivityExpanded;
    ui->diskActivitySection->setVisible(diskActivityExpanded);
    ui->diskActivityToggleButton->setText(diskActivityExpanded ? "Disk Activity ▼" : "Disk Activity ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleNetworkTrafficSection()
{
    networkTrafficExpanded = !networkTrafficExpanded;
    ui->networkTrafficSection->setVisible(networkTrafficExpanded);
    ui->networkTrafficToggleButton->setText(
        networkTrafficExpanded ? "Network Traffic ▼" : "Network Traffic ►"
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleNetworkInterfacesSection()
{
    networkInterfacesExpanded = !networkInterfacesExpanded;
    ui->networkInterfacesSection->setVisible(networkInterfacesExpanded);
    ui->networkInterfacesToggleButton->setText(
        networkInterfacesExpanded ? "Interface Traffic ▼" : "Interface Traffic ►"
        );

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::setupInterfaceTrafficBars(const std::vector<InterfaceRate> &interfaces)
{
    QGridLayout *grid = ui->networkInterfacesGridLayout;

    while (QLayoutItem *item = grid->takeAt(0))
    {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    interfaceTrafficBars.clear();
    interfaceTrafficValueLabels.clear();

    QVBoxLayout *listLayout = new QVBoxLayout();
    listLayout->setContentsMargins(6, 6, 6, 6);
    listLayout->setSpacing(8);

    double maxCombined = 1.0;
    for (const InterfaceRate &iface : interfaces)
    {
        double combined = iface.downloadBytesPerSec + iface.uploadBytesPerSec;
        maxCombined = std::max(maxCombined, combined);
    }

    for (const InterfaceRate &iface : interfaces)
    {
        QWidget *rowWidget = new QWidget(ui->networkInterfacesScrollWidget);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        QLabel *nameLabel = new QLabel(QString::fromStdString(iface.name), rowWidget);
        nameLabel->setMinimumWidth(90);

        QProgressBar *bar = new QProgressBar(rowWidget);
        bar->setRange(0, 1000);
        bar->setTextVisible(false);
        bar->setMinimumHeight(18);
        bar->setStyleSheet(
            "QProgressBar {"
            "  border: 1px solid #cfcfcf;"
            "  border-radius: 4px;"
            "  background: #f3f3f3;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #f97316;"
            "  border-radius: 3px;"
            "}"
            );

        double combined = iface.downloadBytesPerSec + iface.uploadBytesPerSec;
        int scaledValue = static_cast<int>((combined / maxCombined) * 1000.0);
        bar->setValue(std::clamp(scaledValue, 0, 1000));

        QLabel *valueLabel = new QLabel(
            QString::fromStdString(SystemStats::formatNetworkRate(combined)),
            rowWidget
            );
        valueLabel->setMinimumWidth(80);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(bar, 1);
        rowLayout->addWidget(valueLabel);

        listLayout->addWidget(rowWidget);

        interfaceTrafficBars.push_back(bar);
        interfaceTrafficValueLabels.push_back(valueLabel);
    }

    if (interfaces.empty())
    {
        QLabel *emptyLabel = new QLabel("No interfaces", ui->networkInterfacesScrollWidget);
        listLayout->addWidget(emptyLabel);
    }

    QWidget *container = new QWidget(ui->networkInterfacesScrollWidget);
    container->setLayout(listLayout);
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    container->adjustSize();

    grid->addWidget(container, 0, 0);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setVerticalSpacing(0);
    grid->setHorizontalSpacing(0);
    grid->setRowStretch(0, 0);
    grid->setColumnStretch(0, 1);

    const int rowHeight = 28;
    const int spacing = 8;
    const int margins = 12;
    int rowCount = std::max(1, static_cast<int>(interfaces.size()));
    int contentHeight = margins + rowCount * rowHeight + std::max(0, rowCount - 1) * spacing;

    ui->networkInterfacesScrollArea->setFixedHeight(contentHeight);

    updateCurrentPageHeight();
}

void MainWindow::setupPerCoreGraphs(int coreCount)
{
    if (coreCount < 0)
        coreCount = 0;

    ui->cpuPerCoreToggleButton->setText(
        QString("Per-Core CPU (%1 cores) %2")
            .arg(coreCount)
            .arg(cpuPerCoreExpanded ? "▼" : "►")
        );

    if (static_cast<int>(perCoreBars.size()) == coreCount)
        return;

    QGridLayout *grid = ui->cpuPerCoreGridLayout;

    while (QLayoutItem *item = grid->takeAt(0))
    {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    perCoreBars.clear();
    perCoreValueLabels.clear();

    QVBoxLayout *listLayout = new QVBoxLayout();
    listLayout->setContentsMargins(6, 6, 6, 6);
    listLayout->setSpacing(8);

    for (int i = 0; i < coreCount; ++i)
    {
        QWidget *rowWidget = new QWidget(ui->cpuPerCoreScrollWidget);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        QLabel *nameLabel = new QLabel(QString("Core %1").arg(i + 1), rowWidget);
        nameLabel->setMinimumWidth(70);

        QProgressBar *bar = new QProgressBar(rowWidget);
        bar->setRange(0, 1000);
        bar->setValue(0);
        bar->setTextVisible(false);
        bar->setMinimumHeight(18);
        bar->setStyleSheet(
            "QProgressBar {"
            "  border: 1px solid #cfcfcf;"
            "  border-radius: 4px;"
            "  background: #f3f3f3;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #4f8ef7;"
            "  border-radius: 3px;"
            "}"
            );

        QLabel *valueLabel = new QLabel("0.0%", rowWidget);
        valueLabel->setMinimumWidth(50);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(bar, 1);
        rowLayout->addWidget(valueLabel);

        listLayout->addWidget(rowWidget);

        perCoreBars.push_back(bar);
        perCoreValueLabels.push_back(valueLabel);
    }

    QWidget *container = new QWidget(ui->cpuPerCoreScrollWidget);
    container->setLayout(listLayout);
    container->adjustSize();
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    grid->addWidget(container, 0, 0);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setVerticalSpacing(0);
    grid->setHorizontalSpacing(0);
    grid->setRowStretch(0, 0);
    grid->setColumnStretch(0, 1);

    const int rowHeight = 28;
    const int spacing = 8;
    const int margins = 12;
    int contentHeight = margins + coreCount * rowHeight + std::max(0, coreCount - 1) * spacing;

    ui->cpuPerCoreScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->cpuPerCoreScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->cpuPerCoreScrollArea->setFixedHeight(contentHeight);

    updateCurrentPageHeight();
}

double MainWindow::averageOf(const std::vector<double> &values) const
{
    if (values.empty())
        return 0.0;

    double sum = 0.0;
    for (double v : values)
        sum += v;

    return sum / static_cast<double>(values.size());
}

double MainWindow::peakOf(const std::vector<double> &values) const
{
    if (values.empty())
        return 0.0;

    return *std::max_element(values.begin(), values.end());
}

void MainWindow::updateCpuStats(const SystemData &data)
{
    cpuHistory.push_back(data.cpu.totalPercent);
    cpuUserHistory.push_back(data.cpu.userPercent);
    cpuSystemHistory.push_back(data.cpu.systemPercent);

    const std::size_t maxHistory = 120;

    if (cpuHistory.size() > maxHistory)
        cpuHistory.erase(cpuHistory.begin());
    if (cpuUserHistory.size() > maxHistory)
        cpuUserHistory.erase(cpuUserHistory.begin());
    if (cpuSystemHistory.size() > maxHistory)
        cpuSystemHistory.erase(cpuSystemHistory.begin());

    ui->cpuCurrentValueLabel->setText(QString("%1%").arg(data.cpu.totalPercent, 0, 'f', 1));
    ui->cpuAverageValueLabel->setText(QString("%1%").arg(averageOf(cpuHistory), 0, 'f', 1));
    ui->cpuPeakValueLabel->setText(QString("%1%").arg(peakOf(cpuHistory), 0, 'f', 1));

    ui->cpuUserCurrentValueLabel->setText(QString("%1%").arg(data.cpu.userPercent, 0, 'f', 1));
    ui->cpuUserAverageValueLabel->setText(QString("%1%").arg(averageOf(cpuUserHistory), 0, 'f', 1));
    ui->cpuUserPeakValueLabel->setText(QString("%1%").arg(peakOf(cpuUserHistory), 0, 'f', 1));

    ui->cpuSystemCurrentValueLabel->setText(QString("%1%").arg(data.cpu.systemPercent, 0, 'f', 1));
    ui->cpuSystemAverageValueLabel->setText(QString("%1%").arg(averageOf(cpuSystemHistory), 0, 'f', 1));
    ui->cpuSystemPeakValueLabel->setText(QString("%1%").arg(peakOf(cpuSystemHistory), 0, 'f', 1));
}

void MainWindow::updateMemoryStats(const SystemData &data)
{
    ui->memoryTotalValueLabel->setText(QString("%1 MB").arg(data.memory.totalMB, 0, 'f', 1));
    ui->memoryUsedValueLabel->setText(QString("%1 MB").arg(data.memory.usedMB, 0, 'f', 1));
    ui->memoryAvailableValueLabel->setText(QString("%1 MB").arg(data.memory.availableMB, 0, 'f', 1));

    ui->memorySwapTotalValueLabel->setText(QString("%1 MB").arg(data.memory.swapTotalMB, 0, 'f', 1));
    ui->memorySwapUsedValueLabel->setText(QString("%1 MB").arg(data.memory.swapUsedMB, 0, 'f', 1));
    ui->memorySwapFreeValueLabel->setText(QString("%1 MB").arg(data.memory.swapFreeMB, 0, 'f', 1));

    if (memorySwapBar)
        memorySwapBar->setValue(static_cast<int>(data.memory.swapPercent * 10.0));

    if (memorySwapPercentLabel)
        memorySwapPercentLabel->setText(QString("%1%").arg(data.memory.swapPercent, 0, 'f', 1));

    ui->memoryUsedStatValueLabel->setText(
        QString("%1 MB").arg(data.memory.usedMB, 0, 'f', 1)
        );

    ui->memoryAvailableStatValueLabel->setText(
        QString("%1 MB").arg(data.memory.availableMB, 0, 'f', 1)
        );

    ui->memoryCacheStatValueLabel->setText(
        QString("%1 MB").arg(data.memory.cachedMB, 0, 'f', 1)
        );

    ui->memoryBuffersStatValueLabel->setText(
        QString("%1 MB").arg(data.memory.bufferedMB, 0, 'f', 1)
        );
}

void MainWindow::updateDiskStats(const SystemData &data)
{
    if (diskBarGraph)
        diskBarGraph->setValue(static_cast<double>(data.diskPercent));

    ui->diskReadStatValueLabel->setText(
        QString::fromStdString(SystemStats::formatNetworkRate(data.disk.readBytesPerSec))
        );

    ui->diskWriteStatValueLabel->setText(
        QString::fromStdString(SystemStats::formatNetworkRate(data.disk.writeBytesPerSec))
        );

    ui->diskUsedValueLabel->setText(
        QString("%1 GB").arg(data.disk.usedGB, 0, 'f', 1)
        );

    ui->diskFreeValueLabel->setText(
        QString("%1 GB").arg(data.disk.freeGB, 0, 'f', 1)
        );

    ui->diskTotalValueLabel->setText(
        QString("%1 GB").arg(data.disk.totalGB, 0, 'f', 1)
        );
}

void MainWindow::updateNetworkStats(const SystemData &data)
{
    ui->networkDownloadStatValueLabel->setText(
        QString::fromStdString(data.network.downloadText)
        );

    ui->networkUploadStatValueLabel->setText(
        QString::fromStdString(data.network.uploadText)
        );

    ui->networkTotalDownloadedStatValueLabel->setText(
        QString::fromStdString(SystemStats::formatBytes(data.network.totalDownloadedBytes))
        );

    ui->networkTotalUploadedStatValueLabel->setText(
        QString::fromStdString(SystemStats::formatBytes(data.network.totalUploadedBytes))
        );

    setupInterfaceTrafficBars(data.network.interfaces);
}

void MainWindow::updateGraphs(const SystemData &data)
{
    dashboardCpuGraph->addSample(data.cpu.totalPercent);
    dashboardMemoryGraph->addSample(static_cast<double>(data.memoryPercent));
    dashboardDiskGraph->addSample(static_cast<double>(data.diskPercent));
    dashboardNetworkGraph->addSample(data.network.downloadBytesPerSec);

    cpuGraph->addSamples({
        data.cpu.totalPercent,
        data.cpu.userPercent,
        data.cpu.systemPercent
    });

    memoryGraph->addSample(static_cast<double>(data.memoryPercent));

    memoryUsedGraph->addSamples({
        data.memory.usedMB,
        data.memory.availableMB
    });

    memoryCacheGraph->addSamples({
        data.memory.cachedMB,
        data.memory.bufferedMB
    });

    diskGraph->addSamples({
        data.disk.readBytesPerSec,
        data.disk.writeBytesPerSec
    });

    networkGraph->addSamples({
        data.network.downloadBytesPerSec,
        data.network.uploadBytesPerSec
    });

    setupPerCoreGraphs(static_cast<int>(data.cpu.perCorePercents.size()));

    int count = std::min(static_cast<int>(perCoreBars.size()),
                         static_cast<int>(data.cpu.perCorePercents.size()));

    for (int i = 0; i < count; ++i)
    {
        double percent = data.cpu.perCorePercents[i];

        if (perCoreBars[i])
            perCoreBars[i]->setValue(static_cast<int>(percent * 10.0));

        if (perCoreValueLabels[i])
            perCoreValueLabels[i]->setText(QString("%1%").arg(percent, 0, 'f', 1));
    }
}

void MainWindow::updatePageHeader()
{
    switch (currentPage)
    {
    case MonitorPage::Dashboard:
        ui->pageTitleLabel->setText("Dashboard");
        ui->stackedWidget->setCurrentWidget(ui->dashboardPage);
        break;
    case MonitorPage::CPU:
        ui->pageTitleLabel->setText("CPU");
        ui->stackedWidget->setCurrentWidget(ui->cpuPage);
        break;
    case MonitorPage::Memory:
        ui->pageTitleLabel->setText("Memory");
        ui->stackedWidget->setCurrentWidget(ui->memoryPage);
        break;
    case MonitorPage::Disk:
        ui->pageTitleLabel->setText("Disk");
        ui->stackedWidget->setCurrentWidget(ui->diskPage);
        break;
    case MonitorPage::Network:
        ui->pageTitleLabel->setText("Network");
        ui->stackedWidget->setCurrentWidget(ui->networkPage);
        break;
    }
}

void MainWindow::applyDefaultSplitterSizes()
{
    QList<int> sizes;

    switch (currentPage)
    {
    case MonitorPage::Dashboard:
        sizes << 330 << 320;
        break;

    case MonitorPage::CPU:
        sizes << 460 << 220;
        break;

    case MonitorPage::Memory:
        sizes << 430 << 240;
        break;

    case MonitorPage::Disk:
        sizes << 340 << 300;
        break;

    case MonitorPage::Network:
        sizes << 330 << 330;
        break;
    }

    ui->mainVerticalSplitter->setSizes(sizes);
}

void MainWindow::updateCurrentPageHeight()
{
    QWidget *page = ui->stackedWidget->currentWidget();
    if (!page)
        return;

    page->adjustSize();
    page->updateGeometry();

    const int pageHeight = page->sizeHint().height();

    ui->stackedWidget->setMinimumHeight(pageHeight);
    ui->stackedWidget->setMaximumHeight(pageHeight);

    ui->mainScrollAreaWidgetContents->adjustSize();
    ui->mainScrollAreaWidgetContents->updateGeometry();
    ui->mainScrollArea->widget()->adjustSize();
    ui->mainScrollArea->widget()->updateGeometry();
}

void MainWindow::setNavButtonActive(QPushButton *button, bool active)
{
    if (active)
    {
        button->setStyleSheet(
            "QPushButton {"
            "  font-weight: bold;"
            "  background-color: #dbeafe;"
            "  border: 2px solid #60a5fa;"
            "  border-radius: 6px;"
            "  padding: 6px 8px;"
            "  text-align: left;"
            "}"
            );
    }
    else
    {
        button->setStyleSheet(
            "QPushButton {"
            "  font-weight: normal;"
            "  background-color: none;"
            "  border: 1px solid #cfcfcf;"
            "  border-radius: 6px;"
            "  padding: 6px 8px;"
            "  text-align: left;"
            "}"
            );
    }
}

void MainWindow::updateSidebarHighlight()
{
    setNavButtonActive(ui->dashboardButton, currentPage == MonitorPage::Dashboard);
    setNavButtonActive(ui->cpuButton, currentPage == MonitorPage::CPU);
    setNavButtonActive(ui->memoryButton, currentPage == MonitorPage::Memory);
    setNavButtonActive(ui->diskButton, currentPage == MonitorPage::Disk);
    setNavButtonActive(ui->networkButton, currentPage == MonitorPage::Network);
}

void MainWindow::updateSidebarAppearance()
{
    if (sidebarExpanded)
    {
        ui->sidebarFrame->setMinimumWidth(180);
        ui->sidebarFrame->setMaximumWidth(220);

        ui->sidebarTitleLabel->show();
        ui->sidebarToggleButton->setText("Collapse");

        ui->dashboardButton->setText("Dashboard");
        ui->cpuButton->setText("CPU");
        ui->memoryButton->setText("Memory");
        ui->diskButton->setText("Disk");
        ui->networkButton->setText("Network");
    }
    else
    {
        ui->sidebarFrame->setMinimumWidth(90);
        ui->sidebarFrame->setMaximumWidth(90);

        ui->sidebarTitleLabel->hide();
        ui->sidebarToggleButton->setText(">>");

        ui->dashboardButton->setText("Dash");
        ui->cpuButton->setText("CPU");
        ui->memoryButton->setText("Mem");
        ui->diskButton->setText("Disk");
        ui->networkButton->setText("Net");
    }

    updateSidebarHighlight();
}

void MainWindow::toggleSidebar()
{
    sidebarExpanded = !sidebarExpanded;
    updateSidebarAppearance();
}

std::vector<ProcessColumn> MainWindow::defaultColumnsForPage(MonitorPage page) const
{
    switch (page)
    {
    case MonitorPage::Dashboard:
        return {
            ProcessColumn::Name,
            ProcessColumn::PID,
            ProcessColumn::State,
            ProcessColumn::CpuPercent,
            ProcessColumn::CpuTimeSec,
            ProcessColumn::RssMB,
            ProcessColumn::Threads
        };

    case MonitorPage::CPU:
        return {
            ProcessColumn::Name,
            ProcessColumn::PID,
            ProcessColumn::State,
            ProcessColumn::CpuPercent,
            ProcessColumn::CpuTimeSec,
            ProcessColumn::Threads,
            ProcessColumn::Priority,
            ProcessColumn::NiceValue
        };

    case MonitorPage::Memory:
        return {
            ProcessColumn::Name,
            ProcessColumn::PID,
            ProcessColumn::State,
            ProcessColumn::RssMB,
            ProcessColumn::VszMB,
            ProcessColumn::SharedMB,
            ProcessColumn::Threads
        };

    case MonitorPage::Disk:
        return {
            ProcessColumn::Name,
            ProcessColumn::PID,
            ProcessColumn::State,
            ProcessColumn::ReadPerSec,
            ProcessColumn::WritePerSec,
            ProcessColumn::TotalReadBytes,
            ProcessColumn::TotalWriteBytes
        };

    case MonitorPage::Network:
        return {
            ProcessColumn::Name,
            ProcessColumn::PID,
            ProcessColumn::State,
            ProcessColumn::Threads
        };
    }

    return {};
}

QString MainWindow::columnTitle(ProcessColumn column) const
{
    switch (column)
    {
    case ProcessColumn::PID: return "PID";
    case ProcessColumn::Name: return "Name";
    case ProcessColumn::State: return "State";
    case ProcessColumn::CpuPercent: return "CPU %";
    case ProcessColumn::CpuTimeSec: return "CPU Time";
    case ProcessColumn::Threads: return "Threads";
    case ProcessColumn::RssMB: return "RSS MB";
    case ProcessColumn::VszMB: return "VSZ MB";
    case ProcessColumn::SharedMB: return "Shared MB";
    case ProcessColumn::ReadPerSec: return "Read/s";
    case ProcessColumn::WritePerSec: return "Write/s";
    case ProcessColumn::TotalReadBytes: return "Total Read";
    case ProcessColumn::TotalWriteBytes: return "Total Write";
    case ProcessColumn::Priority: return "Priority";
    case ProcessColumn::NiceValue: return "Nice";
    }

    return "";
}

QString MainWindow::columnText(const ProcessRow &row, ProcessColumn column) const
{
    switch (column)
    {
    case ProcessColumn::PID:
        return QString::number(row.pid);
    case ProcessColumn::Name:
        return QString::fromStdString(row.name);
    case ProcessColumn::State:
        return QString(row.state);
    case ProcessColumn::CpuPercent:
        return QString::number(row.cpuPercent, 'f', 1);
    case ProcessColumn::CpuTimeSec:
        return QString::number(row.cpuTimeSec, 'f', 1);
    case ProcessColumn::Threads:
        return QString::number(row.threads);
    case ProcessColumn::RssMB:
        return QString::number(row.rssMB, 'f', 1);
    case ProcessColumn::VszMB:
        return QString::number(row.vszMB, 'f', 1);
    case ProcessColumn::SharedMB:
        return QString::number(row.sharedMB, 'f', 1);
    case ProcessColumn::ReadPerSec:
        return QString::number(row.readBytesPerSec);
    case ProcessColumn::WritePerSec:
        return QString::number(row.writeBytesPerSec);
    case ProcessColumn::TotalReadBytes:
        return QString::number(row.totalReadBytes);
    case ProcessColumn::TotalWriteBytes:
        return QString::number(row.totalWriteBytes);
    case ProcessColumn::Priority:
        return QString::number(row.priority);
    case ProcessColumn::NiceValue:
        return QString::number(row.niceValue);
    }

    return "";
}

void MainWindow::setupProcessTable()
{
    ui->processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->processTable->setAlternatingRowColors(true);
    ui->processTable->verticalHeader()->setVisible(false);

    ui->processTable->setShowGrid(true);
    ui->processTable->setGridStyle(Qt::SolidLine);

    ui->processTable->horizontalHeader()->setStretchLastSection(false);
    ui->processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->processTable->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->processTable->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "  font-weight: normal;"
        "  padding: 4px;"
        "  border: 1px solid #d0d0d0;"
        "  background: #f7f7f7;"
        "}"
        );

    connect(ui->processTable->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &MainWindow::onProcessHeaderClicked);

    connect(ui->processTable->horizontalHeader(), &QWidget::customContextMenuRequested,
            this, &MainWindow::showProcessHeaderMenu);

    rebuildProcessTableColumns();

    ui->mainVerticalSplitter->setStretchFactor(0, 3);
    ui->mainVerticalSplitter->setStretchFactor(1, 2);
    ui->mainVerticalSplitter->setChildrenCollapsible(false);
}

void MainWindow::rebuildProcessTableColumns()
{
    ui->processTable->clearContents();
    ui->processTable->setRowCount(0);
    ui->processTable->setColumnCount(static_cast<int>(visibleColumns.size()));

    QStringList headers;
    for (ProcessColumn column : visibleColumns)
    {
        QString text = columnTitle(column);

        if (column == currentSortColumn)
        {
            if (currentSortState == SortState::Ascending)
                text += " ▲";
            else if (currentSortState == SortState::Descending)
                text += " ▼";
        }

        headers << text;
    }

    ui->processTable->setHorizontalHeaderLabels(headers);

    QHeaderView *header = ui->processTable->horizontalHeader();

    for (int i = 0; i < static_cast<int>(visibleColumns.size()); ++i)
    {
        ProcessColumn column = visibleColumns[i];

        if (column == ProcessColumn::Name)
        {
            header->setSectionResizeMode(i, QHeaderView::Stretch);
        }
        else
        {
            header->setSectionResizeMode(i, QHeaderView::Interactive);

            switch (column)
            {
            case ProcessColumn::PID:
                ui->processTable->setColumnWidth(i, 70);
                break;
            case ProcessColumn::State:
                ui->processTable->setColumnWidth(i, 60);
                break;
            case ProcessColumn::CpuPercent:
                ui->processTable->setColumnWidth(i, 75);
                break;
            case ProcessColumn::CpuTimeSec:
                ui->processTable->setColumnWidth(i, 90);
                break;
            case ProcessColumn::Threads:
                ui->processTable->setColumnWidth(i, 80);
                break;
            case ProcessColumn::RssMB:
            case ProcessColumn::VszMB:
            case ProcessColumn::SharedMB:
                ui->processTable->setColumnWidth(i, 90);
                break;
            case ProcessColumn::ReadPerSec:
            case ProcessColumn::WritePerSec:
                ui->processTable->setColumnWidth(i, 95);
                break;
            case ProcessColumn::TotalReadBytes:
            case ProcessColumn::TotalWriteBytes:
                ui->processTable->setColumnWidth(i, 110);
                break;
            case ProcessColumn::Priority:
            case ProcessColumn::NiceValue:
                ui->processTable->setColumnWidth(i, 70);
                break;
            default:
                ui->processTable->setColumnWidth(i, 90);
                break;
            }
        }
    }
}

void MainWindow::populateProcessTable(const std::vector<ProcessRow> &rows)
{
    int newRowCount = static_cast<int>(rows.size());

    if (ui->processTable->rowCount() != newRowCount)
        ui->processTable->setRowCount(newRowCount);

    for (int rowIndex = 0; rowIndex < newRowCount; ++rowIndex)
    {
        const ProcessRow &row = rows[rowIndex];

        for (int colIndex = 0; colIndex < static_cast<int>(visibleColumns.size()); ++colIndex)
        {
            ProcessColumn column = visibleColumns[colIndex];
            QString text = columnText(row, column);

            QTableWidgetItem *item = ui->processTable->item(rowIndex, colIndex);
            if (!item)
            {
                item = new QTableWidgetItem(text);
                ui->processTable->setItem(rowIndex, colIndex, item);
            }
            else if (item->text() != text)
            {
                item->setText(text);
            }
        }
    }
}

bool MainWindow::compareRows(const ProcessRow &a,
                             const ProcessRow &b,
                             ProcessColumn column,
                             bool descending) const
{
    auto lessThan = [&](auto lhs, auto rhs) {
        return descending ? (lhs > rhs) : (lhs < rhs);
    };

    switch (column)
    {
    case ProcessColumn::PID: return lessThan(a.pid, b.pid);
    case ProcessColumn::Name: return lessThan(a.name, b.name);
    case ProcessColumn::State: return lessThan(a.state, b.state);
    case ProcessColumn::CpuPercent: return lessThan(a.cpuPercent, b.cpuPercent);
    case ProcessColumn::CpuTimeSec: return lessThan(a.cpuTimeSec, b.cpuTimeSec);
    case ProcessColumn::Threads: return lessThan(a.threads, b.threads);
    case ProcessColumn::RssMB: return lessThan(a.rssMB, b.rssMB);
    case ProcessColumn::VszMB: return lessThan(a.vszMB, b.vszMB);
    case ProcessColumn::SharedMB: return lessThan(a.sharedMB, b.sharedMB);
    case ProcessColumn::ReadPerSec: return lessThan(a.readBytesPerSec, b.readBytesPerSec);
    case ProcessColumn::WritePerSec: return lessThan(a.writeBytesPerSec, b.writeBytesPerSec);
    case ProcessColumn::TotalReadBytes: return lessThan(a.totalReadBytes, b.totalReadBytes);
    case ProcessColumn::TotalWriteBytes: return lessThan(a.totalWriteBytes, b.totalWriteBytes);
    case ProcessColumn::Priority: return lessThan(a.priority, b.priority);
    case ProcessColumn::NiceValue: return lessThan(a.niceValue, b.niceValue);
    }

    return false;
}

std::vector<ProcessRow> MainWindow::applySorting(const std::vector<ProcessRow> &rows) const
{
    if (currentSortState == SortState::Normal)
        return rows;

    std::vector<ProcessRow> sorted = rows;
    bool descending = (currentSortState == SortState::Descending);

    std::sort(sorted.begin(), sorted.end(),
              [&](const ProcessRow &a, const ProcessRow &b)
              {
                  return compareRows(a, b, currentSortColumn, descending);
              });

    return sorted;
}

void MainWindow::onProcessHeaderClicked(int logicalIndex)
{
    if (logicalIndex < 0 || logicalIndex >= static_cast<int>(visibleColumns.size()))
        return;

    ProcessColumn clickedColumn = visibleColumns[logicalIndex];

    if (clickedColumn != currentSortColumn)
    {
        currentSortColumn = clickedColumn;
        currentSortState = SortState::Descending;
    }
    else
    {
        switch (currentSortState)
        {
        case SortState::Normal:
            currentSortState = SortState::Descending;
            break;
        case SortState::Descending:
            currentSortState = SortState::Ascending;
            break;
        case SortState::Ascending:
            currentSortState = SortState::Normal;
            break;
        }
    }

    rebuildProcessTableColumns();
    populateProcessTable(applySorting(baseRows));
}

void MainWindow::showProcessHeaderMenu(const QPoint &pos)
{
    QMenu menu(this);

    for (ProcessColumn column : columnOrder)
    {
        QAction *action = menu.addAction(columnTitle(column));
        action->setCheckable(true);

        bool visible = std::find(visibleColumns.begin(), visibleColumns.end(), column) != visibleColumns.end();
        action->setChecked(visible);

        connect(action, &QAction::triggered, this, [this, column]() {
            auto it = std::find(visibleColumns.begin(), visibleColumns.end(), column);

            if (it != visibleColumns.end())
            {
                if (column == ProcessColumn::Name)
                    return;

                visibleColumns.erase(it);
            }
            else
            {
                auto orderPos = [&](ProcessColumn c) {
                    return std::find(columnOrder.begin(), columnOrder.end(), c);
                };

                auto insertPos = visibleColumns.end();
                for (auto vit = visibleColumns.begin(); vit != visibleColumns.end(); ++vit)
                {
                    if (orderPos(column) < orderPos(*vit))
                    {
                        insertPos = vit;
                        break;
                    }
                }

                visibleColumns.insert(insertPos, column);
            }

            rebuildProcessTableColumns();
            populateProcessTable(applySorting(baseRows));
        });
    }

    menu.exec(ui->processTable->horizontalHeader()->mapToGlobal(pos));
}

void MainWindow::setCurrentPage(MonitorPage page)
{
    currentPage = page;
    visibleColumns = defaultColumnsForPage(page);
    currentSortState = SortState::Normal;

    rebuildProcessTableColumns();
    populateProcessTable(applySorting(baseRows));
    updatePageHeader();
    updateSidebarHighlight();
    applyDefaultSplitterSizes();

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
        ui->mainScrollArea->verticalScrollBar()->setValue(0);
    });
}

void MainWindow::showDashboardPage()
{
    setCurrentPage(MonitorPage::Dashboard);
}

void MainWindow::showCpuPage()
{
    setCurrentPage(MonitorPage::CPU);
}

void MainWindow::showMemoryPage()
{
    setCurrentPage(MonitorPage::Memory);
}

void MainWindow::showDiskPage()
{
    setCurrentPage(MonitorPage::Disk);
}

void MainWindow::showNetworkPage()
{
    setCurrentPage(MonitorPage::Network);
}

void MainWindow::onStatsResult(SystemData data, std::vector<ProcessRow> rows)
{
    updateCpuStats(data);
    updateMemoryStats(data);
    updateDiskStats(data);
    updateNetworkStats(data);
    updateGraphs(data);

    baseRows = rows;
    populateProcessTable(applySorting(baseRows));
}
