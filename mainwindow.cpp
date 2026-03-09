#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cpuThread(new QThread(this))
    , memThread(new QThread(this))
    , diskThread(new QThread(this))
    , netThread(new QThread(this))
    , cpuWorker(new CpuWorker)
    , memWorker(new MemWorker)
    , diskWorker(new DiskWorker)
    , netWorker(new NetWorker)
{
    ui->setupUi(this);

    // Move each worker onto its own thread
    cpuWorker->moveToThread(cpuThread);
    memWorker->moveToThread(memThread);
    diskWorker->moveToThread(diskThread);
    netWorker->moveToThread(netThread);

    // Wire request signals → worker run slots
    connect(this, &MainWindow::requestCpu,  cpuWorker,  &CpuWorker::run);
    connect(this, &MainWindow::requestMem,  memWorker,  &MemWorker::run);
    connect(this, &MainWindow::requestDisk, diskWorker, &DiskWorker::run);
    connect(this, &MainWindow::requestNet,  netWorker,  &NetWorker::run);

    // Wire worker result signals → main window slots (queued across threads)
    connect(cpuWorker,  &CpuWorker::result,  this, &MainWindow::onCpuResult);
    connect(memWorker,  &MemWorker::result,  this, &MainWindow::onMemResult);
    connect(diskWorker, &DiskWorker::result, this, &MainWindow::onDiskResult);
    connect(netWorker,  &NetWorker::result,  this, &MainWindow::onNetResult);

    // Clean up workers when threads finish
    connect(cpuThread,  &QThread::finished, cpuWorker,  &QObject::deleteLater);
    connect(memThread,  &QThread::finished, memWorker,  &QObject::deleteLater);
    connect(diskThread, &QThread::finished, diskWorker, &QObject::deleteLater);
    connect(netThread,  &QThread::finished, netWorker,  &QObject::deleteLater);

    // Start all threads
    cpuThread->start();
    memThread->start();
    diskThread->start();
    netThread->start();

    // Kick off the first sample on all 4 threads simultaneously
    emit requestCpu();
    emit requestMem();
    emit requestDisk();
    emit requestNet();
}

MainWindow::~MainWindow() {
    cpuThread->quit();  cpuThread->wait();
    memThread->quit();  memThread->wait();
    diskThread->quit(); diskThread->wait();
    netThread->quit();  netThread->wait();
    delete ui;
}

// When a result comes back, update the label and immediately request the next sample
void MainWindow::onCpuResult(CpuResult r) {
    ui->cpuLabel->setText(r.ok ? QString::number(r.pct, 'f', 1) + "%" : "Error");
    emit requestCpu();  // loop: request next sample right away
}

void MainWindow::onMemResult(MemResult r) {
    ui->memLabel->setText(r.ok ? QString::number(r.pct, 'f', 1) + "%" : "Error");
    emit requestMem();
}

void MainWindow::onDiskResult(DiskResult r) {
    if (r.ok)
        ui->diskLabel->setText(QString::number(r.pct, 'f', 1) + "% (" + QString::fromStdString(r.dev) + ")");
    else
        ui->diskLabel->setText("Error");
    emit requestDisk();
}

void MainWindow::onNetResult(NetResult r) {
    if (r.ok)
        ui->netLabel->setText(
            QString::fromStdString(r.iface) + "\n" +
            "RX: " + QString::number(r.rx_Bps, 'f', 0) + " B/s\n" +
            "TX: " + QString::number(r.tx_Bps, 'f', 0) + " B/s"
        );
    else
        ui->netLabel->setText("Error");
    emit requestNet();
}


