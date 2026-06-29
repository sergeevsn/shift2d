#include "data_info_dialog.hpp"

#include "load_preview_widgets.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {

struct ValueStats {
    double min_v = 0.0;
    double max_v = 0.0;
    double mean_v = 0.0;
    bool valid = false;
};

ValueStats computeValueStats(const std::vector<double>& values)
{
    ValueStats stats;
    if (values.empty())
        return stats;

    stats.valid = true;
    stats.min_v = stats.max_v = values.front();
    double sum = 0.0;
    for (double v : values) {
        stats.min_v = std::min(stats.min_v, v);
        stats.max_v = std::max(stats.max_v, v);
        sum += v;
    }
    stats.mean_v = sum / static_cast<double>(values.size());
    return stats;
}

QString formatRange(double min_v, double max_v, int decimals = 2)
{
    return QStringLiteral("%1 .. %2")
        .arg(QString::number(min_v, 'f', decimals))
        .arg(QString::number(max_v, 'f', decimals));
}

void addFormRow(QFormLayout* layout, const QString& label, const QString& value)
{
    auto* value_label = new QLabel(value);
    value_label->setWordWrap(true);
    value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addRow(label, value_label);
}

QGroupBox* makeStatsGroup(QWidget* parent, QFormLayout** out_layout)
{
    auto* group = new QGroupBox(QObject::tr("Statistics"), parent);
    auto* layout = new QFormLayout(group);
    layout->setHorizontalSpacing(16);
    layout->setVerticalSpacing(8);
    *out_layout = layout;
    return group;
}

QDialog* createBaseDialog(QWidget* parent, const QString& title)
{
    auto* dialog = new QDialog(parent);
    dialog->setWindowTitle(title);
    dialog->setMinimumSize(560, 520);
    dialog->resize(640, 680);
    return dialog;
}

void addCloseButton(QDialog* dialog, QVBoxLayout* main_layout)
{
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    main_layout->addWidget(buttons);
}

bool staticsUseXYMode(const std::vector<StaticPoint>& points)
{
    for (const auto& sp : points) {
        if (sp.x != 0.0 || sp.y != 0.0)
            return true;
    }
    return false;
}

int findNearestTrace(double x,
                     double y,
                     const std::vector<double>& profile_x,
                     const std::vector<double>& profile_y)
{
    if (profile_x.empty() || profile_x.size() != profile_y.size())
        return -1;

    int nearest = -1;
    double min_dist_sq = std::numeric_limits<double>::max();
    for (size_t i = 0; i < profile_x.size(); ++i) {
        const double dx = profile_x[i] - x;
        const double dy = profile_y[i] - y;
        const double dist_sq = dx * dx + dy * dy;
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

void sortSeriesByTrace(std::vector<double>& traces, std::vector<double>& values)
{
    if (traces.size() != values.size() || traces.empty())
        return;

    std::vector<size_t> order(traces.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return traces[a] < traces[b];
    });

    std::vector<double> sorted_traces;
    std::vector<double> sorted_values;
    sorted_traces.reserve(order.size());
    sorted_values.reserve(order.size());
    for (size_t idx : order) {
        sorted_traces.push_back(traces[idx]);
        sorted_values.push_back(values[idx]);
    }
    traces = std::move(sorted_traces);
    values = std::move(sorted_values);
}

void buildStaticsPreview(const std::vector<StaticPoint>& points,
                         const std::vector<double>& profile_x,
                         const std::vector<double>& profile_y,
                         int n_traces,
                         std::vector<double>& overlay_x,
                         std::vector<double>& overlay_y,
                         std::vector<double>& series_traces,
                         std::vector<double>& series_values)
{
    overlay_x.clear();
    overlay_y.clear();
    series_traces.clear();
    series_values.clear();

    const bool xy_mode = staticsUseXYMode(points);
    for (const auto& sp : points) {
        const double value_ms = sp.value * 1000.0;

        if (xy_mode) {
            overlay_x.push_back(sp.x);
            overlay_y.push_back(sp.y);
            const int trace_idx = findNearestTrace(sp.x, sp.y, profile_x, profile_y);
            if (trace_idx < 0)
                continue;
            series_traces.push_back(static_cast<double>(trace_idx));
            series_values.push_back(value_ms);
        } else {
            const int trace_idx = static_cast<int>(std::lround(sp.trace_num));
            if (trace_idx < 0 || trace_idx >= n_traces)
                continue;
            if (trace_idx < static_cast<int>(profile_x.size())) {
                overlay_x.push_back(profile_x[static_cast<size_t>(trace_idx)]);
                overlay_y.push_back(profile_y[static_cast<size_t>(trace_idx)]);
            }
            series_traces.push_back(static_cast<double>(trace_idx));
            series_values.push_back(value_ms);
        }
    }

    sortSeriesByTrace(series_traces, series_values);
}

void buildHorizonPreview(const std::vector<std::pair<double, double>>& points,
                         const std::vector<double>& profile_x,
                         const std::vector<double>& profile_y,
                         int n_traces,
                         std::vector<double>& overlay_x,
                         std::vector<double>& overlay_y,
                         std::vector<double>& series_traces,
                         std::vector<double>& series_values)
{
    overlay_x.clear();
    overlay_y.clear();
    series_traces.clear();
    series_values.clear();

    for (const auto& [trace, time_ms] : points) {
        const int trace_idx = static_cast<int>(std::lround(trace));
        if (trace_idx < 0 || trace_idx >= n_traces)
            continue;

        if (trace_idx < static_cast<int>(profile_x.size())) {
            overlay_x.push_back(profile_x[static_cast<size_t>(trace_idx)]);
            overlay_y.push_back(profile_y[static_cast<size_t>(trace_idx)]);
        }
        series_traces.push_back(static_cast<double>(trace_idx));
        series_values.push_back(time_ms);
    }

    sortSeriesByTrace(series_traces, series_values);
}

} // namespace

namespace DataInfoDialog {

void showSegyProfile(QWidget* parent,
                     const QString& file_path,
                     const StreamMetadata& metadata,
                     int x_byte,
                     int y_byte,
                     const std::vector<double>& trace_x,
                     const std::vector<double>& trace_y)
{
    auto* dialog = createBaseDialog(parent, QObject::tr("SEG-Y Profile"));

    auto* main_layout = new QVBoxLayout(dialog);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);

    QFormLayout* stats_layout = nullptr;
    main_layout->addWidget(makeStatsGroup(dialog, &stats_layout));

    const QFileInfo file_info(file_path);
    addFormRow(stats_layout, QObject::tr("File"), file_info.fileName());
    addFormRow(stats_layout, QObject::tr("Path"), file_info.absoluteFilePath());
    addFormRow(stats_layout, QObject::tr("Traces"), QString::number(metadata.n_traces));
    addFormRow(stats_layout, QObject::tr("Samples / trace"), QString::number(metadata.n_samples));

    if (metadata.dt > 0.0f) {
        const double dt_ms = metadata.dt * 1000.0;
        const double max_time_ms = metadata.n_samples * dt_ms;
        addFormRow(stats_layout, QObject::tr("Sample interval"),
                   QStringLiteral("%1 ms").arg(QString::number(dt_ms, 'f', 4)));
        addFormRow(stats_layout, QObject::tr("Record length"),
                   QStringLiteral("%1 ms").arg(QString::number(max_time_ms, 'f', 2)));
    }

    addFormRow(stats_layout, QObject::tr("X byte"), QString::number(x_byte));
    addFormRow(stats_layout, QObject::tr("Y byte"), QString::number(y_byte));

    if (!trace_x.empty() && trace_x.size() == trace_y.size()) {
        const double min_x = *std::min_element(trace_x.begin(), trace_x.end());
        const double max_x = *std::max_element(trace_x.begin(), trace_x.end());
        const double min_y = *std::min_element(trace_y.begin(), trace_y.end());
        const double max_y = *std::max_element(trace_y.begin(), trace_y.end());
        addFormRow(stats_layout, QObject::tr("X range"), formatRange(min_x, max_x, 1));
        addFormRow(stats_layout, QObject::tr("Y range"), formatRange(min_y, max_y, 1));
    }

    auto* map_group = new QGroupBox(QObject::tr("Coordinate map"), dialog);
    auto* map_layout = new QVBoxLayout(map_group);
    auto* map = new ProfileOverlayMapWidget(map_group);
    map->setMinimumSize(280, 280);
    map->setProfile(trace_x, trace_y);
    map->setOverlay({}, {});
    map_layout->addWidget(map);
    main_layout->addWidget(map_group, 1);

    addCloseButton(dialog, main_layout);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void showStatics(QWidget* parent,
                 const QString& file_path,
                 const std::vector<StaticPoint>& points,
                 const std::vector<double>& profile_x,
                 const std::vector<double>& profile_y,
                 int n_traces)
{
    auto* dialog = createBaseDialog(parent, QObject::tr("Statics"));

    std::vector<double> overlay_x;
    std::vector<double> overlay_y;
    std::vector<double> series_traces;
    std::vector<double> series_values;
    buildStaticsPreview(points, profile_x, profile_y, n_traces,
                        overlay_x, overlay_y, series_traces, series_values);

    std::vector<double> value_ms;
    value_ms.reserve(points.size());
    for (const auto& sp : points)
        value_ms.push_back(sp.value * 1000.0);
    const ValueStats value_stats = computeValueStats(value_ms);

    auto* main_layout = new QVBoxLayout(dialog);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);

    QFormLayout* stats_layout = nullptr;
    main_layout->addWidget(makeStatsGroup(dialog, &stats_layout));

    const QFileInfo file_info(file_path);
    addFormRow(stats_layout, QObject::tr("File"), file_info.fileName());
    addFormRow(stats_layout, QObject::tr("Path"), file_info.absoluteFilePath());
    addFormRow(stats_layout, QObject::tr("Points"), QString::number(points.size()));
    addFormRow(stats_layout, QObject::tr("Mode"),
               staticsUseXYMode(points) ? QObject::tr("X / Y coordinates")
                                          : QObject::tr("Trace number"));

    if (value_stats.valid) {
        addFormRow(stats_layout, QObject::tr("Static range"),
                   formatRange(value_stats.min_v, value_stats.max_v, 2) + QObject::tr(" ms"));
        addFormRow(stats_layout, QObject::tr("Mean static"),
                   QStringLiteral("%1 ms").arg(QString::number(value_stats.mean_v, 'f', 2)));
    }

    if (!overlay_x.empty()) {
        const double min_x = *std::min_element(overlay_x.begin(), overlay_x.end());
        const double max_x = *std::max_element(overlay_x.begin(), overlay_x.end());
        const double min_y = *std::min_element(overlay_y.begin(), overlay_y.end());
        const double max_y = *std::max_element(overlay_y.begin(), overlay_y.end());
        addFormRow(stats_layout, QObject::tr("Overlay X"), formatRange(min_x, max_x, 1));
        addFormRow(stats_layout, QObject::tr("Overlay Y"), formatRange(min_y, max_y, 1));
    }

    auto* map_group = new QGroupBox(QObject::tr("Coordinate map"), dialog);
    auto* map_layout = new QVBoxLayout(map_group);
    auto* map = new ProfileOverlayMapWidget(map_group);
    map->setMinimumSize(280, 220);
    map->setProfile(profile_x, profile_y);
    map->setOverlay(overlay_x, overlay_y);
    map_layout->addWidget(map);
    main_layout->addWidget(map_group, 1);

    auto* series_group = new QGroupBox(QObject::tr("Statics vs trace"), dialog);
    auto* series_layout = new QVBoxLayout(series_group);
    auto* series = new TraceSeriesPlotWidget(series_group);
    series->setMinimumHeight(160);
    series->setSeries(series_traces, series_values, QObject::tr("Static, ms"));
    series_layout->addWidget(series);
    main_layout->addWidget(series_group);

    addCloseButton(dialog, main_layout);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void showHorizon(QWidget* parent,
                 const QString& file_path,
                 const std::vector<std::pair<double, double>>& points,
                 const std::vector<double>& profile_x,
                 const std::vector<double>& profile_y,
                 int n_traces)
{
    auto* dialog = createBaseDialog(parent, QObject::tr("Horizon"));

    std::vector<double> overlay_x;
    std::vector<double> overlay_y;
    std::vector<double> series_traces;
    std::vector<double> series_values;
    buildHorizonPreview(points, profile_x, profile_y, n_traces,
                        overlay_x, overlay_y, series_traces, series_values);

    std::vector<double> times_ms;
    times_ms.reserve(points.size());
    for (const auto& [trace, time_ms] : points)
        times_ms.push_back(time_ms);
    const ValueStats time_stats = computeValueStats(times_ms);

    auto* main_layout = new QVBoxLayout(dialog);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(12);

    QFormLayout* stats_layout = nullptr;
    main_layout->addWidget(makeStatsGroup(dialog, &stats_layout));

    const QFileInfo file_info(file_path);
    addFormRow(stats_layout, QObject::tr("File"), file_info.fileName());
    addFormRow(stats_layout, QObject::tr("Path"), file_info.absoluteFilePath());
    addFormRow(stats_layout, QObject::tr("Points"), QString::number(points.size()));

    if (time_stats.valid) {
        addFormRow(stats_layout, QObject::tr("Time range"),
                   formatRange(time_stats.min_v, time_stats.max_v, 2) + QObject::tr(" ms"));
        addFormRow(stats_layout, QObject::tr("Mean time"),
                   QStringLiteral("%1 ms").arg(QString::number(time_stats.mean_v, 'f', 2)));
    }

    if (!series_traces.empty()) {
        addFormRow(stats_layout, QObject::tr("Trace range"),
                   formatRange(series_traces.front(), series_traces.back(), 0));
    }

    auto* map_group = new QGroupBox(QObject::tr("Coordinate map"), dialog);
    auto* map_layout = new QVBoxLayout(map_group);
    auto* map = new ProfileOverlayMapWidget(map_group);
    map->setMinimumSize(280, 220);
    map->setProfile(profile_x, profile_y);
    map->setOverlay(overlay_x, overlay_y);
    map_layout->addWidget(map);
    main_layout->addWidget(map_group, 1);

    auto* series_group = new QGroupBox(QObject::tr("Horizon vs trace"), dialog);
    auto* series_layout = new QVBoxLayout(series_group);
    auto* series = new TraceSeriesPlotWidget(series_group);
    series->setMinimumHeight(160);
    series->setSeries(series_traces, series_values, QObject::tr("Time, ms"));
    series_layout->addWidget(series);
    main_layout->addWidget(series_group);

    addCloseButton(dialog, main_layout);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

} // namespace DataInfoDialog
