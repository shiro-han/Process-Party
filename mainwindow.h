#pragma once

#include <QMainWindow>
#include <QThread>
#include <QObject>

#include "cpu_pct.h"
#include "mem_pct.h"
#include "disk_pct.h"
#include "net_rate.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ── Worker classes ─────────────────────────────────────────────────────────
// Each worker lives on its own QThread and emits a result signal when done.

class CpuWorker : public QObject {
    Q_OBJECT
public slots:
    void run() { emit result(sample_cpu(1.0)); }
signals:
    void result(CpuResult r);
};

class MemWorker : public QObject {
    Q_OBJECT
public slots:
    void run() { emit result(sample_mem()); }
signals:
    void result(MemResult r);
};

class DiskWorker : public QObject {
    Q_OBJECT
public slots:
    void run() { emit result(sample_disk(1.0)); }
signals:
    void result(DiskResult r);
};

class NetWorker : public QObject {
    Q_OBJECT
public slots:
    void run() { emit result(sample_net(1.0)); }
signals:
    void result(NetResult r);
};

// ── MainWindow ─────────────────────────────────────────────────────────────

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void requestCpu();
    void requestMem();
    void requestDisk();
    void requestNet();

private slots:
    void onCpuResult(CpuResult r);
    void onMemResult(MemResult r);
    void onDiskResult(DiskResult r);
    void onNetResult(NetResult r);

private:
    Ui::MainWindow *ui;

    QThread    *cpuThread;
    QThread    *memThread;
    QThread    *diskThread;
    QThread    *netThread;

    CpuWorker  *cpuWorker;
    MemWorker  *memWorker;
    DiskWorker *diskWorker;
    NetWorker  *netWorker;
};
