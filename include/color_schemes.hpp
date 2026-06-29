#ifndef COLOR_SCHEMES_HPP
#define COLOR_SCHEMES_HPP

#include <QColor>
#include <QString>
#include <vector>

enum class ColorMap {
    Grayscale = 0,
    Viridis,
    RedBlue,
    PetrelClassic,
    Kingdom,
    Seismic
};

/// Стоп для палитры
struct ColorStop {
    float position;  // 0.0 - 1.0
    QColor color;

    ColorStop(float pos, const QColor& col) : position(pos), color(col) {}
};

/// Цветовая схема
class ColorScheme {
public:
    QString name;
    std::vector<ColorStop> stops;
    float contrast = 1.0f;
    float brightness = 0.0f;

    ColorScheme() = default;
    explicit ColorScheme(const QString& n) : name(n) {}

    void addStop(float pos, const QColor& color);
    QColor getColor(float value) const;
};

/// Утилиты для работы с цветовыми схемами
class ColorSchemes {
public:
    static QColor interpolateFromPalette(const std::vector<ColorStop>& stops, float value);

    /// Получить цвет из палитры по нормализованному значению [0, 1]
    static QColor getColor(float normalizedValue, const QString& schemeName);
    static QColor getColor(float normalizedValue, ColorMap cmap);

    /// Получить палитру для seismic данных (симметричная относительно 0)
    /// amplitude - амплитуда
    /// clip - значение клиппинга (амплитуда >= clip будет иметь максимальный цвет)
    static QColor getSeismicColor(float amplitude, float clip, ColorMap cmap);

    /// Получить список доступных схем
    static QStringList getAvailableSchemes();

    /// Получить имя схемы по enum
    static QString getSchemeName(ColorMap cmap);
    static ColorMap getSchemeFromName(const QString& name);

private:
    static std::vector<ColorStop> getGrayStops();
    static std::vector<ColorStop> getViridisStops();
    static std::vector<ColorStop> getRedBlueStops();
    static std::vector<ColorStop> getPetrelClassicStops();
    static std::vector<ColorStop> getKingdomStops();
    static std::vector<ColorStop> getSeismicStops();

    static QColor interpolateColor(const QColor& c1, const QColor& c2, float t);

    static float normalizeValue(float v);
};

#endif // COLOR_SCHEMES_HPP
