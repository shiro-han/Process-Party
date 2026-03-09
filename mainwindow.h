#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "processtable.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateStats();

private:
    void setupProcessTable();
    void populateProcessTable(const std::vector<ProcessRow> &rows);

    Ui::MainWindow *ui;
    QTimer *timer;
    ProcessTable processTable;
};

#endif