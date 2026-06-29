#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstddef>
#include <vector>
#include <segyio/segy.h>

/// Заголовок одной трассы (размер по segy.h).
constexpr std::size_t kTraceHeaderSize = SEGY_TRACE_HEADER_SIZE;

/// Текстовый заголовок файла.
constexpr std::size_t kTextHeaderSize = SEGY_TEXT_HEADER_SIZE;

/// Бинарный заголовок файла.
constexpr std::size_t kBinaryHeaderSize = SEGY_BINARY_HEADER_SIZE;

/// Одна трасса: заголовок + сэмплы + исходный индекс для сохранения порядка при записи.
struct Trace {
    char header[kTraceHeaderSize] = {};
    std::vector<float> data;
    int original_index = -1;
};

/// Заголовки файла SEG-Y (текстовый + бинарный).
struct FileHeaders {
    char textual[kTextHeaderSize] = {};
    char binary[kBinaryHeaderSize] = {};
};

/// Метаданные потока: интервал сэмплинга (с), число сэмплов на трассу, число трасс.
struct StreamMetadata {
    float dt = 0.f;       /// интервал сэмплинга, секунды
    int n_samples = 0;
    int n_traces = 0;
};

/// Статическая коррекция для одной точки
struct StaticPoint {
    double x = 0.0;           // Координата X
    double y = 0.0;           // Координата Y
    double trace_num = 0.0;   // Номер трассы
    double value = 0.0;       // Значение статики (секунды)
};

/// Точка горизонта
typedef StaticPoint HorizonPoint;

#endif // TYPES_HPP
