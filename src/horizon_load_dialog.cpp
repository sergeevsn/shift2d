#include "horizon_load_dialog.hpp"

#include "app_theme.hpp"

#include "load_preview_widgets.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <algorithm>
#include <cmath>
#include <limits>

HorizonLoadDialog::HorizonLoadDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    AppTheme::applyLoadDialogTypography(this);
    setWindowTitle(tr("Horizon CSV Load Settings"));
    setMinimumWidth(880);
    setMinimumHeight(480);
}

void HorizonLoadDialog::setSourceFile(const QString& path)
{
    source_path_ = path;
    updateContentPreview();
}

void HorizonLoadDialog::setColumnHeaders(const QStringList& headers,
                                         const std::vector<std::vector<QString>>& preview_data)
{
    headers_ = headers;
    preview_data_ = preview_data;

    x_col_combo_->clear();
    y_col_combo_->clear();
    trace_col_combo_->clear();
    time_col_combo_->clear();

    for (int i = 0; i < headers.size(); ++i) {
        QString label = headers[i].isEmpty() ? tr("Column %1").arg(i) : headers[i];
        x_col_combo_->addItem(label, i);
        y_col_combo_->addItem(label, i);
        trace_col_combo_->addItem(label, i);
        time_col_combo_->addItem(label, i);
    }

    if (x_col_combo_->count() > 0)
        x_col_combo_->setCurrentIndex(std::min(0, x_col_combo_->count() - 1));
    if (y_col_combo_->count() > 1)
        y_col_combo_->setCurrentIndex(std::min(1, y_col_combo_->count() - 1));
    if (trace_col_combo_->count() > 0)
        trace_col_combo_->setCurrentIndex(0);
    if (time_col_combo_->count() > 2)
        time_col_combo_->setCurrentIndex(std::min(2, time_col_combo_->count() - 1));

    autoDetectTimeUnit(time_col_combo_->currentData().toInt());
    updateContentPreview();
}

void HorizonLoadDialog::setProfileCoordinates(const std::vector<double>& xs, const std::vector<double>& ys)
{
    profile_xs_ = xs;
    profile_ys_ = ys;
    updateContentPreview();
}

void HorizonLoadDialog::setMatchOptions(bool nearest_trace, double x_range, double y_range)
{
    use_nearest_trace_ = nearest_trace;
    if (nearest_trace_radio_)
        nearest_trace_radio_->setChecked(nearest_trace);
    if (range_match_radio_)
        range_match_radio_->setChecked(!nearest_trace);
    if (x_range_spin_)
        x_range_spin_->setValue(x_range);
    if (y_range_spin_)
        y_range_spin_->setValue(y_range);
    onMatchModeChanged();
}

bool HorizonLoadDialog::useXYMode() const
{
    return xy_mode_;
}

int HorizonLoadDialog::xColumn() const
{
    return x_col_;
}

int HorizonLoadDialog::yColumn() const
{
    return y_col_;
}

int HorizonLoadDialog::traceColumn() const
{
    return trace_col_;
}

int HorizonLoadDialog::timeColumn() const
{
    return time_col_;
}

double HorizonLoadDialog::matchXRange() const
{
    return match_x_range_;
}

double HorizonLoadDialog::matchYRange() const
{
    return match_y_range_;
}

bool HorizonLoadDialog::timeInMilliseconds() const
{
    return time_in_ms_;
}

bool HorizonLoadDialog::useNearestTraceMatch() const
{
    return use_nearest_trace_;
}

void HorizonLoadDialog::autoDetectTimeUnit(int col)
{
    if (!time_unit_combo_)
        return;

    double max_abs = 0.0;
    int count = 0;
    for (const auto& row : preview_data_) {
        if (col < 0 || col >= row.size())
            continue;
        bool ok = false;
        const double v = row[col].toDouble(&ok);
        if (!ok)
            continue;
        max_abs = std::max(max_abs, std::abs(v));
        count++;
    }

    if (count == 0)
        return;

    time_unit_combo_->setCurrentIndex(max_abs >= 10.0 ? 1 : 0);
}

void HorizonLoadDialog::onTimeColumnChanged()
{
    autoDetectTimeUnit(time_col_combo_->currentData().toInt());
    updateContentPreview();
}

void HorizonLoadDialog::onPreviewSettingsChanged()
{
    updateContentPreview();
}

int HorizonLoadDialog::findNearestTrace(double x, double y) const
{
    if (profile_xs_.empty() || profile_ys_.empty())
        return -1;

    const bool use_range = range_match_radio_ && range_match_radio_->isChecked();
    const double match_x = x_range_spin_ ? x_range_spin_->value() : match_x_range_;
    const double match_y = y_range_spin_ ? y_range_spin_->value() : match_y_range_;

    int nearest_trace = -1;
    double min_dist_sq = std::numeric_limits<double>::max();
    const int n = static_cast<int>(profile_xs_.size());

    for (int i = 0; i < n; ++i) {
        const double dx = std::abs(profile_xs_[i] - x);
        const double dy = std::abs(profile_ys_[i] - y);
        if (use_range) {
            if (match_x > 0.0 && dx > match_x)
                continue;
            if (match_y > 0.0 && dy > match_y)
                continue;
        }

        const double dist_sq = dx * dx + dy * dy;
        if (dist_sq < min_dist_sq) {
            min_dist_sq = dist_sq;
            nearest_trace = i;
        }
    }

    return nearest_trace;
}

void HorizonLoadDialog::onMatchModeChanged()
{
    const bool use_range = range_match_radio_ && range_match_radio_->isChecked();
    if (x_range_spin_)
        x_range_spin_->setEnabled(use_range);
    if (y_range_spin_)
        y_range_spin_->setEnabled(use_range);
    updateContentPreview();
}

void HorizonLoadDialog::setupUi()
{
    auto* main_layout = new QVBoxLayout(this);

    auto* content_layout = new QHBoxLayout();
    content_layout->setSpacing(12);

    auto* params_column = new QVBoxLayout();
    params_column->setSpacing(8);

    auto* mode_group = new QGroupBox(tr("Location Mode"), this);
    auto* mode_layout = new QVBoxLayout(mode_group);

    xy_mode_radio_ = new QRadioButton(tr("Use X/Y coordinates"), this);
    xy_mode_radio_->setChecked(true);
    connect(xy_mode_radio_, &QRadioButton::toggled, this, &HorizonLoadDialog::onModeChanged);
    mode_layout->addWidget(xy_mode_radio_);

    trace_mode_radio_ = new QRadioButton(tr("Use Trace Numbers"), this);
    connect(trace_mode_radio_, &QRadioButton::toggled, this, &HorizonLoadDialog::onModeChanged);
    mode_layout->addWidget(trace_mode_radio_);

    params_column->addWidget(mode_group);

    auto* col_group = new QGroupBox(tr("Column Mapping"), this);
    auto* col_layout = new QFormLayout(col_group);

    x_col_combo_ = new QComboBox(this);
    col_layout->addRow(tr("X column:"), x_col_combo_);

    y_col_combo_ = new QComboBox(this);
    col_layout->addRow(tr("Y column:"), y_col_combo_);

    trace_col_combo_ = new QComboBox(this);
    trace_col_combo_->setEnabled(false);
    col_layout->addRow(tr("Trace column:"), trace_col_combo_);

    time_col_combo_ = new QComboBox(this);
    col_layout->addRow(tr("Time column:"), time_col_combo_);

    time_unit_combo_ = new QComboBox(this);
    time_unit_combo_->addItem(tr("Seconds"), 0);
    time_unit_combo_->addItem(tr("Milliseconds"), 1);
    col_layout->addRow(tr("Time units:"), time_unit_combo_);

    params_column->addWidget(col_group);

    auto* range_group = new QGroupBox(tr("Coordinate Matching"), this);
    auto* range_layout = new QVBoxLayout(range_group);

    nearest_trace_radio_ = new QRadioButton(tr("Nearest trace"), range_group);
    nearest_trace_radio_->setChecked(true);
    range_layout->addWidget(nearest_trace_radio_);

    range_match_radio_ = new QRadioButton(tr("Match within range"), range_group);
    range_layout->addWidget(range_match_radio_);

    auto* range_fields = new QFormLayout();
    x_range_spin_ = new QDoubleSpinBox(range_group);
    x_range_spin_->setRange(0.0, 1e9);
    x_range_spin_->setDecimals(3);
    x_range_spin_->setValue(12.0);
    x_range_spin_->setEnabled(false);
    range_fields->addRow(tr("X range:"), x_range_spin_);

    y_range_spin_ = new QDoubleSpinBox(range_group);
    y_range_spin_->setRange(0.0, 1e9);
    y_range_spin_->setDecimals(3);
    y_range_spin_->setValue(12.0);
    y_range_spin_->setEnabled(false);
    range_fields->addRow(tr("Y range:"), y_range_spin_);
    range_layout->addLayout(range_fields);

    connect(nearest_trace_radio_, &QRadioButton::toggled, this, &HorizonLoadDialog::onMatchModeChanged);
    connect(range_match_radio_, &QRadioButton::toggled, this, &HorizonLoadDialog::onMatchModeChanged);

    params_column->addWidget(range_group);
    params_column->addStretch();

    auto* previews_column = new QVBoxLayout();
    previews_column->setSpacing(8);

    auto* map_group = new QGroupBox(tr("Map Preview"), this);
    auto* map_layout = new QVBoxLayout(map_group);
    map_preview_ = new ProfileOverlayMapWidget(map_group);
    map_preview_->setMinimumSize(280, 220);
    map_layout->addWidget(map_preview_, 1);
    previews_column->addWidget(map_group, 1);

    auto* series_group = new QGroupBox(tr("Horizon Preview"), this);
    auto* series_layout = new QVBoxLayout(series_group);
    series_preview_ = new TraceSeriesPlotWidget(series_group);
    series_preview_->setMinimumHeight(140);
    series_layout->addWidget(series_preview_);
    previews_column->addWidget(series_group);

    content_layout->addLayout(params_column, 0);
    content_layout->addLayout(previews_column, 1);
    main_layout->addLayout(content_layout, 1);

    connect(x_col_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);
    connect(y_col_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);
    connect(trace_col_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);
    connect(time_col_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HorizonLoadDialog::onTimeColumnChanged);
    connect(time_unit_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);
    connect(x_range_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);
    connect(y_range_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &HorizonLoadDialog::onPreviewSettingsChanged);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    ok_button_ = new QPushButton(tr("OK"), this);
    ok_button_->setDefault(true);
    connect(ok_button_, &QPushButton::clicked, this, &HorizonLoadDialog::onOkClicked);
    button_layout->addWidget(ok_button_);

    cancel_button_ = new QPushButton(tr("Cancel"), this);
    connect(cancel_button_, &QPushButton::clicked, this, &HorizonLoadDialog::onCancelClicked);
    button_layout->addWidget(cancel_button_);

    main_layout->addLayout(button_layout);
}

void HorizonLoadDialog::updateTablePreview()
{
}

void HorizonLoadDialog::updateContentPreview()
{
    if (!map_preview_ || !series_preview_)
        return;

    map_preview_->setProfile(profile_xs_, profile_ys_);

    if (source_path_.isEmpty()) {
        map_preview_->setOverlay({}, {});
        series_preview_->setSeries({}, {}, tr("Time, ms"));
        return;
    }

    QStringList headers;
    std::vector<std::vector<QString>> rows;
    if (!readCsvDataRows(source_path_, headers, rows)) {
        map_preview_->setOverlay({}, {});
        series_preview_->setSeries({}, {}, tr("Time, ms"));
        return;
    }

    const bool xy_mode = xy_mode_radio_->isChecked();
    const int x_col = x_col_combo_->currentData().toInt();
    const int y_col = y_col_combo_->currentData().toInt();
    const int trace_col = trace_col_combo_->currentData().toInt();
    const int time_col = time_col_combo_->currentData().toInt();
    const bool time_in_ms = time_unit_combo_->currentIndex() == 1;

    std::vector<double> overlay_xs;
    std::vector<double> overlay_ys;
    std::vector<double> series_traces;
    std::vector<double> series_values;

    for (const auto& row : rows) {
        try {
            if (time_col < 0 || time_col >= row.size())
                continue;

            double time_ms = row[time_col].toDouble();
            if (!time_in_ms)
                time_ms *= 1000.0;

            if (xy_mode) {
                if (x_col < 0 || x_col >= row.size() || y_col < 0 || y_col >= row.size())
                    continue;
                const double x = row[x_col].toDouble();
                const double y = row[y_col].toDouble();
                overlay_xs.push_back(x);
                overlay_ys.push_back(y);

                const int trace_idx = findNearestTrace(x, y);
                if (trace_idx < 0)
                    continue;
                series_traces.push_back(static_cast<double>(trace_idx));
                series_values.push_back(time_ms);
            } else {
                if (trace_col < 0 || trace_col >= row.size())
                    continue;
                const int trace_idx = static_cast<int>(std::lround(row[trace_col].toDouble()));
                if (trace_idx < 0 || trace_idx >= static_cast<int>(profile_xs_.size()))
                    continue;

                overlay_xs.push_back(profile_xs_[trace_idx]);
                overlay_ys.push_back(profile_ys_[trace_idx]);
                series_traces.push_back(static_cast<double>(trace_idx));
                series_values.push_back(time_ms);
            }
        } catch (...) {
        }
    }

    if (!series_traces.empty()) {
        std::vector<size_t> order(series_traces.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return series_traces[a] < series_traces[b];
        });

        std::vector<double> sorted_traces;
        std::vector<double> sorted_values;
        sorted_traces.reserve(order.size());
        sorted_values.reserve(order.size());
        for (size_t idx : order) {
            sorted_traces.push_back(series_traces[idx]);
            sorted_values.push_back(series_values[idx]);
        }
        series_traces = std::move(sorted_traces);
        series_values = std::move(sorted_values);
    }

    map_preview_->setOverlay(overlay_xs, overlay_ys);
    series_preview_->setSeries(series_traces, series_values, tr("Time, ms"));
}

void HorizonLoadDialog::onModeChanged()
{
    bool xy_mode = xy_mode_radio_->isChecked();
    x_col_combo_->setEnabled(xy_mode);
    y_col_combo_->setEnabled(xy_mode);
    trace_col_combo_->setEnabled(!xy_mode);
    if (x_range_spin_)
        x_range_spin_->setEnabled(xy_mode && range_match_radio_->isChecked());
    if (y_range_spin_)
        y_range_spin_->setEnabled(xy_mode && range_match_radio_->isChecked());
    if (nearest_trace_radio_)
        nearest_trace_radio_->setEnabled(xy_mode);
    if (range_match_radio_)
        range_match_radio_->setEnabled(xy_mode);
    updateContentPreview();
}

void HorizonLoadDialog::onOkClicked()
{
    xy_mode_ = xy_mode_radio_->isChecked();
    x_col_ = x_col_combo_->currentData().toInt();
    y_col_ = y_col_combo_->currentData().toInt();
    trace_col_ = trace_col_combo_->currentData().toInt();
    time_col_ = time_col_combo_->currentData().toInt();
    match_x_range_ = x_range_spin_->value();
    match_y_range_ = y_range_spin_->value();
    use_nearest_trace_ = nearest_trace_radio_->isChecked();
    time_in_ms_ = time_unit_combo_->currentIndex() == 1;
    accept();
}

void HorizonLoadDialog::onCancelClicked()
{
    reject();
}
