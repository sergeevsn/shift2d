#ifndef SEISMIC_VIEW_HPP
#define SEISMIC_VIEW_HPP

#include <QWidget>
#include <QColor>
#include <QPoint>
#include <vector>
#include <memory>
#include <optional>

#include "types.hpp"
#include "color_schemes.hpp"

/// Виджет для отображения сейсмического профиля SEG-Y
/// Поддерживает zoom/pan, настройку палитры, клиппинг
class SeismicView : public QWidget {
    Q_OBJECT

public:
    explicit SeismicView(QWidget* parent = nullptr);

    /// Установить данные для отображения
    void setData(const std::vector<Trace>& traces, float dt);

    /// Установить окно по трассам [first_trace, first_trace + count)
    void setTraceWindow(int first_trace, int count);

    /// Установить окно по времени [first_sample, first_sample + count)
    void setTimeWindow(int first_sample, int sample_count);

    /// Установить цветовую палитру
    void setColorMap(ColorMap cmap);

    /// Установить значение клиппинга (для амплитудной нормализации)
    void setClipValue(float clip);
    float clipValue() const { return clip_value_; }

    /// Установить режим взаимодействия (Zoom / Pan / None)
    enum class InteractionMode { None, Zoom, Pan };
    void setInteractionMode(InteractionMode mode);

    /// Получить текущие границы отображения
    int firstTrace() const { return first_trace_; }
    int traceCount() const { return trace_count_; }
    int firstSample() const { return first_sample_; }
    int sampleCount() const { return sample_count_; }

    /// Сбросить зум на полный разрез
    void resetZoom();

    /// Установить данные горизонта для отрисовки
    void setHorizon(const std::vector<std::pair<double, double>>& horizon_points);
    void clearHorizon();

    void setStaticsCurve(const std::vector<float>& values);
    void clearStaticsCurve();

signals:
    /// Запрос изменения окна (при панорамировании)
    void panRequested(int delta_traces, int delta_samples);

    /// Запрос зума по трассам
    void traceZoomRequested(int first_trace, int count);

    /// Запрос зума по времени
    void timeZoomRequested(int first_sample, int count);

    /// Запрос сброса зума
    void resetZoomRequested();

    /// Информация под курсором
    void hoverInfoChanged(int trace_index, double time_ms, float amplitude, bool valid);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct PlotRect {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        int width() const { return right - left; }
        int height() const { return bottom - top; }
    };

    // Константы отступов
    static constexpr int kLeftMargin = 90;
    static constexpr int kTimeAxisTitleColumn = 22;
    static constexpr int kTimeTickLabelGap = 6;
    static constexpr int kRightMargin = 10;
    static constexpr int kTopMargin = 40;
    static constexpr int kTraceAxisLineOffset = 8;
    static constexpr int kTraceTickLength = 4;
    static constexpr int kTraceTickLabelGap = 6;
    static constexpr int kTraceAxisTitleGap = 10;
    static constexpr int kTraceAxisBottomPadding = 6;
    static constexpr int kStaticsDataGap = 12;

    int traceAxisBandHeight() const;

    PlotRect plotRect() const;
    float traceSpacingPx() const;
    float traceX(int trace_idx, const PlotRect& plot) const;
    int traceIndexAtX(int x_px, const PlotRect& plot) const;
    double timeMsFromY(int y_px, const PlotRect& plot) const;
    int sampleFromY(int y_px, const PlotRect& plot) const;
    PlotRect fullPlotRect() const;
    int staticsRegionHeight() const;

    void updateCachedGeometry();
    void updateColorLut();

    // Отрисовка
    void drawSeismicImage(QPainter& p, const PlotRect& plot);
    void drawPlotFrame(QPainter& p, const PlotRect& plot);
    void drawEmptyState(QPainter& p);
    void drawAxes(QPainter& p, const PlotRect& plot);
    void drawHorizon(QPainter& p, const PlotRect& plot);
    void drawZoomRect(QPainter& p);
    void drawStaticsCurve(QPainter& p, const PlotRect& statics_rect);

    // Данные
    std::vector<Trace> traces_;
    float dt_ = 0.0f;
    int n_samples_total_ = 0;

    // Окно отображения
    int first_trace_ = 0;
    int trace_count_ = 0;
    int first_sample_ = 0;
    int sample_count_ = 0;

    // Настройки отображения
    ColorMap color_map_ = ColorMap::Grayscale;
    float clip_value_ = 1.0f;
    std::vector<QRgb> color_lut_;

    // Геометрия
    float samples_per_pixel_y_ = 1.0f;

    // Взаимодействие
    InteractionMode interaction_mode_ = InteractionMode::Zoom;
    bool is_zooming_ = false;
    bool is_panning_ = false;
    QPoint zoom_start_;
    QPoint zoom_end_;
    QPoint pan_last_pos_;

    std::vector<float> statics_curve_;

    // Горизонт
    std::vector<std::pair<double, double>> horizon_points_;
    bool has_horizon_ = false;

    // Для определения амплитуды при клике
    float getAmplitudeAt(int trace_idx, int sample_idx) const;
};

#endif // SEISMIC_VIEW_HPP
