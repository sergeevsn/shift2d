#include <QApplication>
#include "main_window.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SEG-Y Statics Applier");
    app.setOrganizationName("shift2d");

    MainWindow window;
    window.show();

    return app.exec();
}
