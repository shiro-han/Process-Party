#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "systemstats.h"

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
#include <QMessageBox>
#include <signal.h>
#include <errno.h>
#include <cstring>

#include <QSettings>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QFontComboBox>
#include <QFileDialog>

#include <QPixmap>
#include <QIcon>

#include <algorithm>
#include <fstream>

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
    ui->historyPageLayout->setStretch(0, 0);
    ui->historyPageLayout->setStretch(1, 0);
    ui->historyPageLayout->setStretch(2, 1);
    
    hide();

    // ICON
    QIcon appIcon(":/capyy_2.png");

    if (!appIcon.isNull()) {
        setWindowIcon(appIcon);
        qApp->setWindowIcon(appIcon);
        QApplication::setWindowIcon(appIcon);
    } else {
        qWarning() << "Warning: Could not load application icon ':/capyy_2.png'";
    }

    // Set logo in sidebar
    QPixmap logo(":/capyy_2.png");
    ui->sidebarTitleLabel->setPixmap(
        logo.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );

    loadThemeSettings();
    applyTheme();


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
    ui->sidebarFrame->updateGeometry();
    ui->centralwidget->updateGeometry();


    setupGraphs();
    setupProcessTable();
    setupHistoryTable();
    
    currentHistorySortColumn = HistoryColumn::LastSeen;
    currentHistorySortState = SortState::Normal;
    
    updatePageHeader();
    updateSidebarAppearance();
    updateSidebarHighlight();

    connect(ui->processSearchBar, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    applyDefaultSplitterSizes();

    connect(ui->sidebarToggleButton, &QPushButton::clicked, this, &MainWindow::toggleSidebar);

    connect(ui->dashboardButton, &QPushButton::clicked, this, &MainWindow::showDashboardPage);
    connect(ui->cpuButton, &QPushButton::clicked, this, &MainWindow::showCpuPage);
    connect(ui->memoryButton, &QPushButton::clicked, this, &MainWindow::showMemoryPage);
    connect(ui->diskButton, &QPushButton::clicked, this, &MainWindow::showDiskPage);
    connect(ui->networkButton, &QPushButton::clicked, this, &MainWindow::showNetworkPage);
    connect(ui->historyButton, &QPushButton::clicked, this, &MainWindow::showHistoryPage);
    
    connect(ui->historySearchBar, &QLineEdit::textChanged,
        this, &MainWindow::onHistorySearchTextChanged);
        
    connect(ui->historyTable->horizontalHeader(), &QHeaderView::sectionClicked,
        this, &MainWindow::onHistoryHeaderClicked);

    connect(ui->historyExportButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "Export History CSV",
            QDir::homePath() + "/history_export.csv",
            "CSV Files (*.csv)"
        );

        if (fileName.isEmpty())
            return;

        exportSelectedHistoryToCSV(fileName.toStdString());
    });
    
    connect(ui->historyStartButton, &QPushButton::clicked,
        this, &MainWindow::onStartRecordingClicked);
    connect(ui->historyRecordForButton, &QPushButton::clicked,
        this, &MainWindow::onRecordForClicked);
    connect(ui->historyStopButton, &QPushButton::clicked,
            this, &MainWindow::onStopRecordingClicked);
    connect(ui->historySessionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onRecordingSessionChanged);
    connect(ui->historyRecordOnStartupCheckBox, &QCheckBox::toggled,
        this, &MainWindow::onRecordOnStartupToggled);

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

    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::showSettingsDialog);

    // Move worker onto background thread
    statsWorker->moveToThread(workerThread);

    // Timer fires on main thread → triggers worker on background thread
    connect(timer, &QTimer::timeout, this, &MainWindow::requestStats);

    // Wire worker result back to main thread
    connect(this, &MainWindow::requestStats, statsWorker, &StatsWorker::run);
    connect(statsWorker, &StatsWorker::result, this, &MainWindow::onStatsResult);
    connect(workerThread, &QThread::finished, statsWorker, &QObject::deleteLater);

    workerThread->start();
    
    if (recordOnStartup)
    {
        startRecordingSession();
    }
    
    // Default interval: 1000ms (1 second)
    timer->start(1000);
    
    // Kick off first sample immediately without waiting for first tick
    emit requestStats();

    // Force fix process table header
    QTimer::singleShot(50, this, [this]() {
        QColor sectionHeader = currentTheme.value("sectionHeader");
        QColor textColor     = currentTheme.value("text");
        QColor borderColor   = currentTheme.value("border");

        QString headerStyle = QString(
            "QHeaderView::section { "
            "background-color: %1; "
            "color: %2; "
            "font-weight: bold; "
            "border: 1px solid %3; "
            "padding: 4px 6px; "
            "}"
        ).arg(sectionHeader.name())
         .arg(textColor.name())
         .arg(borderColor.name());

        if (ui->processTable && ui->processTable->horizontalHeader())
        {
            ui->processTable->horizontalHeader()->setStyleSheet(headerStyle);
            ui->processTable->horizontalHeader()->repaint();   // Force redraw
        }
        applySavedFont();
    });
}

MainWindow::~MainWindow()
{
    workerThread->quit();
    workerThread->wait();
    delete ui;
}

void MainWindow::setRefreshInterval(int milliseconds)
{
    timer->setInterval(milliseconds);
}

void MainWindow::startRecordingSession()
{
    RecordingSession session;
    session.startedAt = std::chrono::system_clock::now();
    session.endedAt = session.startedAt;

    recordingSessions.push_back(session);
    currentRecordingSessionIndex = static_cast<int>(recordingSessions.size()) - 1;
    isRecording = true;
    
    refreshRecordingSessionComboBox();
}

void MainWindow::stopRecordingSession()
{
    if (currentRecordingSessionIndex >= 0 &&
        currentRecordingSessionIndex < static_cast<int>(recordingSessions.size()))
    {
        recordingSessions[currentRecordingSessionIndex].endedAt = std::chrono::system_clock::now();
    }

    isRecording = false;
    
    refreshRecordingSessionComboBox();
}

const RecordingSession* MainWindow::selectedRecordingSession() const
{
    if (currentRecordingSessionIndex < 0 ||
        currentRecordingSessionIndex >= static_cast<int>(recordingSessions.size()))
    {
        return nullptr;
    }

    return &recordingSessions[currentRecordingSessionIndex];
}

RecordingSession* MainWindow::selectedRecordingSession()
{
    if (currentRecordingSessionIndex < 0 ||
        currentRecordingSessionIndex >= static_cast<int>(recordingSessions.size()))
    {
        return nullptr;
    }

    return &recordingSessions[currentRecordingSessionIndex];
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
    if (cpuGraph) cpuGraph->setVisible(cpuUsageExpanded);
    ui->cpuUsageToggleButton->setText(cpuUsageExpanded ? "CPU Usage ▼" : "CPU Usage ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleCpuPerCoreSection()
{
    cpuPerCoreExpanded = !cpuPerCoreExpanded;
    ui->cpuPerCoreScrollArea->setVisible(cpuPerCoreExpanded);
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
    if (memoryGraph) memoryGraph->setVisible(memoryUsageExpanded);
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
    if (memoryUsedGraph) memoryUsedGraph->setVisible(memoryUsedAvailableExpanded);
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
    if (memoryCacheGraph) memoryCacheGraph->setVisible(memoryCacheBuffersExpanded);
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
    if (memorySwapBar) memorySwapBar->setVisible(memorySwapExpanded);
    ui->memorySwapToggleButton->setText(memorySwapExpanded ? "Swap ▼" : "Swap ►");
    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    QString query = text.trimmed();

    for (int row = 0; row < ui->processTable->rowCount(); ++row) {
        QTableWidgetItem *pidItem = ui->processTable->item(row, 1);   // PID column
        QTableWidgetItem *nameItem = ui->processTable->item(row, 0);  // Name column

        bool match = false;

        if (query.isEmpty()) {
            match = true;
        } else {
            QString pidText = pidItem ? pidItem->text() : "";
            QString nameText = nameItem ? nameItem->text() : "";

            match = pidText.startsWith(query) ||
                    nameText.contains(query, Qt::CaseInsensitive);
        }

        ui->processTable->setRowHidden(row, !match);
    }
}

void MainWindow::toggleDiskUsageSection()
{
    diskUsageExpanded = !diskUsageExpanded;
    if (diskBarGraph) diskBarGraph->setVisible(diskUsageExpanded);
    ui->diskUsageToggleButton->setText(diskUsageExpanded ? "Disk Usage ▼" : "Disk Usage ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleDiskActivitySection()
{
    diskActivityExpanded = !diskActivityExpanded;
    if (diskGraph) diskGraph->setVisible(diskActivityExpanded);
    ui->diskActivityToggleButton->setText(diskActivityExpanded ? "Disk Activity ▼" : "Disk Activity ►");

    updateCurrentPageHeight();

    QTimer::singleShot(0, this, [this]() {
        updateCurrentPageHeight();
    });
}

void MainWindow::toggleNetworkTrafficSection()
{
    networkTrafficExpanded = !networkTrafficExpanded;
    if (networkGraph) networkGraph->setVisible(networkTrafficExpanded);
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
    ui->networkInterfacesScrollArea->setVisible(networkInterfacesExpanded);
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
    case MonitorPage::History:
        ui->pageTitleLabel->setText("History");
        ui->stackedWidget->setCurrentWidget(ui->historyPage);
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
        
    case MonitorPage::History:
        sizes << 1000 << 0;
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

    if (currentPage == MonitorPage::History)
    {
        ui->stackedWidget->setMinimumHeight(0);
        ui->stackedWidget->setMaximumHeight(QWIDGETSIZE_MAX);

        ui->mainScrollAreaWidgetContents->adjustSize();
        ui->mainScrollAreaWidgetContents->updateGeometry();
        ui->mainScrollArea->widget()->adjustSize();
        ui->mainScrollArea->widget()->updateGeometry();
        return;
    }

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
    QColor accent = currentTheme.value("accent", QColor("#3b82f6"));
    QColor text   = currentTheme.value("text",   QColor("#1e2937"));
    QColor bg     = currentTheme.value("sectionHeader", QColor("#f1f5f9"));

    if (active)
    {
        button->setStyleSheet(QString(
            "QPushButton {"
            "  font-weight: bold;"
            "  background-color: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 6px;"
            "  padding: 6px 8px;"
            "  text-align: left;"
            "  color: %3;"
            "}"
        ).arg(accent.lighter(160).name())
         .arg(accent.name())
         .arg(text.name()));
    }
    else
    {
        button->setStyleSheet(QString(
            "QPushButton {"
            "  font-weight: normal;"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "  padding: 6px 8px;"
            "  text-align: left;"
            "  color: %3;"
            "}"
        ).arg(bg.name())
         .arg(accent.darker(120).name())
         .arg(text.name()));
    }
}

void MainWindow::updateSidebarHighlight()
{
    setNavButtonActive(ui->dashboardButton, currentPage == MonitorPage::Dashboard);
    setNavButtonActive(ui->cpuButton, currentPage == MonitorPage::CPU);
    setNavButtonActive(ui->memoryButton, currentPage == MonitorPage::Memory);
    setNavButtonActive(ui->diskButton, currentPage == MonitorPage::Disk);
    setNavButtonActive(ui->networkButton, currentPage == MonitorPage::Network);
    setNavButtonActive(ui->historyButton, currentPage == MonitorPage::History);

    ui->sidebarFrame->update();
}

void MainWindow::updateSidebarAppearance()
{
    bool expanded = sidebarExpanded;

    ui->sidebarFrame->setMinimumWidth(expanded ? 180 : 90);
    ui->sidebarFrame->setMaximumWidth(expanded ? 220 : 90);
    ui->sidebarTitleLabel->setVisible(expanded);

    ui->sidebarToggleButton->setText(expanded ? "Collapse" : ">>");

    ui->dashboardButton->setText(expanded ? "Dashboard" : "Dash");
    ui->cpuButton->setText(expanded ? "CPU" : "CPU");
    ui->memoryButton->setText(expanded ? "Memory" : "Mem");
    ui->diskButton->setText(expanded ? "Disk" : "Disk");
    ui->networkButton->setText(expanded ? "Network" : "Net");
    ui->settingsButton->setText(expanded ? "Settings" : "     ⚙");

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
    ui->processTable->setColumnCount(9);
    QStringList headers;
    headers << "PID"
            << "Name"
            << "State"
            << "CPU %"
            << "RSS MB"
            << "Threads"
            << "Read/s"
            << "Write/s"
            << "CPU Time";
    ui->processTable->setHorizontalHeaderLabels(headers);
    ui->processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->processTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->processTable->setAlternatingRowColors(true);
    ui->processTable->verticalHeader()->setVisible(false);
    ui->processTable->horizontalHeader()->setStretchLastSection(true);
    ui->processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

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
    ui->processTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->processTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showProcessContextMenu);
}

void MainWindow::setupHistoryTable()
{
    ui->historyTable->setColumnCount(9);

    QStringList headers;
    headers << "Name"
            << "PID"
            << "Samples"
            << "Status"
            << "Avg CPU %"
            << "Peak CPU %"
            << "Avg RSS MB"
            << "First Seen"
            << "Last Seen";

    ui->historyTable->setHorizontalHeaderLabels(headers);
    ui->historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->historyTable->setAlternatingRowColors(true);
    ui->historyTable->verticalHeader()->setVisible(false);

    ui->historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    ui->historyTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->historyTable->setMinimumHeight(0);
    
    updateHistoryHeaderLabels();
}

void MainWindow::populateHistoryTable()
{
    int previouslySelectedPid = -1;
    qlonglong previouslySelectedStartTimeTicks = -1;

    int selectedRow = ui->historyTable->currentRow();
    if (selectedRow >= 0)
    {
        QTableWidgetItem *selectedPidItem = ui->historyTable->item(selectedRow, 1);
        if (selectedPidItem)
        {
            previouslySelectedPid = selectedPidItem->text().toInt();
            previouslySelectedStartTimeTicks = selectedPidItem->data(Qt::UserRole).toLongLong();
        }
    }
    
    ui->historyTable->setRowCount(0);

    const RecordingSession* session = selectedRecordingSession();
    if (!session)
    {
        ui->historySummaryLabel->setText("History: no recording session selected");
        return;
    }
    
    struct HistorySummary
    {
        int pid = 0;
        long long startTimeTicks = 0;
        QString name;
        QString firstSeen;
        QString lastSeen;
        int sampleCount = 0;
        double cpuSum = 0.0;
        double cpuPeak = 0.0;
        double rssSum = 0.0;
        bool running = false;
    };

    std::unordered_map<std::string, HistorySummary> summaries;

    for (const auto &entry : session->history)
    {
        std::time_t t = std::chrono::system_clock::to_time_t(entry.first);
        QString timestamp = QString::fromStdString(std::string(std::ctime(&t)));
        timestamp = timestamp.trimmed();

        for (const auto &row : entry.second)
        {
            std::string key = std::to_string(row.pid) + "_" + std::to_string(row.startTimeTicks);
            auto &summary = summaries[key];

            if (summary.sampleCount == 0)
            {
                summary.pid = row.pid;
                summary.startTimeTicks = row.startTimeTicks;
                summary.name = QString::fromStdString(row.name);
                summary.firstSeen = timestamp;
                summary.cpuPeak = row.cpuPercent;
            }

            summary.lastSeen = timestamp;
            summary.sampleCount++;
            summary.cpuSum += row.cpuPercent;
            summary.rssSum += row.rssMB;

            if (row.cpuPercent > summary.cpuPeak)
                summary.cpuPeak = row.cpuPercent;

            summary.name = QString::fromStdString(row.name);
        }
    }

    if (!session->history.empty())
    {
        const auto &latestSnapshot = session->history.back().second;
        for (const auto &row : latestSnapshot)
        {
            std::string key = std::to_string(row.pid) + "_" + std::to_string(row.startTimeTicks);
            auto it = summaries.find(key);
            if (it != summaries.end())
                it->second.running = true;
        }
    }

    std::vector<HistorySummary> rows;
    rows.reserve(summaries.size());

    for (const auto &pair : summaries)
        rows.push_back(pair.second);

    if (currentHistorySortState == SortState::Normal)
    {
        std::sort(rows.begin(), rows.end(),
                  [](const HistorySummary &a, const HistorySummary &b)
                  {
                      return a.lastSeen > b.lastSeen;
                  });
    }
    else
    {
        bool descending = (currentHistorySortState == SortState::Descending);

        std::sort(rows.begin(), rows.end(),
                  [this, descending](const HistorySummary &a, const HistorySummary &b)
                  {
                      switch (currentHistorySortColumn)
                      {
                      case HistoryColumn::Name:
                          return descending ? (a.name > b.name) : (a.name < b.name);

                      case HistoryColumn::PID:
                          return descending ? (a.pid > b.pid) : (a.pid < b.pid);

                      case HistoryColumn::Samples:
                          return descending ? (a.sampleCount > b.sampleCount) : (a.sampleCount < b.sampleCount);

                      case HistoryColumn::Status:
                          return descending ? ((a.running ? 1 : 0) > (b.running ? 1 : 0))
                                            : ((a.running ? 1 : 0) < (b.running ? 1 : 0));

                      case HistoryColumn::AvgCpu:
                      {
                          double aAvg = (a.sampleCount > 0)
                              ? (a.cpuSum / static_cast<double>(a.sampleCount))
                              : 0.0;
                          double bAvg = (b.sampleCount > 0)
                              ? (b.cpuSum / static_cast<double>(b.sampleCount))
                              : 0.0;
                          return descending ? (aAvg > bAvg) : (aAvg < bAvg);
                      }

                      case HistoryColumn::PeakCpu:
                          return descending ? (a.cpuPeak > b.cpuPeak) : (a.cpuPeak < b.cpuPeak);

                      case HistoryColumn::AvgRssMB:
                      {
                          double aAvg = (a.sampleCount > 0)
                              ? (a.rssSum / static_cast<double>(a.sampleCount))
                              : 0.0;
                          double bAvg = (b.sampleCount > 0)
                              ? (b.rssSum / static_cast<double>(b.sampleCount))
                              : 0.0;
                          return descending ? (aAvg > bAvg) : (aAvg < bAvg);
                      }

                      case HistoryColumn::FirstSeen:
                          return descending ? (a.firstSeen > b.firstSeen) : (a.firstSeen < b.firstSeen);

                      case HistoryColumn::LastSeen:
                          return descending ? (a.lastSeen > b.lastSeen) : (a.lastSeen < b.lastSeen);
                      }

                      return false;
                  });
    }

    int rowIndex = 0;
    for (const auto &summary : rows)
    {
        ui->historyTable->insertRow(rowIndex);

        double avgCpu = (summary.sampleCount > 0)
            ? (summary.cpuSum / static_cast<double>(summary.sampleCount))
            : 0.0;

        double avgRss = (summary.sampleCount > 0)
            ? (summary.rssSum / static_cast<double>(summary.sampleCount))
            : 0.0;

        QTableWidgetItem *pidItem = new QTableWidgetItem(QString::number(summary.pid));
        pidItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(summary.startTimeTicks)));

        ui->historyTable->setItem(rowIndex, 0, new QTableWidgetItem(summary.name));
        ui->historyTable->setItem(rowIndex, 1, pidItem);
        ui->historyTable->setItem(rowIndex, 2, new QTableWidgetItem(QString::number(summary.sampleCount)));
        ui->historyTable->setItem(rowIndex, 3, new QTableWidgetItem(summary.running ? "Running" : "Exited"));
        ui->historyTable->setItem(rowIndex, 4, new QTableWidgetItem(QString::number(avgCpu, 'f', 1)));
        ui->historyTable->setItem(rowIndex, 5, new QTableWidgetItem(QString::number(summary.cpuPeak, 'f', 1)));
        ui->historyTable->setItem(rowIndex, 6, new QTableWidgetItem(QString::number(avgRss, 'f', 1)));
        ui->historyTable->setItem(rowIndex, 7, new QTableWidgetItem(summary.firstSeen));
        ui->historyTable->setItem(rowIndex, 8, new QTableWidgetItem(summary.lastSeen));
        
        if (summary.pid == previouslySelectedPid &&
            summary.startTimeTicks == previouslySelectedStartTimeTicks)
        {
            ui->historyTable->selectRow(rowIndex);
        }
        
        ++rowIndex;
    }

    ui->historySummaryLabel->setText(
    QString("History: %1 snapshots recorded, %2 process sessions")
        .arg(session->history.size())
        .arg(rowIndex)
    );

    onHistorySearchTextChanged(ui->historySearchBar->text());
}

void MainWindow::onStartRecordingClicked()
{
    startRecordingSession();
    populateHistoryTable();

    QMessageBox::information(this, "Recording",
                             "Started a new recording session.");
}

void MainWindow::onStopRecordingClicked()
{
    if (!isRecording)
    {
        QMessageBox::information(this, "Recording",
                                 "Recording is already stopped.");
        return;
    }

    timedRecordingActive = false;
    stopRecordingSession();
    populateHistoryTable();

    QMessageBox::information(this, "Recording",
                             "Recording session stopped.");
}

void MainWindow::onHistorySearchTextChanged(const QString &text)
{
    QString query = text.trimmed();

    for (int row = 0; row < ui->historyTable->rowCount(); ++row)
    {
        QTableWidgetItem *nameItem = ui->historyTable->item(row, 0);
        QTableWidgetItem *pidItem = ui->historyTable->item(row, 1);

        bool match = false;

        if (query.isEmpty())
        {
            match = true;
        }
        else
        {
            QString pidText = pidItem ? pidItem->text() : "";
            QString nameText = nameItem ? nameItem->text() : "";

            match = pidText.startsWith(query) ||
                    nameText.contains(query, Qt::CaseInsensitive);
        }

        ui->historyTable->setRowHidden(row, !match);
    }
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
    int pidColumnIndex = -1;
    for (int i = 0; i < static_cast<int>(visibleColumns.size()); ++i)
    {
        if (visibleColumns[i] == ProcessColumn::PID)
        {
            pidColumnIndex = i;
            break;
        }
    }

    int previouslySelectedPid = -1;
    qlonglong previouslySelectedStartTimeTicks = -1;

    int selectedRow = ui->processTable->currentRow();
    if (selectedRow >= 0 && pidColumnIndex >= 0)
    {
        QTableWidgetItem *selectedPidItem = ui->processTable->item(selectedRow, pidColumnIndex);
        if (selectedPidItem)
        {
            previouslySelectedPid = selectedPidItem->text().toInt();
            previouslySelectedStartTimeTicks = selectedPidItem->data(Qt::UserRole).toLongLong();
        }
    }

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

                if (column == ProcessColumn::PID)
                {
                    item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(row.startTimeTicks)));
                }

                ui->processTable->setItem(rowIndex, colIndex, item);
            }
            else
            {
                if (item->text() != text)
                    item->setText(text);

                if (column == ProcessColumn::PID)
                {
                    item->setData(Qt::UserRole, QVariant::fromValue(static_cast<qlonglong>(row.startTimeTicks)));
                }
            }
        }

        if (row.pid == previouslySelectedPid &&
            row.startTimeTicks == previouslySelectedStartTimeTicks)
        {
            ui->processTable->selectRow(rowIndex);
        }
    }

    onSearchTextChanged(ui->processSearchBar->text());
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

    bool isHistoryPage = (page == MonitorPage::History);

    ui->processDividerFrame->setVisible(!isHistoryPage);
    ui->processSectionLabel->setVisible(!isHistoryPage);
    ui->processSearchBar->setVisible(!isHistoryPage);
    ui->processTable->setVisible(!isHistoryPage);

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

void MainWindow::showHistoryPage()
{
    setCurrentPage(MonitorPage::History);
    refreshRecordingSessionComboBox();
    populateHistoryTable();
}

void MainWindow::onStatsResult(SystemData data, std::vector<ProcessRow> rows)
{
    // history capture
    auto now = std::chrono::system_clock::now();

    if (isRecording)
    {
        RecordingSession* session = selectedRecordingSession();
        if (session)
        {
            session->history.push_back({now, rows});
            session->endedAt = now;

            const size_t MAX_HISTORY = 1000;
            if (session->history.size() > MAX_HISTORY)
            {
                session->history.erase(session->history.begin());
            }
        }
    }
    
    updateCpuStats(data);
    updateMemoryStats(data);
    updateDiskStats(data);
    updateNetworkStats(data);
    updateGraphs(data);

    baseRows = rows;
    populateProcessTable(applySorting(baseRows));
    
    checkTimedRecordingStop();
    
    if (currentPage == MonitorPage::History)
    {
        populateHistoryTable();
    }
}

// right click for Process
void MainWindow::showProcessContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = ui->processTable->itemAt(pos);
    if (!item) return;

    // Pause the table updates while menu is open
    bool wasTimerActive = timer->isActive();
    if (wasTimerActive) {
        timer->stop();
    }

    ui->processTable->selectRow(item->row());

    QMenu menu(this);
    QAction *terminateAction = menu.addAction("Terminate Process (SIGTERM)");
    QAction *killAction = menu.addAction("Kill Process (SIGKILL)");

    connect(terminateAction, &QAction::triggered, this, [this, wasTimerActive]() {
        sendSignalToSelectedProcess(SIGTERM);
        if (wasTimerActive) timer->start(1000);   // Resume
    });

    connect(killAction, &QAction::triggered, this, [this, wasTimerActive]() {
        sendSignalToSelectedProcess(SIGKILL);
        if (wasTimerActive) timer->start(1000);   // Resume
    });

    // Resume if user clicks outside the menu (cancels)
    connect(&menu, &QMenu::aboutToHide, this, [this, wasTimerActive]() {
        if (wasTimerActive && !timer->isActive()) {
            timer->start(1000);
        }
    });

    menu.exec(ui->processTable->viewport()->mapToGlobal(pos));
}

void MainWindow::sendSignalToSelectedProcess(int signal)
{
    int row = ui->processTable->currentRow();
    if (row < 0)
        return;

    int pidColIndex = -1;
    for (int i = 0; i < static_cast<int>(visibleColumns.size()); ++i)
    {
        if (visibleColumns[i] == ProcessColumn::PID)
        {
            pidColIndex = i;
            break;
        }
    }

    if (pidColIndex < 0)
        return;

    QTableWidgetItem *pidItem = ui->processTable->item(row, pidColIndex);
    QTableWidgetItem *nameItem = ui->processTable->item(row, 0);
    if (!pidItem || !nameItem)
        return;

    int pid = pidItem->text().toInt();
    if (pid <= 0)
        return;

    QString signalName = (signal == SIGKILL) ? "Kill (SIGKILL)" : "Terminate (SIGTERM)";
    QString confirmMsg = (signal == SIGKILL)
        ? "This will immediately and forcefully kill the process with no chance to clean up."
        : "This will ask the process to shut down cleanly.";

    QMessageBox::StandardButton reply = QMessageBox::warning(
        this,
        signalName,
        QString("%1\n\nProcess: %2 (PID %3)\n\nContinue?")
            .arg(confirmMsg)
            .arg(nameItem->text())
            .arg(pid),
        QMessageBox::Yes | QMessageBox::Cancel
    );

    if (reply != QMessageBox::Yes)
        return;

    if (kill(pid, signal) == 0)
    {
        QString successMsg = (signal == SIGTERM)
            ? QString("Terminate signal sent to process %1 (PID %2).\nIt may take a moment to stop.")
                .arg(nameItem->text()).arg(pid)
            : QString("Process %1 (PID %2) was killed.")
                .arg(nameItem->text()).arg(pid);

        QMessageBox::information(this, "Success", successMsg);
    }
    else
    {
        QString reason;
        if (errno == EPERM)
            reason = "Permission denied — you may need to run as root to kill this process.";
        else if (errno == ESRCH)
            reason = "Process no longer exists.";
        else
            reason = QString::fromLocal8Bit(strerror(errno));

        QMessageBox::critical(this, "Error",
            QString("Failed to send %1 to process %2 (PID %3):\n%4")
                .arg(signalName)
                .arg(nameItem->text())
                .arg(pid)
                .arg(reason));
    }
}

void MainWindow::loadThemeSettings()
{
    QSettings settings("ProcessParty", "Theme");

    // Current themes from settings (saved to disk)
    currentTheme["accent"]        = QColor(settings.value("accent",        "#3b82f6").toString());
    currentTheme["background"]    = QColor(settings.value("background",    "#f8fafc").toString());
    currentTheme["panel"]         = QColor(settings.value("panel",         "#ffffff").toString());
    currentTheme["sectionHeader"] = QColor(settings.value("sectionHeader", "#f1f5f9").toString());
    currentTheme["text"]          = QColor(settings.value("text",          "#1e2937").toString());
    currentTheme["tableAlt"]      = QColor(settings.value("tableAlt",      "#f1f5f9").toString());
    currentTheme["border"]        = QColor(settings.value("border",        "#64748b").toString());

    // Current fonts from settings (saved to disk)
    m_savedFontFamily = settings.value("fontFamily", "Segoe UI").toString();
    m_savedFontSize   = settings.value("fontSize", 9).toInt();
}

void MainWindow::saveThemeSettings()
{
    QSettings settings("ProcessParty", "Theme");
    for (auto it = currentTheme.begin(); it != currentTheme.end(); ++it) {
        settings.setValue(it.key(), it.value().name());
    }

    QFont currentFont = qApp->font();
    settings.setValue("fontFamily", currentFont.family());
    settings.setValue("fontSize", currentFont.pointSize());

    settings.sync();   // Force write to disk
}


void MainWindow::applyTheme() // Applies the saved themes to the UI
{
    QColor accent       = currentTheme.value("accent",       QColor("#3b82f6"));
    QColor bg           = currentTheme.value("background",   QColor("#f8fafc"));
    QColor panel        = currentTheme.value("panel",        QColor("#ffffff"));
    QColor sectionHeader= currentTheme.value("sectionHeader",QColor("#f1f5f9"));
    QColor text         = currentTheme.value("text",         QColor("#1e2937"));
    QColor altRow       = currentTheme.value("tableAlt",     QColor("#f1f5f9"));
    QColor borderColor  = currentTheme.value("border",       QColor("#64748b"));

    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Base, panel);
    pal.setColor(QPalette::AlternateBase, altRow);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, text.lightness() > 128 ? Qt::black : Qt::white);

    setPalette(pal);
    qApp->setPalette(pal);

    QString textStyle = QString("color: %1;").arg(text.name());

    // Force text color everywhere
    if (ui->pageTitleLabel) ui->pageTitleLabel->setStyleSheet("font-size: 22px; font-weight: bold; " + textStyle);
    if (ui->sidebarTitleLabel) ui->sidebarTitleLabel->setStyleSheet("font-size: 20px; font-weight: bold; " + textStyle);
    if (ui->processSectionLabel) ui->processSectionLabel->setStyleSheet("font-size: 16px; font-weight: bold; " + textStyle);

    QString statTitleStyle = "font-weight: bold; " + textStyle;

    // CPU Stats
    if (ui->cpuCurrentTitleLabel) ui->cpuCurrentTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuAverageTitleLabel) ui->cpuAverageTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuPeakTitleLabel) ui->cpuPeakTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuUserCurrentTitleLabel) ui->cpuUserCurrentTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuUserAverageTitleLabel) ui->cpuUserAverageTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuUserPeakTitleLabel) ui->cpuUserPeakTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuSystemCurrentTitleLabel) ui->cpuSystemCurrentTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuSystemAverageTitleLabel) ui->cpuSystemAverageTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->cpuSystemPeakTitleLabel) ui->cpuSystemPeakTitleLabel->setStyleSheet(statTitleStyle);

    // Memory, Disk, Network titles
    if (ui->memoryTotalTitleLabel) ui->memoryTotalTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->memoryUsedTitleLabel) ui->memoryUsedTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->memoryAvailableTitleLabel) ui->memoryAvailableTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->diskReadStatTitleLabel) ui->diskReadStatTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->diskWriteStatTitleLabel) ui->diskWriteStatTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->networkDownloadStatTitleLabel) ui->networkDownloadStatTitleLabel->setStyleSheet(statTitleStyle);
    if (ui->networkUploadStatTitleLabel) ui->networkUploadStatTitleLabel->setStyleSheet(statTitleStyle);

    // All Buttons with Border Color
    QString buttonStyle = QString(
        "background-color: %1; "
        "color: %2; "
        "font-weight: bold; "
        "padding: 6px 8px; "
        "text-align: left; "
        "border-radius: 6px; "
        "border: 2px solid %3;"
    ).arg(sectionHeader.name())
     .arg(text.name())
     .arg(borderColor.name());

    if (ui->dashboardButton) ui->dashboardButton->setStyleSheet(buttonStyle);
    if (ui->cpuButton) ui->cpuButton->setStyleSheet(buttonStyle);
    if (ui->memoryButton) ui->memoryButton->setStyleSheet(buttonStyle);
    if (ui->diskButton) ui->diskButton->setStyleSheet(buttonStyle);
    if (ui->networkButton) ui->networkButton->setStyleSheet(buttonStyle);

    if (ui->cpuUsageToggleButton) ui->cpuUsageToggleButton->setStyleSheet(buttonStyle);
    if (ui->cpuPerCoreToggleButton) ui->cpuPerCoreToggleButton->setStyleSheet(buttonStyle);
    if (ui->memoryUsageToggleButton) ui->memoryUsageToggleButton->setStyleSheet(buttonStyle);
    if (ui->memoryUsedAvailableToggleButton) ui->memoryUsedAvailableToggleButton->setStyleSheet(buttonStyle);
    if (ui->memoryCacheBuffersToggleButton) ui->memoryCacheBuffersToggleButton->setStyleSheet(buttonStyle);
    if (ui->memorySwapToggleButton) ui->memorySwapToggleButton->setStyleSheet(buttonStyle);
    if (ui->diskUsageToggleButton) ui->diskUsageToggleButton->setStyleSheet(buttonStyle);
    if (ui->diskActivityToggleButton) ui->diskActivityToggleButton->setStyleSheet(buttonStyle);
    if (ui->networkTrafficToggleButton) ui->networkTrafficToggleButton->setStyleSheet(buttonStyle);
    if (ui->networkInterfacesToggleButton) ui->networkInterfacesToggleButton->setStyleSheet(buttonStyle);

    if (ui->sidebarToggleButton) ui->sidebarToggleButton->setStyleSheet(buttonStyle);
    if (ui->settingsButton) ui->settingsButton->setStyleSheet(buttonStyle);

    // Table Header
    QString headerStyle = QString(
        "QHeaderView::section { "
        "background-color: %1; "
        "color: %2; "
        "font-weight: bold; "
        "border: 1px solid %3; "
        "padding: 4px 6px; "
        "}"
    ).arg(sectionHeader.name())
     .arg(text.name())
     .arg(borderColor.name());

    if (ui->processTable && ui->processTable->horizontalHeader())
        ui->processTable->horizontalHeader()->setStyleSheet(headerStyle);

    // Process Table + Graphs
    QString tableStyle = QString(
        "QTableWidget { border: 2px solid %1; gridline-color: %2; selection-background-color: %3; }"
    ).arg(borderColor.name())
     .arg(borderColor.lighter(180).name())
     .arg(accent.name());

    if (ui->processTable && ui->processTable->horizontalHeader())
        ui->processTable->setStyleSheet(tableStyle);

    QString graphBorderStyle = QString(
        "QWidget { border: 2px solid %1; border-radius: 8px; background-color: transparent; }"
    ).arg(borderColor.name());

    if (ui->dashboardCpuGraphContainer) ui->dashboardCpuGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->dashboardMemoryGraphContainer) ui->dashboardMemoryGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->dashboardDiskGraphContainer) ui->dashboardDiskGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->dashboardNetworkGraphContainer) ui->dashboardNetworkGraphContainer->setStyleSheet(graphBorderStyle);

    if (ui->cpuGraphContainer) ui->cpuGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->memoryGraphContainer) ui->memoryGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->memoryUsedGraphContainer) ui->memoryUsedGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->memoryCacheGraphContainer) ui->memoryCacheGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->diskGraphContainer) ui->diskGraphContainer->setStyleSheet(graphBorderStyle);
    if (ui->networkGraphContainer) ui->networkGraphContainer->setStyleSheet(graphBorderStyle);

    // Hover effects
    QString extraStyle = QString(
        "QPushButton:hover { background-color: %1; color: white; }"
        "QProgressBar::chunk { background-color: %1; }"
    ).arg(accent.name());

    if (ui->processTable)
        ui->processTable->setStyleSheet(ui->processTable->styleSheet() + extraStyle);

}

void MainWindow::showSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Customize Theme");
    dialog.resize(620, 680);
    dialog.setWindowModality(Qt::WindowModal);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(12);

    mainLayout->addWidget(new QLabel("<h2>Customize Theme</h2>"));

    // === Color Options ===
    auto addColorButton = [&](const QString& label, const QString& key, QColor defaultColor) {
        QPushButton *btn = new QPushButton(label);
        btn->setMinimumHeight(52);

        QColor current = currentTheme.value(key, defaultColor);
        btn->setStyleSheet(QString(
            "background-color: %1; "
            "color: %2; "
            "font-weight: bold; "
            "border: 2px solid #555; "
            "border-radius: 6px;"
        ).arg(current.name())
         .arg(current.lightness() > 130 ? "black" : "white"));

        connect(btn, &QPushButton::clicked, this, [this, btn, key, label]() {
            QColorDialog dlg(currentTheme.value(key), this);
            dlg.setWindowTitle("Choose " + label);
            dlg.setOption(QColorDialog::DontUseNativeDialog, true);
            dlg.setWindowModality(Qt::WindowModal);
            if (dlg.exec() == QDialog::Accepted) {
                QColor chosen = dlg.currentColor();
                if (chosen.isValid()) {
                    currentTheme[key] = chosen;
                    btn->setStyleSheet(QString(
                        "background-color: %1; "
                        "color: %2; "
                        "font-weight: bold; "
                        "border: 2px solid #555; "
                        "border-radius: 6px;"
                    ).arg(chosen.name())
                     .arg(chosen.lightness() > 130 ? "black" : "white"));
                }
            }
        });

        mainLayout->addWidget(btn);
    };

    addColorButton("Background",       "background",    QColor("#f8fafc"));
    addColorButton("Section Headers",  "sectionHeader", QColor("#f1f5f9"));
    addColorButton("Accent Color",     "accent",        QColor("#3b82f6"));
    addColorButton("Text Color",       "text",          QColor("#1e2937"));
    addColorButton("Table Primary",    "panel",         QColor("#ffffff"));
    addColorButton("Table Alternate",  "tableAlt",      QColor("#f1f5f9"));
    addColorButton("Border Color",     "border",        QColor("#64748b"));

    // === Font Selection ===
    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel("<b>Font Settings</b>"));

    QHBoxLayout *fontLayout = new QHBoxLayout();
    QLabel *fontLabel = new QLabel("Application Font:");
    QFontComboBox *fontCombo = new QFontComboBox();
    fontCombo->setCurrentFont(qApp->font());
    fontCombo->setMinimumHeight(32);
    fontCombo->setFocusPolicy(Qt::StrongFocus);
    fontLayout->addWidget(fontLabel);
    fontLayout->addWidget(fontCombo, 1);
    mainLayout->addLayout(fontLayout);

    // === Presets ===
    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel("<b>Quick Presets</b>"));

    // Define all presets
    struct Preset {
        QString name;
        QString background;
        QString sectionHeader;
        QString accent;
        QString text;
        QString panel;
        QString tableAlt;
        QString border;
    };

    QList<Preset> darkPresets = {
        { "Default",    "#111111", "#222222", "#9d65d6", "#bebebe", "#262626", "#2d2d2d", "#3b3b3b" },
        { "Pure Black", "#000000", "#212121", "#1271ff", "#c4c4c4", "#111111", "#222222", "#6d6d6d" },
        { "Mars",       "#241e1d", "#342a26", "#ff5b24", "#c4bbbb", "#39302c", "#312926", "#1a1616" },
        { "Slate",      "#343434", "#3e3e3e", "#a0b3cf", "#c4c4c4", "#3e3e3e", "#454545", "#787878" },
        { "Royal",      "#000c15", "#011627", "#b07522", "#fffbdc", "#011321", "#01101c", "#011c2d" },
        { "Nebula",     "#11081a", "#1a1125", "#8d003f", "#bfb1a6", "#11081a", "#1a1125", "#1a1125" },
    };

    QList<Preset> lightPresets = {
        { "Default",        "#ffffff", "#f5f5f5", "#9d65d6", "#2b2b2b", "#ffffff", "#f5f5f5", "#c1c1c1" },
        { "Cherry Blossom", "#fff5f5", "#f4b4cc", "#de8bb1", "#8b546b", "#fceaf1", "#f8d7e7", "#d681a2" },
        { "Orange Slices",  "#fffbdc", "#ffd3a5", "#f57b35", "#943100", "#ffd3a5", "#ffaa6e", "#f39b69" },
        { "Matcha",         "#f0f0d8", "#d6eb98", "#809a4d", "#604848", "#dcf19c", "#c4d872", "#87a251" },
        { "Coastal Sands",  "#edddc4", "#e3cda6", "#77b0a4", "#554040", "#edddc4", "#e3cda6", "#b9a587" },
        { "Frost",          "#f3fcff", "#e9f2f5", "#a3b3bf", "#71767e", "#f3fcff", "#e9f2f5", "#b1b9c5" },
    };

    auto applyPreset = [&](const Preset &p, QDialog *dlg) {
        currentTheme["background"]    = QColor(p.background);
        currentTheme["sectionHeader"] = QColor(p.sectionHeader);
        currentTheme["accent"]        = QColor(p.accent);
        currentTheme["text"]          = QColor(p.text);
        currentTheme["panel"]         = QColor(p.panel);
        currentTheme["tableAlt"]      = QColor(p.tableAlt);
        currentTheme["border"]        = QColor(p.border);
        dlg->accept();
        QTimer::singleShot(0, this, [this]() {
                    updateSidebarHighlight();
                });
    };

    // Helper to build a preset dropdown button
    auto makePresetMenu = [&](const QString &label,
                               const QList<Preset> &presets,
                               const QString &btnBg,
                               const QString &btnText,
                               const QString &btnBorder) -> QPushButton*
    {
        QPushButton *btn = new QPushButton(label + "  ▾");
        btn->setMinimumHeight(44);
        btn->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  font-weight: bold;"
            "  border: 2px solid %3;"
            "  border-radius: 6px;"
            "}"
            "QPushButton:hover { opacity: 0.85; }"
        ).arg(btnBg).arg(btnText).arg(btnBorder));

        connect(btn, &QPushButton::clicked, this, [this, btn, presets, &dialog, applyPreset]() {
            QMenu menu(btn);
            for (const Preset &p : presets)
            {
                QPixmap swatch(14, 14);
                swatch.fill(QColor(p.accent));
                QAction *action = menu.addAction(QIcon(swatch), p.name);
                connect(action, &QAction::triggered, this, [this, p, &dialog, applyPreset]() {
                    applyPreset(p, &dialog);
                });
            }
            menu.exec(btn->mapToGlobal(btn->rect().topLeft()));
        });

        return btn;
    };

    QHBoxLayout *presetLayout = new QHBoxLayout();

    QPushButton *darkBtn = makePresetMenu(
        "Dark Themes", darkPresets,
        "#2a2a2a", "#e0e0e0", "#555555"
    );
    QPushButton *lightBtn = makePresetMenu(
        "Light Themes", lightPresets,
        "#f0f0f0", "#1e2937", "#d0d0d0"
    );

    presetLayout->addWidget(darkBtn);
    presetLayout->addWidget(lightBtn);
    mainLayout->addLayout(presetLayout);

    // === OK / Cancel ===
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel
    );
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QFont newFont = fontCombo->currentFont();

        // Apply font globally
        qApp->setFont(newFont);

        // Updates all widgets
        QWidgetList widgets = QApplication::allWidgets();
        for (QWidget *w : widgets) {
            w->setFont(newFont);
            w->update();
        }

        applyTheme();
        saveThemeSettings();

        // Extra safety
        updateSidebarHighlight();
    }
}

void MainWindow::exportToCSV(const std::string& filename)
{
    const RecordingSession* session = selectedRecordingSession();
    if (!session)
    {
        QMessageBox::warning(this, "Export History",
                             "No recording session is currently selected.");
        return;
    }

    std::ofstream out(filename);
    if (!out)
    {
        QMessageBox::critical(this, "Export History",
                              "Failed to open the export file for writing.");
        return;
    }

    out << "timestamp,pid,startTimeTicks,name,state,cpuPercent,cpuTimeSec,threads,"
           "rssMB,vszMB,sharedMB,readBps,writeBps,totalRead,totalWrite,"
           "priority,nice\n";

    for (const auto& [time, rows] : session->history)
    {
        auto t = std::chrono::system_clock::to_time_t(time);

        for (const auto& row : rows)
        {
            out << t << ","
                << row.pid << ","
                << row.startTimeTicks << ","
                << row.name << ","
                << row.state << ","
                << row.cpuPercent << ","
                << row.cpuTimeSec << ","
                << row.threads << ","
                << row.rssMB << ","
                << row.vszMB << ","
                << row.sharedMB << ","
                << row.readBytesPerSec << ","
                << row.writeBytesPerSec << ","
                << row.totalReadBytes << ","
                << row.totalWriteBytes << ","
                << row.priority << ","
                << row.niceValue
                << "\n";
        }
    }
}

void MainWindow::exportSelectedHistoryToCSV(const std::string& filename)
{
    int selectedRow = ui->historyTable->currentRow();
    if (selectedRow < 0)
    {
        QMessageBox::information(this, "Export History",
                                 "Please select a process session in the History table first.");
        return;
    }

    const RecordingSession* session = selectedRecordingSession();
    if (!session)
    {
        QMessageBox::warning(this, "Export History",
                             "No recording session is currently selected.");
        return;
    }

    QTableWidgetItem *pidItem = ui->historyTable->item(selectedRow, 1);
    if (!pidItem)
    {
        QMessageBox::warning(this, "Export History",
                             "Could not determine the selected PID.");
        return;
    }

    int selectedPid = pidItem->text().toInt();
    qlonglong selectedStartTimeTicks = pidItem->data(Qt::UserRole).toLongLong();

    if (selectedPid <= 0 || selectedStartTimeTicks <= 0)
    {
        QMessageBox::warning(this, "Export History",
                             "Selected process session is invalid.");
        return;
    }

    std::ofstream out(filename);
    if (!out)
    {
        QMessageBox::critical(this, "Export History",
                              "Failed to open the export file for writing.");
        return;
    }

    out << "timestamp,pid,startTimeTicks,name,state,cpuPercent,cpuTimeSec,threads,"
           "rssMB,vszMB,sharedMB,readBps,writeBps,totalRead,totalWrite,"
           "priority,nice\n";

    int writtenRows = 0;

    for (const auto &entry : session->history)
    {
        std::time_t t = std::chrono::system_clock::to_time_t(entry.first);

        for (const auto &row : entry.second)
        {
            if (row.pid != selectedPid || row.startTimeTicks != selectedStartTimeTicks)
                continue;

            out << t << ","
                << row.pid << ","
                << row.startTimeTicks << ","
                << row.name << ","
                << row.state << ","
                << row.cpuPercent << ","
                << row.cpuTimeSec << ","
                << row.threads << ","
                << row.rssMB << ","
                << row.vszMB << ","
                << row.sharedMB << ","
                << row.readBytesPerSec << ","
                << row.writeBytesPerSec << ","
                << row.totalReadBytes << ","
                << row.totalWriteBytes << ","
                << row.priority << ","
                << row.niceValue
                << "\n";

            ++writtenRows;
        }
    }

    QMessageBox::information(
        this,
        "Export History",
        QString("Exported %1 rows for PID %2 to:\n%3")
            .arg(writtenRows)
            .arg(selectedPid)
            .arg(QString::fromStdString(filename))
    );
}

void MainWindow::refreshRecordingSessionComboBox()
{
    ui->historySessionComboBox->blockSignals(true);
    ui->historySessionComboBox->clear();

    for (int i = 0; i < static_cast<int>(recordingSessions.size()); ++i)
    {
        const RecordingSession &session = recordingSessions[i];

        std::time_t startT = std::chrono::system_clock::to_time_t(session.startedAt);
        QString startText = QString::fromStdString(std::string(std::ctime(&startT))).trimmed();

        QString label = QString("Session %1").arg(i + 1);
        ui->historySessionComboBox->addItem(label);
        ui->historySessionComboBox->setItemData(
            i,
            QString("Started: %1").arg(startText),
            Qt::ToolTipRole
        );
    }

    if (currentRecordingSessionIndex >= 0 &&
        currentRecordingSessionIndex < ui->historySessionComboBox->count())
    {
        ui->historySessionComboBox->setCurrentIndex(currentRecordingSessionIndex);
    }

    ui->historySessionComboBox->blockSignals(false);
}

void MainWindow::onRecordingSessionChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(recordingSessions.size()))
        return;

    currentRecordingSessionIndex = index;
    populateHistoryTable();
}

void MainWindow::loadRecordingSettings()
{
    QSettings settings("ProcessParty", "Recording");
    recordOnStartup = settings.value("recordOnStartup", false).toBool();

    if (ui->historyRecordOnStartupCheckBox)
        ui->historyRecordOnStartupCheckBox->setChecked(recordOnStartup);
}

void MainWindow::saveRecordingSettings()
{
    QSettings settings("ProcessParty", "Recording");
    settings.setValue("recordOnStartup", recordOnStartup);
}

void MainWindow::onRecordOnStartupToggled(bool checked)
{
    recordOnStartup = checked;
    saveRecordingSettings();
}

void MainWindow::onRecordForClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Record For...");
    dialog.setMinimumSize(320, 120);
    dialog.resize(400, 140);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel("Select recording duration:", &dialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QHBoxLayout *timeLayout = new QHBoxLayout();

    QSpinBox *minutesSpinBox = new QSpinBox(&dialog);
    minutesSpinBox->setRange(0, 999);
    minutesSpinBox->setSuffix(" min");

    QSpinBox *secondsSpinBox = new QSpinBox(&dialog);
    secondsSpinBox->setRange(0, 59);
    secondsSpinBox->setSuffix(" sec");

    timeLayout->addStretch();
    timeLayout->addWidget(minutesSpinBox);
    timeLayout->addWidget(secondsSpinBox);
    timeLayout->addStretch();

    mainLayout->addLayout(timeLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
    );
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    int totalSeconds = minutesSpinBox->value() * 60 + secondsSpinBox->value();
    if (totalSeconds <= 0)
    {
        QMessageBox::warning(this, "Timed Recording",
                             "Please choose a duration greater than zero.");
        return;
    }

    startRecordingSession();
    timedRecordingActive = true;
    timedRecordingEndTime = std::chrono::steady_clock::now() + std::chrono::seconds(totalSeconds);

    populateHistoryTable();

    QMessageBox::information(
        this,
        "Timed Recording",
        QString("Started a new recording session for %1 minute(s) and %2 second(s).")
            .arg(minutesSpinBox->value())
            .arg(secondsSpinBox->value())
    );
}

void MainWindow::checkTimedRecordingStop()
{
    if (!isRecording || !timedRecordingActive)
        return;

    if (std::chrono::steady_clock::now() >= timedRecordingEndTime)
    {
        timedRecordingActive = false;
        stopRecordingSession();
        populateHistoryTable();

        QMessageBox::information(this, "Timed Recording",
                                 "Timed recording session completed.");
    }
}

void MainWindow::onHistoryHeaderClicked(int logicalIndex)
{
    HistoryColumn clickedColumn;

    switch (logicalIndex)
    {
    case 0: clickedColumn = HistoryColumn::Name; break;
    case 1: clickedColumn = HistoryColumn::PID; break;
    case 2: clickedColumn = HistoryColumn::Samples; break;
    case 3: clickedColumn = HistoryColumn::Status; break;
    case 4: clickedColumn = HistoryColumn::AvgCpu; break;
    case 5: clickedColumn = HistoryColumn::PeakCpu; break;
    case 6: clickedColumn = HistoryColumn::AvgRssMB; break;
    case 7: clickedColumn = HistoryColumn::FirstSeen; break;
    case 8: clickedColumn = HistoryColumn::LastSeen; break;
    default: return;
    }

    if (currentHistorySortColumn != clickedColumn)
    {
        currentHistorySortColumn = clickedColumn;
        currentHistorySortState = SortState::Descending;
    }
    else
    {
        switch (currentHistorySortState)
        {
        case SortState::Normal:
            currentHistorySortState = SortState::Descending;
            break;
        case SortState::Descending:
            currentHistorySortState = SortState::Ascending;
            break;
        case SortState::Ascending:
            currentHistorySortState = SortState::Normal;
            break;
        }
    }
    updateHistoryHeaderLabels();
    populateHistoryTable();
}

void MainWindow::updateHistoryHeaderLabels()
{
    QStringList headers;
    headers << "Name"
            << "PID"
            << "Samples"
            << "Status"
            << "Avg CPU %"
            << "Peak CPU %"
            << "Avg RSS MB"
            << "First Seen"
            << "Last Seen";

    if (currentHistorySortState != SortState::Normal)
    {
        int sortColumnIndex = -1;

        switch (currentHistorySortColumn)
        {
        case HistoryColumn::Name:      sortColumnIndex = 0; break;
        case HistoryColumn::PID:       sortColumnIndex = 1; break;
        case HistoryColumn::Samples:   sortColumnIndex = 2; break;
        case HistoryColumn::Status:    sortColumnIndex = 3; break;
        case HistoryColumn::AvgCpu:    sortColumnIndex = 4; break;
        case HistoryColumn::PeakCpu:   sortColumnIndex = 5; break;
        case HistoryColumn::AvgRssMB:  sortColumnIndex = 6; break;
        case HistoryColumn::FirstSeen: sortColumnIndex = 7; break;
        case HistoryColumn::LastSeen:  sortColumnIndex = 8; break;
        }

        if (sortColumnIndex >= 0)
        {
            headers[sortColumnIndex] += (currentHistorySortState == SortState::Descending)
                ? " ▼"
                : " ▲";
        }
    }

    ui->historyTable->setHorizontalHeaderLabels(headers);
}

void MainWindow::applySavedFont()
{
    QFont font(m_savedFontFamily, m_savedFontSize);
    qApp->setFont(font);

    // Apply to all existing widgets
    QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *w : widgets) {
        w->setFont(font);
        w->update();
    }
}
