#ifndef STATICS_LOAD_DIALOG_HPP
#define STATICS_LOAD_DIALOG_HPP

#include <QDialog>
#include <QString>
#include <QStringList>
#include <vector>

class QComboBox;
class QRadioButton;
class QPushButton;
class QDoubleSpinBox;
class ProfileOverlayMapWidget;
class TraceSeriesPlotWidget;

/// Диалог настроек загрузки CSV статик
class StaticsLoadDialog : public QDialog {
    Q_OBJECT

public:
    explicit StaticsLoadDialog(QWidget* parent = nullptr);

    void setSourceFile(const QString& path);
    void setColumnHeaders(const QStringList& headers, const std::vector<std::vector<QString>>& preview_data);
    void setProfileCoordinates(const std::vector<double>& xs, const std::vector<double>& ys);
    void setMatchOptions(bool nearest_trace, double x_range, double y_range);

    bool useXYMode() const;
    int xColumn() const;
    int yColumn() const;
    int traceColumn() const;
    int staticColumn() const;
    double matchXRange() const;
    double matchYRange() const;
    bool useNearestTraceMatch() const;
    bool valueInMilliseconds() const;

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onModeChanged();
    void onValueColumnChanged();
    void onPreviewSettingsChanged();
    void onMatchModeChanged();

private:
    void setupUi();
    void updateTablePreview();
    void updateContentPreview();
    void autoDetectTimeUnit(int col);
    int findNearestTrace(double x, double y) const;

    QRadioButton* xy_mode_radio_ = nullptr;
    QRadioButton* trace_mode_radio_ = nullptr;
    QRadioButton* nearest_trace_radio_ = nullptr;
    QRadioButton* range_match_radio_ = nullptr;
    QComboBox* x_col_combo_ = nullptr;
    QComboBox* y_col_combo_ = nullptr;
    QComboBox* trace_col_combo_ = nullptr;
    QComboBox* static_col_combo_ = nullptr;
    QComboBox* time_unit_combo_ = nullptr;
    QDoubleSpinBox* x_range_spin_ = nullptr;
    QDoubleSpinBox* y_range_spin_ = nullptr;
    ProfileOverlayMapWidget* map_preview_ = nullptr;
    TraceSeriesPlotWidget* series_preview_ = nullptr;
    QPushButton* ok_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;

    QString source_path_;
    QStringList headers_;
    std::vector<std::vector<QString>> preview_data_;
    std::vector<double> profile_xs_;
    std::vector<double> profile_ys_;

    int x_col_ = 0;
    int y_col_ = 1;
    int trace_col_ = 0;
    int static_col_ = 2;
    double match_x_range_ = 12.0;
    double match_y_range_ = 12.0;
    bool use_nearest_trace_ = true;
    bool value_in_ms_ = false;
    bool xy_mode_ = true;
};

#endif // STATICS_LOAD_DIALOG_HPP
