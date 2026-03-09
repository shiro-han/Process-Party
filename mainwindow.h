#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QThread>
#include <QObject>
#include "processtable.h"
#include "systemstats.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// ── Worker: runs SystemStats + ProcessTable on a background thread ────────────
class StatsWorker : public QObject {
    Q_OBJECT
public slots:
    void run() {
        SystemData data = SystemStats::readSystemData();
        std::vector<ProcessRow> rows = processTable.readProcesses();
        emit result(data, rows);
    }
signals:
    void result(SystemData data, std::vector<ProcessRow> rows);
private:
    ProcessTable processTable;
};

// ── MainWindow ────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Call this from the settings panel to change the refresh interval
    void setRefreshInterval(int milliseconds);

signals:
    void requestStats();

private slots:
    void onStatsResult(SystemData data, std::vector<ProcessRow> rows);

private:
    void setupProcessTable();
    void populateProcessTable(const std::vector<ProcessRow> &rows);

    Ui::MainWindow *ui;
    QTimer     *timer;
    QThread    *workerThread;
    StatsWorker *statsWorker;
};

#endif
