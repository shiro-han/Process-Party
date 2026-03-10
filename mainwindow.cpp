#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "systemstats.h"
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      timer(new QTimer(this)),
      workerThread(new QThread(this)),
      statsWorker(new StatsWorker)
{
    ui->setupUi(this);
    setupProcessTable();

    // Move worker onto background thread
    statsWorker->moveToThread(workerThread);

    // Timer fires on main thread → triggers worker on background thread
    connect(timer, &QTimer::timeout, this, &MainWindow::requestStats);

    // Wire worker result back to main thread
    connect(this, &MainWindow::requestStats, statsWorker, &StatsWorker::run);
    connect(statsWorker, &StatsWorker::result, this, &MainWindow::onStatsResult);
    connect(workerThread, &QThread::finished, statsWorker, &QObject::deleteLater);

    workerThread->start();

    // Default interval: 1000ms (1 second)
    timer->start(1000);

    // Kick off first sample immediately without waiting for first tick
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
}

void MainWindow::populateProcessTable(const std::vector<ProcessRow> &rows)
{
    int newRowCount = static_cast<int>(rows.size());

    // Only reset row count if it actually changed
    if (ui->processTable->rowCount() != newRowCount)
        ui->processTable->setRowCount(newRowCount);

    for (int row = 0; row < newRowCount; ++row)
    {
        const ProcessRow &p = rows[row];

        auto setText = [&](int col, const QString &text) {
            QTableWidgetItem *item = ui->processTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem(text);
                ui->processTable->setItem(row, col, item);
            } else if (item->text() != text) {
                item->setText(text);
            }
        };

        setText(0, QString::number(p.pid));
        setText(1, QString::fromStdString(p.name));
        setText(2, QString(p.state));
        setText(3, QString::number(p.cpuPercent, 'f', 1));
        setText(4, QString::number(p.rssMB, 'f', 1));
        setText(5, QString::number(p.threads));
        setText(6, QString::number(p.readBytesPerSec));
        setText(7, QString::number(p.writeBytesPerSec));
        setText(8, QString::number(p.cpuTimeSec, 'f', 1));
    }
}

void MainWindow::onStatsResult(SystemData data, std::vector<ProcessRow> rows)
{
    // Update the 4 summary boxes
    ui->cpuSummaryValue->setText(QString("%1%").arg(data.cpuPercent));
    ui->memorySummaryValue->setText(QString("%1%").arg(data.memoryPercent));
    ui->diskSummaryValue->setText(QString("%1%").arg(data.diskPercent));
    ui->networkSummaryValue->setText(QString::fromStdString(data.networkDownloadText));

    // Update process table
    populateProcessTable(rows);

    // Update status bar
    ui->statusLabel->setText(
        QString("CPU %1% | Memory %2% | Disk %3% | Network %4 | Processes %5")
            .arg(data.cpuPercent)
            .arg(data.memoryPercent)
            .arg(data.diskPercent)
            .arg(QString::fromStdString(data.networkDownloadText))
            .arg(rows.size())
    );
}
