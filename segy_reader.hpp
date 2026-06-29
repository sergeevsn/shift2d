#ifndef SEGY_READER_HPP
#define SEGY_READER_HPP

#include <string>
#include "types.hpp"
#include <segyio/segy.h>

/// Чтение SEG-Y: открытие файла, чтение заголовков и трасс по индексу.
class SegyReader {
public:
    SegyReader();
    ~SegyReader();

    SegyReader(const SegyReader&) = delete;
    SegyReader& operator=(const SegyReader&) = delete;

    /// Открыть файл для чтения. Кидает std::runtime_error при ошибке.
    void open(const std::string& path);

    void close();

    bool isOpen() const;

    int traceCount() const;
    int sampleCount() const;
    float dt() const { return metadata_.dt; }
    const StreamMetadata& metadata() const { return metadata_; }

    /// Заголовки файла (текстовый + бинарный). Вызывать после open().
    FileHeaders fileHeaders() const;

    /// Прочитать трассу по индексу [0 .. traceCount()-1]. Возвращает Trace с original_index = idx.
    Trace readTrace(int idx);

    /// Прочитать значение из заголовка трассы по произвольному байту (1-нумерованный).
    double getTraceHeaderValue(int idx, int byte_pos) const;

private:
    segy_file* fp_ = nullptr;
    StreamMetadata metadata_;
    int format_ = 0;  // SEGY_FORMAT для segy_to_native
    long trace0_ = 0;
    int trace_bsize_ = 0;
    int bytes_per_sample_ = 4;
};

#endif // SEGY_READER_HPP
