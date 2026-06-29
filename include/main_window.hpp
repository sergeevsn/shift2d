#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>
#include <QString>
#include <memory>
#include <vector>

#include "types.hpp"
#include "color_schemes.hpp"

class SeismicView;
class SegyLoadDialog;
class StaticsLoadDialog;
class HorizonLoadDialog;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QCheckBox;
class QAction;
class QActionGroup;
class QScrollBar;

class SegyReader;
class SegyWriter;

/// Главное окно приложения для применения статических сдвигов к SEG-Y
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // Файловые операции
    void onOpenSegy();
    void onOpenStatics();
    void onOpenHorizon();
    void onSaveSegy();

    // Обновление вида
    void onUpdateView();
    void onModeChanged(int id);
    void onPaletteChanged(int index);
    void onClipChanged(double value);
    void onExtraShiftChanged(double value);

    // Zoom/Pan обработка
    void onPanRequested(int delta_traces, int delta_samples);
    void onTraceZoomRequested(int first_trace, int count);
    void onTimeZoomRequested(int first_sample, int count);
    void onResetZoomRequested();

    // Скроллбары
    void onHScrollChanged(int value);
    void onVScrollChanged(int value);

    // Информация под курсором
    void onHoverInfoChanged(int trace_index, double time_ms, float amplitude, bool valid);

private:
    void setupUi();
    void setupMenu();
    void setupToolbar();
    void createLeftPanel();

    // Загрузка данных
    void loadSegyFile(const QString& path, int x_byte, int y_byte);
    void loadStaticsFile(const QString& path, bool xy_mode, int x_col, int y_col, int trace_col, int static_col,
                         bool nearest_match, double match_x_range, double match_y_range, bool value_in_ms);
    void loadHorizonFile(const QString& path, bool xy_mode, int x_col, int y_col,
                         int trace_col, int time_col, bool nearest_match, double match_x_range,
                         double match_y_range, bool time_in_ms);

    // Применение статик и обновление вида
    void applyStaticsAndUpdate();
    void applyViewLimitsFromControls();
    std::vector<float> computeInterpolatedStatics();
    float computeClipForTraces(const std::vector<Trace>& traces) const;
    void updateScrollbars();
    void syncLimitControls();

    // Вспомогательные функции для интерполяции
    std::vector<float> fillMissingStatics(const std::vector<float>& statics,
                                          const std::vector<bool>& known_mask) const;
    std::vector<float> smoothStatics(const std::vector<float>& statics) const;
    void updateSmoothStaticsControls();
    int findNearestTraceInRange(double x, double y) const;

    // UI элементы
    SeismicView* seismic_view_ = nullptr;
    QScrollBar* h_scrollbar_ = nullptr;
    QScrollBar* v_scrollbar_ = nullptr;
    QWidget* left_panel_ = nullptr;

    // Левая панель
    QLabel* segy_path_label_ = nullptr;
    QLabel* statics_path_label_ = nullptr;
    QLabel* horizon_path_label_ = nullptr;
    QLabel* status_label_ = nullptr;

    QButtonGroup* mode_group_ = nullptr;
    QRadioButton* mode_none_radio_ = nullptr;
    QRadioButton* mode_forward_radio_ = nullptr;
    QRadioButton* mode_inverse_radio_ = nullptr;
    QCheckBox* interpolate_checkbox_ = nullptr;
    QCheckBox* extrapolate_checkbox_ = nullptr;
    QCheckBox* smooth_statics_checkbox_ = nullptr;
    QWidget* smooth_statics_options_ = nullptr;
    QComboBox* smooth_filter_type_combo_ = nullptr;
    QSpinBox* smooth_filter_size_spin_ = nullptr;

    QDoubleSpinBox* extra_shift_spin_ = nullptr;
    QDoubleSpinBox* clip_spin_ = nullptr;
    QComboBox* palette_combo_ = nullptr;

    QSpinBox* trace_min_spin_ = nullptr;
    QSpinBox* trace_max_spin_ = nullptr;
    QSpinBox* time_min_spin_ = nullptr;
    QSpinBox* time_max_spin_ = nullptr;

    QPushButton* reset_zoom_btn_ = nullptr;
    QPushButton* apply_limits_btn_ = nullptr;

    QAction* zoom_action_ = nullptr;
    QAction* pan_action_ = nullptr;

    // Данные
    std::unique_ptr<SegyReader> reader_;
    std::vector<Trace> original_traces_;
    std::vector<Trace> shifted_traces_;
    StreamMetadata metadata_;

    // Настройки SEGY
    QString current_segy_path_;
    int x_byte_ = 181;
    int y_byte_ = 185;
    double coord_match_x_range_ = 12.0;
    double coord_match_y_range_ = 12.0;
    bool coord_match_nearest_ = true;
    std::vector<double> trace_x_coords_;
    std::vector<double> trace_y_coords_;

    // Статики
    QString current_statics_path_;
    std::vector<StaticPoint> static_points_;

    // Горизонт
    QString current_horizon_path_;
    std::vector<std::pair<double, double>> horizon_points_;  // trace -> time_ms

    // Настройки отображения
    ColorMap current_palette_ = ColorMap::Grayscale;
    int extra_shift_mode_ = 0;  // 0=none, 1=forward, 2=inverse
    double extra_shift_value_ = 0.0;

    // Окно отображения
    int view_first_trace_ = 0;
    int view_trace_count_ = 0;
    int view_first_sample_ = 0;
    int view_sample_count_ = 0;
};

#endif // MAIN_WINDOW_HPP
