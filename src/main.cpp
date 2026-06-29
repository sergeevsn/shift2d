#include <QApplication>
#include "main_window.hpp"
#include "app_theme.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Shift2D");
    app.setApplicationDisplayName("Shift2D");
    app.setOrganizationName("shift2d");

    AppTheme::apply(app);

    MainWindow window;
    window.show();

    return app.exec();
}
