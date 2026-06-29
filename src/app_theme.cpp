#include "app_theme.hpp"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QWidget>
#include <QLabel>
#include <QGroupBox>
#include <QAbstractButton>
#include <QComboBox>
#include <QAbstractSpinBox>

namespace {

constexpr QColor kCanvasBackground{0x12, 0x14, 0x1a};
constexpr QColor kPlotSurface{0x18, 0x1b, 0x24};
constexpr QColor kPlotBorder{0x3a, 0x3f, 0x4d};
constexpr QColor kAxisText{0xa8, 0xb0, 0xc0};
constexpr QColor kAxisLine{0x5c, 0x63, 0x72};
constexpr QColor kGridLine{0x28, 0x2d, 0x3a};
constexpr QColor kAccent{0x3b, 0x9e, 0xff};
constexpr QColor kAccentMuted{0x2a, 0x6f, 0xb8};
constexpr QColor kHorizonLine{0xff, 0xc1, 0x07};
constexpr QColor kStaticsCurve{0x4d, 0xcc, 0xff};
constexpr QColor kEmptyStateText{0xd0, 0xd6, 0xe0};
constexpr QColor kEmptyStateHint{0x7a, 0x82, 0x94};

} // namespace

namespace AppTheme {

void apply(QApplication& app)
{
    QFont ui_font = app.font();
    ui_font.setFamily(QStringLiteral("Segoe UI"));
    ui_font.setPointSize(10);
    app.setFont(ui_font);

    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x1e, 0x21, 0x28));
    palette.setColor(QPalette::WindowText, QColor(0xe8, 0xea, 0xed));
    palette.setColor(QPalette::Base, QColor(0x14, 0x16, 0x1c));
    palette.setColor(QPalette::AlternateBase, QColor(0x24, 0x28, 0x32));
    palette.setColor(QPalette::ToolTipBase, QColor(0x2a, 0x2e, 0x38));
    palette.setColor(QPalette::ToolTipText, QColor(0xe8, 0xea, 0xed));
    palette.setColor(QPalette::Text, QColor(0xe8, 0xea, 0xed));
    palette.setColor(QPalette::Button, QColor(0x2a, 0x2e, 0x38));
    palette.setColor(QPalette::ButtonText, QColor(0xe8, 0xea, 0xed));
    palette.setColor(QPalette::BrightText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Highlight, kAccent);
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6b, 0x72, 0x80));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6b, 0x72, 0x80));
    palette.setColor(QPalette::Link, kAccent);
    app.setPalette(palette);

    QFile style_file(QStringLiteral(":/styles/app.qss"));
    if (style_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(style_file.readAll()));
    }
}

void applyLoadDialogTypography(QWidget* root)
{
    if (!root)
        return;

    QFont dialog_font = root->font();
    dialog_font.setPointSize(10);
    root->setFont(dialog_font);

    const auto set_widget_font = [&](QWidget* widget, int point_size) {
        QFont font = dialog_font;
        font.setPointSize(point_size);
        widget->setFont(font);
    };

    for (QGroupBox* box : root->findChildren<QGroupBox*>())
        set_widget_font(box, 10);
    for (QAbstractButton* button : root->findChildren<QAbstractButton*>())
        set_widget_font(button, 10);
    for (QComboBox* combo : root->findChildren<QComboBox*>())
        set_widget_font(combo, 10);
    for (QAbstractSpinBox* spin : root->findChildren<QAbstractSpinBox*>())
        set_widget_font(spin, 10);
    for (QLabel* label : root->findChildren<QLabel*>())
        set_widget_font(label, 10);
}

QColor canvasBackground() { return kCanvasBackground; }
QColor plotSurface() { return kPlotSurface; }
QColor plotBorder() { return kPlotBorder; }
QColor axisText() { return kAxisText; }
QColor axisLine() { return kAxisLine; }
QColor gridLine() { return kGridLine; }
QColor accent() { return kAccent; }
QColor accentMuted() { return kAccentMuted; }
QColor horizonLine() { return kHorizonLine; }
QColor staticsCurve() { return kStaticsCurve; }
QColor emptyStateText() { return kEmptyStateText; }
QColor emptyStateHint() { return kEmptyStateHint; }

} // namespace AppTheme
