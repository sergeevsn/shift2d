#ifndef APP_THEME_HPP
#define APP_THEME_HPP

#include <QColor>

class QApplication;

namespace AppTheme {

void apply(QApplication& app);

QColor canvasBackground();
QColor plotSurface();
QColor plotBorder();
QColor axisText();
QColor axisLine();
QColor gridLine();
QColor accent();
QColor accentMuted();
QColor horizonLine();
QColor staticsCurve();
QColor emptyStateText();
QColor emptyStateHint();

} // namespace AppTheme

#endif // APP_THEME_HPP
