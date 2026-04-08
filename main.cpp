#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QIcon appIcon(":/capyy_2.png");
    app.setWindowIcon(appIcon);

    MainWindow window;
    window.show();

    return app.exec();
}
