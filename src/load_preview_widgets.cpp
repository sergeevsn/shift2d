#include "load_preview_widgets.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

namespace {

struct Bounds {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double uniform_span = 1.0;
    double plot_min_x = 0.0;
    double plot_min_y = 0.0;
};

Bounds computeBounds(const std::vector<double>& xs, const std::vector<double>& ys)
{
    Bounds b;
    if (xs.empty() || ys.empty() || xs.size() != ys.size())
        return b;

    b.min_x = b.max_x = xs[0];
    b.min_y = b.max_y = ys[0];
    for (size_t i = 0; i < xs.size(); ++i) {
        b.min_x = std::min(b.min_x, xs[i]);
        b.max_x = std::max(b.max_x, xs[i]);
        b.min_y = std::min(b.min_y, ys[i]);
        b.max_y = std::max(b.max_y, ys[i]);
    }

    const double span_x = b.max_x - b.min_x;
    const double span_y = b.max_y - b.min_y;
    const double pad = std::max(1.0, std::max(span_x, span_y) * 0.05);
    b.uniform_span = std::max(span_x + 2.0 * pad, span_y + 2.0 * pad);
    const double center_x = (b.min_x + b.max_x) * 0.5;
    const double center_y = (b.min_y + b.max_y) * 0.5;
    b.plot_min_x = center_x - b.uniform_span * 0.5;
    b.plot_min_y = center_y - b.uniform_span * 0.5;
    return b;
}

Bounds mergedBounds(const std::vector<double>& xs1,
                    const std::vector<double>& ys1,
                    const std::vector<double>& xs2,
                    const std::vector<double>& ys2)
{
    std::vector<double> xs = xs1;
    std::vector<double> ys = ys1;
    xs.insert(xs.end(), xs2.begin(), xs2.end());
    ys.insert(ys.end(), ys2.begin(), ys2.end());
    return computeBounds(xs, ys);
}

QRect squarePlotRect(const QRect& avail)
{
    const int side = std::min(avail.width(), avail.height());
    return QRect(avail.left() + (avail.width() - side) / 2,
                 avail.top() + (avail.height() - side) / 2,
                 side,
                 side);
}

QPoint mapPoint(double x,
                double y,
                const QRect& plot,
                const Bounds& bounds)
{
    const double rel_x = (x - bounds.plot_min_x) / bounds.uniform_span;
    const double rel_y = (y - bounds.plot_min_y) / bounds.uniform_span;
    return QPoint(plot.left() + static_cast<int>(rel_x * plot.width()),
                  plot.bottom() - static_cast<int>(rel_y * plot.height()));
}

}  // namespace

ProfileOverlayMapWidget::ProfileOverlayMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

bool ProfileOverlayMapWidget::hasHeightForWidth() const
{
    return true;
}

int ProfileOverlayMapWidget::heightForWidth(int width) const
{
    return width;
}

void ProfileOverlayMapWidget::setProfile(const std::vector<double>& xs, const std::vector<double>& ys)
{
    profile_xs_ = xs;
    profile_ys_ = ys;
    update();
}

void ProfileOverlayMapWidget::setOverlay(const std::vector<double>& xs, const std::vector<double>& ys)
{
    overlay_xs_ = xs;
    overlay_ys_ = ys;
    update();
}

void ProfileOverlayMapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(245, 245, 245));

    if (profile_xs_.empty() && overlay_xs_.empty()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, tr("No coordinate preview"));
        return;
    }

    const Bounds bounds = mergedBounds(profile_xs_, profile_ys_, overlay_xs_, overlay_ys_);
  if (bounds.uniform_span <= 0.0)
        return;

    const QRect plot = squarePlotRect(rect().adjusted(12, 8, -12, -8));
    p.setPen(QColor(200, 200, 200));
    p.drawRect(plot);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(170, 170, 170));
    for (size_t i = 0; i < profile_xs_.size(); ++i) {
        const QPoint pt = mapPoint(profile_xs_[i], profile_ys_[i], plot, bounds);
        p.drawEllipse(pt, 1, 1);
    }

    p.setBrush(QColor(210, 60, 60));
    for (size_t i = 0; i < overlay_xs_.size(); ++i) {
        const QPoint pt = mapPoint(overlay_xs_[i], overlay_ys_[i], plot, bounds);
        p.drawEllipse(pt, 3, 3);
    }
}

TraceSeriesPlotWidget::TraceSeriesPlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TraceSeriesPlotWidget::setSeries(const std::vector<double>& traces,
                                      const std::vector<double>& values,
                                      const QString& y_label)
{
    traces_ = traces;
    values_ = values;
    y_label_ = y_label;
    update();
}

void TraceSeriesPlotWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(250, 250, 250));

    if (traces_.empty() || values_.empty() || traces_.size() != values_.size()) {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, tr("No series preview"));
        return;
    }

    const QRect plot = rect().adjusted(48, 14, -12, -24);
    p.setPen(QColor(210, 210, 210));
    p.drawRect(plot);

    double min_x = traces_.front();
    double max_x = traces_.front();
    double min_y = values_.front();
    double max_y = values_.front();
    for (size_t i = 0; i < traces_.size(); ++i) {
        min_x = std::min(min_x, traces_[i]);
        max_x = std::max(max_x, traces_[i]);
        min_y = std::min(min_y, values_[i]);
        max_y = std::max(max_y, values_[i]);
    }

    if (max_x - min_x < 1e-9)
        max_x = min_x + 1.0;
    if (max_y - min_y < 1e-9) {
        min_y -= 0.5;
        max_y += 0.5;
    }

    const double pad_y = (max_y - min_y) * 0.1;
    min_y -= pad_y;
    max_y += pad_y;

    QFont label_font = p.font();
    label_font.setPointSize(8);
    p.setFont(label_font);
    p.setPen(Qt::black);

    p.save();
    const int label_cx = 20;
    const int label_cy = plot.top() + plot.height() / 2;
    p.translate(label_cx, label_cy);
    p.rotate(-90.0);
    p.drawText(QRect(-60, -8, 120, 16), Qt::AlignCenter, y_label_);
    p.restore();

    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    for (size_t i = 0; i < traces_.size(); ++i) {
        const double rel_x = (traces_[i] - min_x) / (max_x - min_x);
        const double rel_y = (values_[i] - min_y) / (max_y - min_y);
        const QPointF pt(plot.left() + rel_x * plot.width(),
                         plot.bottom() - rel_y * plot.height());
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }

    QPen line_pen(QColor(30, 120, 220), 1.5);
    line_pen.setCosmetic(true);
    p.setPen(line_pen);
    p.drawPath(path);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 120, 220));
    for (size_t i = 0; i < traces_.size(); ++i) {
        const double rel_x = (traces_[i] - min_x) / (max_x - min_x);
        const double rel_y = (values_[i] - min_y) / (max_y - min_y);
        const QPointF pt(plot.left() + rel_x * plot.width(),
                         plot.bottom() - rel_y * plot.height());
        p.drawEllipse(pt, 2.5, 2.5);
    }

    p.setPen(Qt::gray);
    p.drawText(QRect(plot.left(), plot.bottom() + 4, plot.width(), 16),
               Qt::AlignCenter,
               tr("Trace"));
}

bool readCsvDataRows(const QString& path,
                     QStringList& headers,
                     std::vector<std::vector<QString>>& rows)
{
    headers.clear();
    rows.clear();

    std::ifstream file(path.toStdString());
    if (!file.is_open())
        return false;

    std::string line;
    bool first_line = true;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string cell;
        std::vector<QString> row;
        while (std::getline(iss, cell, ',')) {
            row.push_back(QString::fromStdString(cell).trimmed());
        }
        if (row.empty())
            continue;

        if (first_line) {
            bool all_numeric = true;
            for (const auto& val : row) {
                bool ok = false;
                val.toDouble(&ok);
                if (!ok) {
                    all_numeric = false;
                    break;
                }
            }
            first_line = false;
            if (all_numeric) {
                rows.push_back(row);
                for (int i = 0; i < row.size(); ++i)
                    headers.append(QStringLiteral("Column %1").arg(i));
            } else {
                for (const auto& val : row)
                    headers.append(val);
            }
            continue;
        }

        rows.push_back(row);
    }

    return !rows.empty();
}
