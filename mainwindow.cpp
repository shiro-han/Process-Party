#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "systemstats.h"

#include <QHeaderView>
#include <QTableWidgetItem>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      timer(new QTimer(this))
{
    ui->setupUi(this);

    setupProcessTable();

    connect(timer, &QTimer::timeout,
            this, &MainWindow::updateStats);

    timer->start(1000);

    updateStats();
}

MainWindow::~MainWindow()
{
    delete ui;
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
    ui->processTable->setRowCount(static_cast<int>(rows.size()));

    for (int row = 0; row < static_cast<int>(rows.size()); ++row)
    {
        const ProcessRow &p = rows[row];

        ui->processTable->setItem(row, 0, new QTableWidgetItem(QString::number(p.pid)));
        ui->processTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(p.name)));
        ui->processTable->setItem(row, 2, new QTableWidgetItem(QString(p.state)));
        ui->processTable->setItem(row, 3, new QTableWidgetItem(QString::number(p.cpuPercent, 'f', 1)));
        ui->processTable->setItem(row, 4, new QTableWidgetItem(QString::number(p.rssMB, 'f', 1)));
        ui->processTable->setItem(row, 5, new QTableWidgetItem(QString::number(p.threads)));
        ui->processTable->setItem(row, 6, new QTableWidgetItem(QString::number(p.readBytesPerSec)));
        ui->processTable->setItem(row, 7, new QTableWidgetItem(QString::number(p.writeBytesPerSec)));
        ui->processTable->setItem(row, 8, new QTableWidgetItem(QString::number(p.cpuTimeSec, 'f', 1)));
    }
}

void MainWindow::updateStats()
{
    SystemData data = SystemStats::readSystemData();

    ui->cpuSummaryValue->setText(QString("%1%").arg(data.cpuPercent));
    ui->memorySummaryValue->setText(QString("%1%").arg(data.memoryPercent));
    ui->diskSummaryValue->setText(QString("%1%").arg(data.diskPercent));
    ui->networkSummaryValue->setText(QString::fromStdString(data.networkDownloadText));

    std::vector<ProcessRow> rows = processTable.readProcesses();
    populateProcessTable(rows);

    ui->statusLabel->setText(
        QString("CPU %1%% | Memory %2%% | Disk %3%% | Network %4 | Processes %5")
            .arg(data.cpuPercent)
            .arg(data.memoryPercent)
            .arg(data.diskPercent)
            .arg(QString::fromStdString(data.networkDownloadText))
            .arg(rows.size())
    );
}
