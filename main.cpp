#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QIcon appIcon(":/capyy_2.png");
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);

        if (QWindow *window = app.topLevelWindows().value(0)) {
            window->setIcon(appIcon);
        }
    } else {
        qWarning() << "Warning: Could not load application icon ':/capyy_2.png'";
    }

    MainWindow window;
    window.show();

    return app.exec();
}
