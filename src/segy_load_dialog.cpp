#include "segy_load_dialog.hpp"
#include "app_theme.hpp"

#include "segy_reader.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QStyle>
#include <QPaintEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

class CoordPreviewWidget : public QWidget {
public:
    explicit CoordPreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(200, 200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return width;
    }

    void setPoints(const std::vector<double>& xs, const std::vector<double>& ys)
    {
        xs_ = xs;
        ys_ = ys;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter p(this);
        p.fillRect(rect(), AppTheme::plotSurface());

        if (xs_.empty() || ys_.empty() || xs_.size() != ys_.size()) {
            p.setPen(AppTheme::emptyStateHint());
            p.drawText(rect(), Qt::AlignCenter, tr("No coordinate preview"));
            return;
        }

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        for (size_t i = 0; i < xs_.size(); ++i) {
            min_x = std::min(min_x, xs_[i]);
            max_x = std::max(max_x, xs_[i]);
            min_y = std::min(min_y, ys_[i]);
            max_y = std::max(max_y, ys_[i]);
        }

        const double span_x = max_x - min_x;
        const double span_y = max_y - min_y;
        const double pad = std::max(1.0, std::max(span_x, span_y) * 0.05);
        const double uniform_span = std::max(span_x + 2.0 * pad, span_y + 2.0 * pad);
        const double center_x = (min_x + max_x) * 0.5;
        const double center_y = (min_y + max_y) * 0.5;
        const double plot_min_x = center_x - uniform_span * 0.5;
        const double plot_min_y = center_y - uniform_span * 0.5;

        const QRect avail = rect().adjusted(12, 8, -12, -8);
        const int side = std::min(avail.width(), avail.height());
        const QRect plot(avail.left() + (avail.width() - side) / 2,
                         avail.top() + (avail.height() - side) / 2,
                         side,
                         side);

        p.setPen(AppTheme::plotBorder());
        p.drawRect(plot);

        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(AppTheme::accent());

        for (size_t i = 0; i < xs_.size(); ++i) {
            const double rel_x = (xs_[i] - plot_min_x) / uniform_span;
            const double rel_y = (ys_[i] - plot_min_y) / uniform_span;
            const int px = plot.left() + static_cast<int>(rel_x * plot.width());
            const int py = plot.bottom() - static_cast<int>(rel_y * plot.height());
            p.drawEllipse(QPoint(px, py), 2, 2);
        }
    }

private:
    std::vector<double> xs_;
    std::vector<double> ys_;
};

bool isMonotonicSeries(const std::vector<double>& values)
{
    if (values.size() < 2)
        return false;

    bool all_same = true;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] != values[0]) {
            all_same = false;
            break;
        }
    }
    if (all_same)
        return false;

    bool increasing = true;
    bool decreasing = true;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] < values[i - 1])
            increasing = false;
        if (values[i] > values[i - 1])
            decreasing = false;
    }
    return increasing || decreasing;
}

bool isValidCoordinatePair(const std::vector<double>& xs, const std::vector<double>& ys)
{
    if (xs.empty() || ys.empty())
        return false;

    bool all_zero = true;
    for (size_t i = 0; i < xs.size(); ++i) {
        if (xs[i] != 0.0 || ys[i] != 0.0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero)
        return false;

    return isMonotonicSeries(xs) || isMonotonicSeries(ys);
}

QString formatRange(double min_v, double max_v)
{
    return QStringLiteral("[%1 .. %2]")
        .arg(QString::number(min_v, 'f', 1))
        .arg(QString::number(max_v, 'f', 1));
}

}  // namespace

SegyLoadDialog::SegyLoadDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("SEGY Load Settings"));
    setMinimumWidth(520);
    setMinimumHeight(420);
}

void SegyLoadDialog::analyzeFile(const QString& path)
{
    options_.clear();
    options_.push_back({181, 185, tr("181-185 — CDP Coordinates")});
    options_.push_back({73, 77, tr("73-77 — Source Coordinates")});
    options_.push_back({81, 85, tr("81-85 — Receiver Coordinates")});

    try {
        SegyReader reader;
        reader.open(path.toStdString());
        const int n_traces = reader.traceCount();

        for (auto& option : options_) {
            option.xs.clear();
            option.ys.clear();
            option.xs.reserve(static_cast<size_t>(n_traces));
            option.ys.reserve(static_cast<size_t>(n_traces));

            for (int i = 0; i < n_traces; ++i) {
                option.xs.push_back(reader.getTraceHeaderValue(i, option.x_byte));
                option.ys.push_back(reader.getTraceHeaderValue(i, option.y_byte));
            }

            const double min_x = *std::min_element(option.xs.begin(), option.xs.end());
            const double max_x = *std::max_element(option.xs.begin(), option.xs.end());
            const double min_y = *std::min_element(option.ys.begin(), option.ys.end());
            const double max_y = *std::max_element(option.ys.begin(), option.ys.end());

            option.valid = isValidCoordinatePair(option.xs, option.ys);
            option.range_text = tr("X %1, Y %2")
                                    .arg(formatRange(min_x, max_x))
                                    .arg(formatRange(min_y, max_y));
            if (!option.valid)
                option.range_text += tr(" — not monotonic");
        }
    } catch (...) {
        for (auto& option : options_) {
            option.valid = false;
            option.range_text = tr("Failed to read coordinates");
        }
    }

    selected_index_ = 0;
    for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
        if (options_[i].valid) {
            selected_index_ = i;
            break;
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (!option_radios_[i] || i >= static_cast<int>(options_.size()))
            continue;

        const auto& option = options_[i];
        option_labels_[i]->setText(QStringLiteral("%1\n%2")
                                       .arg(option.title, option.range_text));
        option_labels_[i]->setProperty("valid", option.valid);
        option_labels_[i]->style()->unpolish(option_labels_[i]);
        option_labels_[i]->style()->polish(option_labels_[i]);
        option_radios_[i]->setEnabled(true);
        option_radios_[i]->setChecked(i == selected_index_);
    }

    onSelectionChanged();
}

int SegyLoadDialog::xByte() const
{
    return x_byte_;
}

int SegyLoadDialog::yByte() const
{
    return y_byte_;
}

void SegyLoadDialog::setupUi()
{
    auto* main_layout = new QVBoxLayout(this);

    auto* coord_group = new QGroupBox(tr("Trace Coordinates"), this);
    auto* coord_layout = new QVBoxLayout(coord_group);

    auto* button_group = new QButtonGroup(this);
    for (int i = 0; i < 3; ++i) {
        option_radios_[i] = new QRadioButton(this);
        option_labels_[i] = new QLabel(this);
        option_labels_[i]->setObjectName(QStringLiteral("dialogOptionLabel"));
        option_labels_[i]->setWordWrap(true);

        auto* row = new QHBoxLayout();
        row->addWidget(option_radios_[i]);
        row->addWidget(option_labels_[i], 1);
        coord_layout->addLayout(row);

        button_group->addButton(option_radios_[i], i);
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(button_group, QOverload<int>::of(&QButtonGroup::idClicked),
#else
    connect(button_group, QOverload<int>::of(&QButtonGroup::buttonClicked),
#endif
            this, &SegyLoadDialog::onSelectionChanged);

    main_layout->addWidget(coord_group);

    auto* map_group = new QGroupBox(tr("Coordinate Preview"), this);
    auto* map_layout = new QVBoxLayout(map_group);
    preview_map_ = new CoordPreviewWidget(map_group);
    map_layout->addWidget(preview_map_);
    main_layout->addWidget(map_group);

    auto* info_label = new QLabel(
        tr("Select the byte pair whose coordinates change monotonically along traces."),
        this);
    info_label->setObjectName(QStringLiteral("dialogHint"));
    info_label->setWordWrap(true);
    main_layout->addWidget(info_label);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    ok_button_ = new QPushButton(tr("OK"), this);
    ok_button_->setObjectName(QStringLiteral("primaryButton"));
    ok_button_->setDefault(true);
    connect(ok_button_, &QPushButton::clicked, this, &SegyLoadDialog::onOkClicked);
    button_layout->addWidget(ok_button_);

    cancel_button_ = new QPushButton(tr("Cancel"), this);
    connect(cancel_button_, &QPushButton::clicked, this, &SegyLoadDialog::onCancelClicked);
    button_layout->addWidget(cancel_button_);

    main_layout->addLayout(button_layout);
}

void SegyLoadDialog::onSelectionChanged()
{
    for (int i = 0; i < 3; ++i) {
        if (option_radios_[i] && option_radios_[i]->isChecked()) {
            selected_index_ = i;
            break;
        }
    }

    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
        x_byte_ = options_[selected_index_].x_byte;
        y_byte_ = options_[selected_index_].y_byte;
    }

    updatePreviewMap();
}

void SegyLoadDialog::updatePreviewMap()
{
    auto* map = dynamic_cast<CoordPreviewWidget*>(preview_map_);
    if (!map || selected_index_ < 0 || selected_index_ >= static_cast<int>(options_.size()))
        return;

    const auto& option = options_[selected_index_];
    map->setPoints(option.xs, option.ys);
}

void SegyLoadDialog::onOkClicked()
{
    onSelectionChanged();
    accept();
}

void SegyLoadDialog::onCancelClicked()
{
    reject();
}
