#include "seismic_view.hpp"
#include "app_theme.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <limits>
#include <limits>

SeismicView::SeismicView(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    updateColorLut();
}

void SeismicView::setData(const std::vector<Trace>& traces, float dt)
{
    traces_ = traces;
    dt_ = dt;

    if (!traces_.empty()) {
        n_samples_total_ = static_cast<int>(traces_[0].data.size());
    } else {
        n_samples_total_ = 0;
    }

    // По умолчанию показываем всё
    resetZoom();
    updateCachedGeometry();
    update();
}

void SeismicView::setTraceWindow(int first_trace, int count)
{
    first_trace_ = std::max(0, first_trace);
    trace_count_ = std::max(1, count);
    trace_count_ = std::min(trace_count_, static_cast<int>(traces_.size()) - first_trace_);

    updateCachedGeometry();
    update();
}

void SeismicView::setTimeWindow(int first_sample, int sample_count)
{
    first_sample_ = std::max(0, first_sample);
    sample_count_ = std::max(1, sample_count);
    sample_count_ = std::min(sample_count_, n_samples_total_ - first_sample_);

    updateCachedGeometry();
    update();
}

void SeismicView::setColorMap(ColorMap cmap)
{
    color_map_ = cmap;
    updateColorLut();
    update();
}

void SeismicView::setClipValue(float clip)
{
    clip_value_ = std::max(0.001f, clip);
    update();
}

void SeismicView::setInteractionMode(InteractionMode mode)
{
    interaction_mode_ = mode;
    is_zooming_ = false;
    is_panning_ = false;
    unsetCursor();
}

void SeismicView::resetZoom()
{
    first_trace_ = 0;
    trace_count_ = static_cast<int>(traces_.size());
    first_sample_ = 0;
    sample_count_ = n_samples_total_;

    updateCachedGeometry();
    update();
}

void SeismicView::setHorizon(const std::vector<std::pair<double, double>>& horizon_points)
{
    horizon_points_ = horizon_points;
    has_horizon_ = !horizon_points_.empty();
    update();
}

void SeismicView::clearHorizon()
{
    horizon_points_.clear();
    has_horizon_ = false;
    update();
}

void SeismicView::setStaticsCurve(const std::vector<float>& values)
{
    statics_curve_ = values;
    update();
}

void SeismicView::clearStaticsCurve()
{
    statics_curve_.clear();
    update();
}

int SeismicView::traceAxisBandHeight() const
{
    QFont axis_font = font();
    axis_font.setPointSize(9);
    QFont title_font = axis_font;
    title_font.setBold(true);

    const QFontMetrics tick_fm(axis_font);
    const QFontMetrics title_fm(title_font);

    return kTraceAxisLineOffset + kTraceTickLength + kTraceTickLabelGap
         + tick_fm.height() + kTraceAxisTitleGap + title_fm.height()
         + kTraceAxisBottomPadding;
}

SeismicView::PlotRect SeismicView::plotRect() const
{
    const int w = width();
    const int h = height();
    PlotRect plot;
    plot.left = kLeftMargin;
    plot.right = std::max(plot.left + 1, w - kRightMargin);
    plot.top = kTopMargin + staticsRegionHeight() + kStaticsDataGap;
    plot.bottom = std::max(plot.top + 1, h - traceAxisBandHeight());
    return plot;
}

SeismicView::PlotRect SeismicView::fullPlotRect() const
{
    const int w = width();
    const int h = height();
    PlotRect plot;
    plot.left = kLeftMargin;
    plot.right = std::max(plot.left + 1, w - kRightMargin);
    plot.top = kTopMargin;
    plot.bottom = std::max(plot.top + 1, h - traceAxisBandHeight());
    return plot;
}

int SeismicView::staticsRegionHeight() const
{
    int total = std::max(1, height() - kTopMargin - traceAxisBandHeight());
    return std::max(1, total / 5);
}

float SeismicView::traceSpacingPx() const
{
    const PlotRect plot = plotRect();
    if (trace_count_ <= 1)
        return 0.f;
    return static_cast<float>(plot.width()) / static_cast<float>(trace_count_ - 1);
}

float SeismicView::traceX(int trace_idx, const PlotRect& plot) const
{
    if (trace_count_ == 0)
        return static_cast<float>(plot.left);
    const float dx = traceSpacingPx();
    return static_cast<float>(plot.left) + dx * static_cast<float>(trace_idx);
}

int SeismicView::traceIndexAtX(int x_px, const PlotRect& plot) const
{
    if (trace_count_ == 0)
        return 0;
    const float dx = traceSpacingPx();
    if (dx <= 0.f)
        return 0;
    const float rel = (static_cast<float>(x_px) - static_cast<float>(plot.left)) / dx;
    return std::clamp(static_cast<int>(std::lround(rel)), 0, trace_count_ - 1);
}

double SeismicView::timeMsFromY(int y_px, const PlotRect& plot) const
{
    if (dt_ <= 0.f)
        return 0.0;
    const float local_y = static_cast<float>(y_px - plot.top);
    const int sample_idx = first_sample_ + static_cast<int>(std::lround(local_y * samples_per_pixel_y_));
    return static_cast<double>(sample_idx) * static_cast<double>(dt_) * 1000.0;
}

int SeismicView::sampleFromY(int y_px, const PlotRect& plot) const
{
    const float local_y = static_cast<float>(y_px - plot.top);
    return first_sample_ + static_cast<int>(std::lround(local_y * samples_per_pixel_y_));
}

float SeismicView::getAmplitudeAt(int trace_idx, int sample_idx) const
{
    if (trace_idx < 0 || trace_idx >= static_cast<int>(traces_.size()))
        return 0.0f;
    const auto& trace = traces_[static_cast<size_t>(trace_idx)];
    if (sample_idx < 0 || sample_idx >= static_cast<int>(trace.data.size()))
        return 0.0f;
    return trace.data[static_cast<size_t>(sample_idx)];
}

void SeismicView::updateCachedGeometry()
{
    int h = height();
    const int total_plot_h = std::max(1, h - kTopMargin - traceAxisBandHeight());
    const int statics_h = staticsRegionHeight();
    const int plot_h = std::max(1, total_plot_h - statics_h - kStaticsDataGap);
    if (plot_h <= 0 || sample_count_ <= 0) {
        samples_per_pixel_y_ = 1.0f;
        return;
    }

    samples_per_pixel_y_ = static_cast<float>(sample_count_) / static_cast<float>(plot_h);
    if (samples_per_pixel_y_ <= 0.f)
        samples_per_pixel_y_ = 1.0f;
}

void SeismicView::updateColorLut()
{
    constexpr int kLutSize = 256;
    color_lut_.resize(kLutSize);

    QString scheme_name = ColorSchemes::getSchemeName(color_map_);

    for (int i = 0; i < kLutSize; ++i) {
        float norm = (kLutSize == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(kLutSize - 1);
        QColor c = ColorSchemes::getColor(norm, scheme_name);
        color_lut_[static_cast<size_t>(i)] = c.rgb();
    }
}

void SeismicView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(rect(), AppTheme::canvasBackground());

    if (traces_.empty() || dt_ <= 0.f || trace_count_ <= 0) {
        drawEmptyState(p);
        return;
    }

    PlotRect data_plot = plotRect();
    PlotRect statics_plot = fullPlotRect();
    const int statics_height = staticsRegionHeight();
    statics_plot.bottom = statics_plot.top + statics_height;
    data_plot.top = statics_plot.bottom + kStaticsDataGap;

    drawPlotFrame(p, statics_plot);
    drawPlotFrame(p, data_plot);

    drawStaticsCurve(p, statics_plot);
    drawSeismicImage(p, data_plot);
    drawAxes(p, data_plot);
    drawHorizon(p, data_plot);
    drawZoomRect(p);
}

void SeismicView::drawEmptyState(QPainter& p)
{
    const int cy = height() / 2;

    QFont title_font = p.font();
    title_font.setPointSize(14);
    title_font.setWeight(QFont::DemiBold);
    p.setFont(title_font);
    p.setPen(AppTheme::emptyStateText());

    p.drawText(QRect(0, cy - 48, width(), 28), Qt::AlignCenter,
               tr("No seismic section loaded"));

    QFont hint_font = p.font();
    hint_font.setPointSize(10);
    hint_font.setWeight(QFont::Normal);
    p.setFont(hint_font);
    p.setPen(AppTheme::emptyStateHint());

    p.drawText(QRect(0, cy - 16, width(), 22), Qt::AlignCenter,
               tr("Open a SEG-Y file from the sidebar, File menu, or drag it here"));
    p.drawText(QRect(0, cy + 8, width(), 22), Qt::AlignCenter,
               tr("Drag to zoom  ·  Scroll to zoom in/out  ·  Right-click to reset"));
}

void SeismicView::drawPlotFrame(QPainter& p, const PlotRect& plot)
{
    p.setPen(QPen(AppTheme::plotBorder(), 1));
    p.setBrush(AppTheme::plotSurface());
    p.drawRect(plot.left, plot.top, plot.width(), plot.height());
}

void SeismicView::drawSeismicImage(QPainter& p, const PlotRect& plot)
{
    const int img_w = plot.width();
    const int img_h = plot.height();

    if (img_w <= 0 || img_h <= 0)
        return;

    QImage img(img_w, img_h, QImage::Format_RGB32);
    img.fill(AppTheme::plotSurface().rgb());

    const float samples_per_px = samples_per_pixel_y_;

    // Диапазон индексов трасс для столбца пикселя
    auto traceRange = [this, img_w](int ix) -> std::pair<int, int> {
        if (trace_count_ <= 0)
            return {0, -1};
        if (trace_count_ == 1 || img_w <= 1)
            return {0, 0};
        const int t0 = static_cast<int>(std::floor(static_cast<float>(ix) * static_cast<float>(trace_count_ - 1)
                                                      / static_cast<float>(img_w - 1)));
        const int t1 = static_cast<int>(std::floor(static_cast<float>(ix + 1) * static_cast<float>(trace_count_ - 1)
                                                      / static_cast<float>(img_w - 1)));
        return {std::clamp(t0, 0, trace_count_ - 1), std::clamp(std::max(t1, t0), 0, trace_count_ - 1)};
    };

    // Диапазон сэмплов для строки пикселя
    auto sampleRange = [this, img_h](int iy) -> std::pair<int, int> {
        if (sample_count_ <= 0)
            return {0, 0};
        const float y0f = static_cast<float>(iy) * samples_per_pixel_y_;
        const float y1f = static_cast<float>(iy + 1) * samples_per_pixel_y_;
        int s0 = first_sample_ + static_cast<int>(std::floor(y0f));
        int s1 = first_sample_ + static_cast<int>(std::ceil(y1f));
        s0 = std::clamp(s0, 0, n_samples_total_ - 1);
        s1 = std::clamp(std::max(s1, s0 + 1), s0 + 1, n_samples_total_);
        return {s0, s1};
    };

    // Пиковая амплитуда в бине
    auto peakAmplitude = [this](int t0, int t1, int s0, int s1) -> float {
        float peak = 0.0f;
        bool has_data = false;
        for (int ti = t0; ti <= t1; ++ti) {
            const int global_trace = first_trace_ + ti;
            if (global_trace < 0 || global_trace >= static_cast<int>(traces_.size()))
                continue;
            const auto& tr = traces_[static_cast<size_t>(global_trace)];
            if (tr.data.empty())
                continue;
            const int last_s = std::min(s1, static_cast<int>(tr.data.size()));
            for (int si = s0; si < last_s; ++si) {
                if (si < 0 || si >= static_cast<int>(tr.data.size()))
                    continue;
                float v = tr.data[static_cast<size_t>(si)];
                if (!has_data || std::abs(v) > std::abs(peak)) {
                    peak = v;
                    has_data = true;
                }
            }
        }
        return has_data ? peak : 0.0f;
    };

    for (int iy = 0; iy < img_h; ++iy) {
        const auto [s0, s1] = sampleRange(iy);

        for (int ix = 0; ix < img_w; ++ix) {
            const auto [t0, t1] = traceRange(ix);
            if (t1 < t0 || s1 <= s0)
                continue;

            float peak = peakAmplitude(t0, t1, s0, s1);

            // Нормализация с клиппингом
            float norm = 0.0f;
            if (clip_value_ > 0.0f) {
                norm = (peak / clip_value_ + 1.0f) * 0.5f;  // -clip -> 0, +clip -> 1
                norm = std::clamp(norm, 0.0f, 1.0f);
            }

            // Получаем цвет из LUT
            if (!color_lut_.empty()) {
                int idx = static_cast<int>(norm * static_cast<float>(color_lut_.size() - 1));
                idx = std::clamp(idx, 0, static_cast<int>(color_lut_.size()) - 1);
                img.setPixel(ix, iy, color_lut_[static_cast<size_t>(idx)]);
            }
        }
    }

    p.drawImage(plot.left, plot.top, img);
}

void SeismicView::drawStaticsCurve(QPainter& p, const PlotRect& statics_rect)
{
    if (statics_curve_.empty() || trace_count_ <= 0)
        return;

    float min_static = std::numeric_limits<float>::max();
    float max_static = std::numeric_limits<float>::lowest();
    bool has_visible_static = false;
    for (int i = 0; i < trace_count_; ++i) {
        const int global_trace = first_trace_ + i;
        if (global_trace >= 0 && global_trace < static_cast<int>(statics_curve_.size())) {
            const float value = statics_curve_[static_cast<size_t>(global_trace)];
            min_static = std::min(min_static, value);
            max_static = std::max(max_static, value);
            has_visible_static = true;
        }
    }
    if (!has_visible_static)
        return;

    QPainterPath path;
    QPen line_pen(AppTheme::staticsCurve(), 2.0);
    line_pen.setCosmetic(true);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(line_pen);

    float axis_min = min_static;
    float axis_max = max_static;
    if (std::abs(axis_max - axis_min) < 1e-9f) {
        const float padding = std::max(0.001f, std::abs(axis_min) * 0.1f);
        axis_min -= padding;
        axis_max += padding;
    }

    auto yForStatic = [&statics_rect, axis_min, axis_max](float value) -> int {
        const float rel = (value - axis_min) / (axis_max - axis_min);
        return statics_rect.bottom - static_cast<int>(std::clamp(rel, 0.0f, 1.0f)
                                                      * static_cast<float>(statics_rect.height()));
    };

    bool path_started = false;
    for (int i = 0; i < trace_count_; ++i) {
        const int global_trace = first_trace_ + i;
        if (global_trace < 0 || global_trace >= static_cast<int>(statics_curve_.size()))
            continue;

        float relx = (trace_count_ > 1) ? static_cast<float>(i) / static_cast<float>(trace_count_ - 1) : 0.0f;
        int x = statics_rect.left + static_cast<int>(relx * static_cast<float>(statics_rect.width() - 1));
        int y = yForStatic(statics_curve_[static_cast<size_t>(global_trace)]);
        if (!path_started) {
            path.moveTo(x, y);
            path_started = true;
        } else {
            path.lineTo(x, y);
        }
    }

    QPen base_pen(AppTheme::gridLine(), 1.0);
    base_pen.setCosmetic(true);
    p.setPen(base_pen);
    if (axis_min <= 0.0f && axis_max >= 0.0f) {
        const int zero_y = yForStatic(0.0f);
        p.drawLine(statics_rect.left, zero_y, statics_rect.right, zero_y);
    }

    p.setPen(line_pen);
    p.drawPath(path);

    QPen axis_pen(AppTheme::axisLine(), 1.0);
    axis_pen.setCosmetic(true);
    p.setPen(axis_pen);

    const int axis_x = kLeftMargin - 12;
    const int tick_label_left = kTimeAxisTitleColumn + 4;
    const int tick_label_right = axis_x - kTimeTickLabelGap;
    const int min_y = yForStatic(axis_min);
    const int max_y = yForStatic(axis_max);
    p.drawLine(axis_x, statics_rect.top, axis_x, statics_rect.bottom);
    p.drawLine(axis_x - 4, min_y, axis_x, min_y);
    p.drawLine(axis_x - 4, max_y, axis_x, max_y);

    QFont axis_font = p.font();
    axis_font.setPointSize(9);
    p.setFont(axis_font);
    p.setPen(AppTheme::axisText());

    const double min_ms = static_cast<double>(axis_min) * 1000.0;
    const double max_ms = static_cast<double>(axis_max) * 1000.0;
    const QFontMetrics fm(axis_font);
    const int label_h = fm.height();
    const auto draw_tick_label = [&](int y, double ms) {
        const int label_y = std::clamp(y, statics_rect.top + label_h / 2,
                                       statics_rect.bottom - label_h / 2);
        QRect label(tick_label_left, label_y - label_h / 2,
                    tick_label_right - tick_label_left, label_h);
        const QString text = (std::abs(ms - std::round(ms)) < 0.05)
            ? QString::number(ms, 'f', 0)
            : QString::number(ms, 'f', 1);
        p.drawText(label, Qt::AlignRight | Qt::AlignVCenter, text);
    };
    draw_tick_label(min_y, min_ms);
    draw_tick_label(max_y, max_ms);
}

void SeismicView::drawAxes(QPainter& p, const PlotRect& plot)
{
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen axisPen(AppTheme::axisLine(), 1.0);
    p.setPen(axisPen);

    QFont axisFont = p.font();
    axisFont.setPointSize(9);
    p.setFont(axisFont);
    p.setPen(AppTheme::axisText());

    // Ось времени (Y) слева
    const int axis_left_x = kLeftMargin - 12;
    const int tick_label_left = kTimeAxisTitleColumn + 4;
    const int tick_label_right = axis_left_x - kTimeTickLabelGap;
    p.setPen(AppTheme::axisLine());
    p.drawLine(axis_left_x, plot.top, axis_left_x, plot.bottom);

    // Рисуем тики времени
    if (dt_ > 0.0f && sample_count_ > 0) {
        const double start_ms = first_sample_ * dt_ * 1000.0;
        const double end_ms = (first_sample_ + sample_count_) * dt_ * 1000.0;
        const double range_ms = end_ms - start_ms;

        // Выбираем шаг тиков
        auto chooseStep = [](double range) -> double {
            if (range <= 50.0) return 10.0;
            if (range <= 100.0) return 20.0;
            if (range <= 250.0) return 50.0;
            if (range <= 500.0) return 100.0;
            if (range <= 1000.0) return 200.0;
            if (range <= 2500.0) return 500.0;
            if (range <= 5000.0) return 1000.0;
            return 2000.0;
        };

        double step = chooseStep(range_ms);
        double first_tick = std::ceil(start_ms / step) * step;
        const QFontMetrics fm(axisFont);
        const int label_h = fm.height();
        const int label_margin = label_h / 2 + 1;

        for (double tick = first_tick; tick <= end_ms; tick += step) {
            double rel = (tick - start_ms) / range_ms;
            int y = plot.top + static_cast<int>(rel * plot.height());

            if (y < plot.top + label_margin || y > plot.bottom - label_margin)
                continue;

            p.setPen(AppTheme::axisLine());
            p.drawLine(axis_left_x - 4, y, axis_left_x, y);

            QString label = QString::number(tick, 'f', (step < 1.0) ? 1 : 0);
            QRect textRect(tick_label_left, y - label_h / 2, tick_label_right - tick_label_left, label_h);
            p.setPen(AppTheme::axisText());
            p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }

    // Подпись оси Y — слева от тиков, по центру по вертикали
    {
        QFont titleFont = axisFont;
        titleFont.setBold(true);
        titleFont.setPointSize(9);
        p.setFont(titleFont);
        p.setPen(AppTheme::axisText());

        p.save();
        p.translate(kTimeAxisTitleColumn / 2, plot.top + plot.height() / 2);
        p.rotate(-90.0);
        p.drawText(QRect(-50, -10, 100, 20), Qt::AlignCenter, tr("Time, ms"));
        p.restore();

        p.setFont(axisFont);
    }

    // Ось трасс (X) снизу
    const int axis_bottom_y = plot.bottom + kTraceAxisLineOffset;
    p.setPen(AppTheme::axisLine());
    p.drawLine(plot.left, axis_bottom_y, plot.right, axis_bottom_y);

    const QFontMetrics trace_tick_fm(axisFont);
    const int trace_tick_label_h = trace_tick_fm.height();
    const int trace_tick_label_top = axis_bottom_y + kTraceTickLength + kTraceTickLabelGap;

    // Рисуем тики трасс
    if (trace_count_ > 0) {
        int max_trace_labels = std::max(1, plot.width() / 70);
        int trace_step = std::max(1, trace_count_ / max_trace_labels);

        for (int i = 0; i < trace_count_; i += trace_step) {
            float t = (trace_count_ > 1)
                ? static_cast<float>(i) / static_cast<float>(trace_count_ - 1)
                : 0.f;
            int x = plot.left + static_cast<int>(t * static_cast<float>(plot.width() - 1));

            p.setPen(AppTheme::axisLine());
            p.drawLine(x, axis_bottom_y, x, axis_bottom_y + kTraceTickLength);

            int global_idx = first_trace_ + i;
            QString label = QString::number(global_idx);
            QRect textRect(x - 25, trace_tick_label_top, 50, trace_tick_label_h);
            p.setPen(AppTheme::axisText());
            p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, label);
        }
    }

    // Подпись оси X
    {
        QFont titleFont = axisFont;
        titleFont.setBold(true);
        titleFont.setPointSize(9);
        p.setFont(titleFont);
        p.setPen(AppTheme::axisText());

        const QFontMetrics title_fm(titleFont);
        const int title_h = title_fm.height();
        const int title_top = std::min(
            trace_tick_label_top + trace_tick_label_h + kTraceAxisTitleGap,
            height() - title_h - kTraceAxisBottomPadding);

        p.drawText(QRect(plot.left, title_top, plot.width(), title_h),
                   Qt::AlignCenter,
                   tr("Trace index"));
    }
}

void SeismicView::drawHorizon(QPainter& p, const PlotRect& plot)
{
    if (!has_horizon_ || horizon_points_.empty())
        return;

    std::vector<std::pair<double, double>> visible_points;
    visible_points.reserve(horizon_points_.size());

    for (const auto& [trace, time_ms] : horizon_points_) {
        if (trace < first_trace_ || trace >= first_trace_ + trace_count_)
            continue;

        const int sample = static_cast<int>(time_ms / (dt_ * 1000.0));
        if (sample < first_sample_ || sample >= first_sample_ + sample_count_)
            continue;

        visible_points.emplace_back(trace, time_ms);
    }

    if (visible_points.empty())
        return;

    std::sort(visible_points.begin(), visible_points.end(),
              [](const std::pair<double, double>& a, const std::pair<double, double>& b) {
                  return a.first < b.first;
              });

    std::vector<QPointF> plot_points;
    plot_points.reserve(visible_points.size());

    for (const auto& [trace, time_ms] : visible_points) {
        const float rel_x = (trace_count_ > 1)
            ? static_cast<float>(trace - first_trace_) / static_cast<float>(trace_count_ - 1)
            : 0.0f;
        const int x = plot.left + static_cast<int>(rel_x * static_cast<float>(plot.width()));

        const int sample = static_cast<int>(time_ms / (dt_ * 1000.0));
        const float rel_y = static_cast<float>(sample - first_sample_) / static_cast<float>(sample_count_);
        const int y = plot.top + static_cast<int>(rel_y * static_cast<float>(plot.height()));

        plot_points.emplace_back(static_cast<qreal>(x), static_cast<qreal>(y));
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath horizon_path;
    bool started = false;
    for (const QPointF& pt : plot_points) {
        if (!started) {
            horizon_path.moveTo(pt);
            started = true;
        } else {
            horizon_path.lineTo(pt);
        }
    }

    QPen horizon_pen(AppTheme::horizonLine(), 2.0);
    horizon_pen.setCosmetic(true);
    p.setPen(horizon_pen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(horizon_path);

    p.setPen(Qt::NoPen);
    p.setBrush(AppTheme::horizonLine());
    constexpr qreal point_radius = 3.0;
    for (const QPointF& pt : plot_points)
        p.drawEllipse(pt, point_radius, point_radius);
}

void SeismicView::drawZoomRect(QPainter& p)
{
    if (!is_zooming_)
        return;

    QRect sel = QRect(zoom_start_, zoom_end_).normalized();
    const PlotRect plot = plotRect();
    QRect plot_rect(plot.left, plot.top, plot.width(), plot.height());
    sel = sel.intersected(plot_rect);
    if (sel.width() > 2 && sel.height() > 2) {
        QPen pen(AppTheme::accent(), 1, Qt::DashLine);
        QColor fill = AppTheme::accent();
        fill.setAlpha(50);
        p.setPen(pen);
        p.setBrush(fill);
        p.drawRect(sel);
    }
}

void SeismicView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCachedGeometry();
}

void SeismicView::mousePressEvent(QMouseEvent* event)
{
    if (interaction_mode_ == InteractionMode::Zoom) {
        if (event->button() == Qt::LeftButton) {
            is_zooming_ = true;
            zoom_start_ = event->pos();
            zoom_end_ = event->pos();
            update();
        } else if (event->button() == Qt::RightButton) {
            emit resetZoomRequested();
        }
    } else if (interaction_mode_ == InteractionMode::Pan) {
        if (event->button() == Qt::LeftButton) {
            is_panning_ = true;
            pan_last_pos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    QWidget::mousePressEvent(event);
}

void SeismicView::mouseMoveEvent(QMouseEvent* event)
{
    const PlotRect plot = plotRect();

    // Обработка зума
    if (interaction_mode_ == InteractionMode::Zoom && is_zooming_) {
        zoom_end_ = event->pos();
        update();
    }

    // Обработка панорамирования
    if (interaction_mode_ == InteractionMode::Pan && is_panning_) {
        QPoint cur = event->pos();
        QPoint delta = cur - pan_last_pos_;
        pan_last_pos_ = cur;

        if (plot.width() > 0 && plot.height() > 0) {
            int delta_traces = -static_cast<int>(static_cast<float>(delta.x()) / static_cast<float>(plot.width())
                                                 * static_cast<float>(trace_count_));
            int delta_samples = -static_cast<int>(static_cast<float>(delta.y()) / static_cast<float>(plot.height())
                                                   * static_cast<float>(sample_count_));

            if (delta_traces != 0 || delta_samples != 0) {
                emit panRequested(delta_traces, delta_samples);
            }
        }
    }

    // Информация под курсором
    if (!traces_.empty() && dt_ > 0.f) {
        QPoint pos = event->pos();
        if (pos.x() >= plot.left && pos.x() < plot.right &&
            pos.y() >= plot.top && pos.y() < plot.bottom) {

            int trace_idx = traceIndexAtX(pos.x(), plot);
            int sample_idx = sampleFromY(pos.y(), plot);

            if (trace_idx >= 0 && trace_idx < trace_count_ &&
                sample_idx >= 0 && sample_idx < n_samples_total_) {

                int global_trace = first_trace_ + trace_idx;
                float amp = getAmplitudeAt(global_trace, sample_idx);
                double time_ms = sample_idx * dt_ * 1000.0;

                emit hoverInfoChanged(global_trace, time_ms, amp, true);
            } else {
                emit hoverInfoChanged(-1, 0.0, 0.0f, false);
            }
        } else {
            emit hoverInfoChanged(-1, 0.0, 0.0f, false);
        }
    }

    QWidget::mouseMoveEvent(event);
}

void SeismicView::mouseReleaseEvent(QMouseEvent* event)
{
    if (interaction_mode_ == InteractionMode::Zoom && is_zooming_ && event->button() == Qt::LeftButton) {
        is_zooming_ = false;
        QRect sel = QRect(zoom_start_, zoom_end_).normalized();

        if (sel.width() > 4 && sel.height() > 4) {
            const PlotRect plot = plotRect();

            // Вертикальный зум (время)
            float y0 = static_cast<float>(std::clamp(sel.top(), plot.top, plot.bottom));
            float y1 = static_cast<float>(std::clamp(sel.bottom(), plot.top, plot.bottom));
            if (y1 <= y0) std::swap(y0, y1);

            float ty0 = (y0 - static_cast<float>(plot.top)) / static_cast<float>(plot.height());
            float ty1 = (y1 - static_cast<float>(plot.top)) / static_cast<float>(plot.height());

            int first_sample = first_sample_ + static_cast<int>(ty0 * static_cast<float>(sample_count_));
            int last_sample = first_sample_ + static_cast<int>(ty1 * static_cast<float>(sample_count_));

            first_sample = std::clamp(first_sample, 0, n_samples_total_ - 1);
            last_sample = std::clamp(last_sample, first_sample + 1, n_samples_total_);

            // Горизонтальный зум (трассы)
            float x0 = static_cast<float>(std::clamp(sel.left(), plot.left, plot.right));
            float x1 = static_cast<float>(std::clamp(sel.right(), plot.left, plot.right));
            if (x1 <= x0) std::swap(x0, x1);

            float tx0 = (x0 - static_cast<float>(plot.left)) / static_cast<float>(plot.width());
            float tx1 = (x1 - static_cast<float>(plot.left)) / static_cast<float>(plot.width());

            int first_trace = first_trace_ + static_cast<int>(tx0 * static_cast<float>(trace_count_));
            int last_trace = first_trace_ + static_cast<int>(tx1 * static_cast<float>(trace_count_));

            first_trace = std::clamp(first_trace, 0, static_cast<int>(traces_.size()) - 1);
            last_trace = std::clamp(last_trace, first_trace + 1, static_cast<int>(traces_.size()));

            emit traceZoomRequested(first_trace, last_trace - first_trace);
            emit timeZoomRequested(first_sample, last_sample - first_sample);
        }
        update();
    } else if (interaction_mode_ == InteractionMode::Pan && is_panning_ && event->button() == Qt::LeftButton) {
        is_panning_ = false;
        unsetCursor();
    }

    QWidget::mouseReleaseEvent(event);
}

void SeismicView::wheelEvent(QWheelEvent* event)
{
    const PlotRect plot = plotRect();
    QPoint pos = event->position().toPoint();

    // Проверяем, что курсор над областью данных
    if (pos.x() < plot.left || pos.x() >= plot.right ||
        pos.y() < plot.top || pos.y() >= plot.bottom) {
        QWidget::wheelEvent(event);
        return;
    }

    // Определяем направление прокрутки
    int delta = event->angleDelta().y();
    if (delta == 0) {
        delta = event->angleDelta().x();
    }

    if (delta > 0) {
        // Зум in - уменьшаем окно
        int new_trace_count = std::max(1, trace_count_ / 2);
        int new_sample_count = std::max(1, sample_count_ / 2);
        new_trace_count = std::min(new_trace_count, static_cast<int>(traces_.size()));
        new_sample_count = std::min(new_sample_count, n_samples_total_);

        // Центрируем зум на позиции курсора
        float rel_x = static_cast<float>(pos.x() - plot.left) / plot.width();
        float rel_y = static_cast<float>(pos.y() - plot.top) / plot.height();

        int center_trace = first_trace_ + static_cast<int>(rel_x * trace_count_);
        int center_sample = first_sample_ + static_cast<int>(rel_y * sample_count_);

        int new_first_trace = center_trace - new_trace_count / 2;
        int new_first_sample = center_sample - new_sample_count / 2;

        new_first_trace = std::clamp(new_first_trace, 0, std::max(0, static_cast<int>(traces_.size()) - new_trace_count));
        new_first_sample = std::clamp(new_first_sample, 0, std::max(0, n_samples_total_ - new_sample_count));

        emit traceZoomRequested(new_first_trace, new_trace_count);
        emit timeZoomRequested(new_first_sample, new_sample_count);
    } else {
        // Зум out - увеличиваем окно
        int new_trace_count = std::min(static_cast<int>(traces_.size()), trace_count_ * 2);
        int new_sample_count = std::min(n_samples_total_, sample_count_ * 2);

        int new_first_trace = first_trace_ - (new_trace_count - trace_count_) / 2;
        int new_first_sample = first_sample_ - (new_sample_count - sample_count_) / 2;

        new_first_trace = std::clamp(new_first_trace, 0, std::max(0, static_cast<int>(traces_.size()) - new_trace_count));
        new_first_sample = std::clamp(new_first_sample, 0, std::max(0, n_samples_total_ - new_sample_count));

        emit traceZoomRequested(new_first_trace, new_trace_count);
        emit timeZoomRequested(new_first_sample, new_sample_count);
    }

    event->accept();
}
