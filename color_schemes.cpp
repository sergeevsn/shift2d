#include "color_schemes.hpp"

#include <algorithm>
#include <cmath>

void ColorScheme::addStop(float pos, const QColor& color) {
    stops.emplace_back(pos, color);
    std::sort(stops.begin(), stops.end(),
              [](const ColorStop& a, const ColorStop& b) { return a.position < b.position; });
}

QColor ColorScheme::getColor(float value) const {
    if (stops.empty()) return QColor(0, 0, 0);
    if (stops.size() == 1) return stops[0].color;

    value = std::max(0.0f, std::min(1.0f, value));

    // Применяем контраст и яркость
    value = 0.5f + contrast * (value - 0.5f) + brightness;
    value = std::max(0.0f, std::min(1.0f, value));

    return ColorSchemes::interpolateFromPalette(stops, value);
}

QColor ColorSchemes::getColor(float normalizedValue, const QString& schemeName) {
    return getColor(normalizedValue, getSchemeFromName(schemeName));
}

QColor ColorSchemes::getColor(float normalizedValue, ColorMap cmap) {
    normalizedValue = normalizeValue(normalizedValue);

    switch (cmap) {
        case ColorMap::Grayscale:
            return interpolateFromPalette(getGrayStops(), normalizedValue);
        case ColorMap::Viridis:
            return interpolateFromPalette(getViridisStops(), normalizedValue);
        case ColorMap::RedBlue:
            return interpolateFromPalette(getRedBlueStops(), normalizedValue);
        case ColorMap::PetrelClassic:
            return interpolateFromPalette(getPetrelClassicStops(), normalizedValue);
        case ColorMap::Kingdom:
            return interpolateFromPalette(getKingdomStops(), normalizedValue);
        case ColorMap::Seismic:
            return interpolateFromPalette(getSeismicStops(), normalizedValue);
    }
    return QColor(0, 0, 0);
}

QColor ColorSchemes::getSeismicColor(float amplitude, float clip, ColorMap cmap) {
    // Нормализуем амплитуду в диапазон [0, 1] для палитры
    // При этом симметрично относительно 0:
    // -clip -> 0, 0 -> 0.5, +clip -> 1
    float normalized = (amplitude / clip + 1.0f) * 0.5f;
    return getColor(normalized, cmap);
}

QStringList ColorSchemes::getAvailableSchemes() {
    return QStringList{"Grayscale", "Viridis", "Red/Blue", "Petrel Classic", "Kingdom", "Seismic"};
}

QString ColorSchemes::getSchemeName(ColorMap cmap) {
    switch (cmap) {
        case ColorMap::Grayscale: return "Grayscale";
        case ColorMap::Viridis: return "Viridis";
        case ColorMap::RedBlue: return "Red/Blue";
        case ColorMap::PetrelClassic: return "Petrel Classic";
        case ColorMap::Kingdom: return "Kingdom";
        case ColorMap::Seismic: return "Seismic";
    }
    return "Grayscale";
}

ColorMap ColorSchemes::getSchemeFromName(const QString& name) {
    if (name == "Grayscale") return ColorMap::Grayscale;
    if (name == "Viridis") return ColorMap::Viridis;
    if (name == "Red/Blue" || name == "red_blue") return ColorMap::RedBlue;
    if (name == "Petrel Classic" || name == "petrel_classic") return ColorMap::PetrelClassic;
    if (name == "Kingdom" || name == "kingdom") return ColorMap::Kingdom;
    if (name == "Seismic" || name == "seismic") return ColorMap::Seismic;
    return ColorMap::Grayscale;
}

std::vector<ColorStop> ColorSchemes::getGrayStops() {
    return {
        {0.00f, QColor(0, 0, 0)},       // чёрный
        {1.00f, QColor(255, 255, 255)}  // белый
    };
}

std::vector<ColorStop> ColorSchemes::getViridisStops() {
    return {
        {0.00f, QColor(68, 1, 84)},      // темно-фиолетовый
        {0.10f, QColor(72, 35, 116)},    // фиолетовый
        {0.20f, QColor(64, 67, 135)},    // сине-фиолетовый
        {0.30f, QColor(52, 94, 141)},    // синий
        {0.40f, QColor(41, 120, 142)},   // голубой
        {0.50f, QColor(32, 144, 140)},   // сине-зеленый
        {0.60f, QColor(34, 167, 132)},   // зеленый
        {0.70f, QColor(37, 188, 121)},   // желто-зеленый
        {0.80f, QColor(65, 204, 103)},   // ярко-зеленый
        {0.90f, QColor(119, 216, 67)},   // светло-зеленый
        {1.00f, QColor(253, 231, 37)}    // желтый
    };
}

std::vector<ColorStop> ColorSchemes::getRedBlueStops() {
    return {
        {0.00f, QColor(0, 0, 255)},       // синий
        {0.50f, QColor(255, 255, 255)},  // белый центр
        {1.00f, QColor(255, 0, 0)}       // красный
    };
}

std::vector<ColorStop> ColorSchemes::getPetrelClassicStops() {
    return {
        {0.00f, QColor(0, 0, 90)},        // глубокий синий
        {0.15f, QColor(0, 60, 160)},      // синий
        {0.25f, QColor(0, 120, 220)},    // голубой
        {0.35f, QColor(80, 180, 255)},   // светло-голубой
        {0.45f, QColor(200, 230, 255)},  // очень светло-голубой
        {0.50f, QColor(255, 255, 255)},   // белый центр
        {0.55f, QColor(255, 230, 200)},   // очень светло-оранжевый
        {0.65f, QColor(255, 180, 80)},    // светло-оранжевый
        {0.75f, QColor(220, 120, 0)},    // оранжевый
        {0.85f, QColor(160, 60, 0)},      // красно-оранжевый
        {1.00f, QColor(90, 0, 0)}         // глубокий красный
    };
}

std::vector<ColorStop> ColorSchemes::getKingdomStops() {
    return {
        {0.00f, QColor(20, 20, 120)},     // темно-синий с оттенком
        {0.12f, QColor(0, 80, 180)},      // синий
        {0.25f, QColor(0, 140, 240)},    // ярко-синий
        {0.37f, QColor(100, 200, 255)},   // голубой
        {0.45f, QColor(180, 240, 255)},  // светло-голубой
        {0.50f, QColor(248, 248, 248)},  // почти белый
        {0.55f, QColor(255, 240, 180)},   // светло-желтый
        {0.63f, QColor(255, 200, 100)},   // желто-оранжевый
        {0.75f, QColor(240, 140, 0)},    // оранжевый
        {0.88f, QColor(180, 80, 0)},      // красно-оранжевый
        {1.00f, QColor(120, 20, 20)}      // темно-красный
    };
}

std::vector<ColorStop> ColorSchemes::getSeismicStops() {
    // Классическая сейсмическая палитра: красное - положительные, синее - отрицательные
    return {
        {0.00f, QColor(0, 0, 139)},       // темно-синий
        {0.20f, QColor(0, 0, 255)},       // синий
        {0.40f, QColor(135, 206, 250)}, // светло-голубой
        {0.50f, QColor(255, 255, 255)},   // белый
        {0.60f, QColor(255, 200, 150)}, // светло-оранжевый
        {0.80f, QColor(255, 0, 0)},       // красный
        {1.00f, QColor(139, 0, 0)}        // темно-красный
    };
}

QColor ColorSchemes::interpolateFromPalette(const std::vector<ColorStop>& stops, float value) {
    if (stops.empty()) return QColor(0, 0, 0);
    if (stops.size() == 1) return stops[0].color;

    value = normalizeValue(value);

    // Найти соседние точки
    for (std::size_t i = 0; i < stops.size() - 1; ++i) {
        if (value >= stops[i].position && value <= stops[i + 1].position) {
            float range = stops[i + 1].position - stops[i].position;
            if (range < 1e-6f) return stops[i].color;

            float t = (value - stops[i].position) / range;
            return interpolateColor(stops[i].color, stops[i + 1].color, t);
        }
    }

    // Значение вне диапазона
    if (value < stops[0].position) return stops[0].color;
    return stops.back().color;
}

QColor ColorSchemes::interpolateColor(const QColor& c1, const QColor& c2, float t) {
    t = normalizeValue(t);

    int r = static_cast<int>(c1.red() * (1.0f - t) + c2.red() * t + 0.5f);
    int g = static_cast<int>(c1.green() * (1.0f - t) + c2.green() * t + 0.5f);
    int b = static_cast<int>(c1.blue() * (1.0f - t) + c2.blue() * t + 0.5f);

    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
}

float ColorSchemes::normalizeValue(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}
