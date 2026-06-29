#ifndef LOAD_PREVIEW_WIDGETS_HPP
#define LOAD_PREVIEW_WIDGETS_HPP

#include <QWidget>
#include <QString>
#include <QStringList>
#include <vector>

/// Карта профиля SEG-Y с наложением точек CSV (aspect 1:1).
class ProfileOverlayMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProfileOverlayMapWidget(QWidget* parent = nullptr);

    void setProfile(const std::vector<double>& xs, const std::vector<double>& ys);
    void setOverlay(const std::vector<double>& xs, const std::vector<double>& ys);

    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<double> profile_xs_;
    std::vector<double> profile_ys_;
    std::vector<double> overlay_xs_;
    std::vector<double> overlay_ys_;
};

/// График значения по индексу трассы.
class TraceSeriesPlotWidget : public QWidget {
    Q_OBJECT

public:
    explicit TraceSeriesPlotWidget(QWidget* parent = nullptr);

    void setSeries(const std::vector<double>& traces,
                   const std::vector<double>& values,
                   const QString& y_label);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<double> traces_;
    std::vector<double> values_;
    QString y_label_;
};

bool readCsvDataRows(const QString& path,
                     QStringList& headers,
                     std::vector<std::vector<QString>>& rows);

#endif // LOAD_PREVIEW_WIDGETS_HPP
