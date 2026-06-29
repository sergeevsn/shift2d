#ifndef SEGY_WRITER_HPP
#define SEGY_WRITER_HPP

#include <string>
#include "types.hpp"
#include <segyio/segy.h>

/// Запись SEG-Y: запись заголовков файла и трасс.
class SegyWriter {
public:
    SegyWriter();
    ~SegyWriter();

    SegyWriter(const SegyWriter&) = delete;
    SegyWriter& operator=(const SegyWriter&) = delete;

    /// Открыть файл на запись. Кидает std::runtime_error при ошибке.
    void open(const std::string& path);

    void close();

    bool isOpen() const;

    /// Записать текстовый и бинарный заголовки. В бинарном подставляются
    /// out_dt (секунды) и out_n_samples. Заголовки копируются из headers.
    void writeHeaders(const FileHeaders& headers,
                      float out_dt,
                      int out_n_samples,
                      int out_n_traces);

    /// Записать одну трассу в позицию out_trace_index (0, 1, 2, ...) в выходном файле.
    /// В заголовок трассы подставляются out_n_samples и out_dt (мкс в trace header).
    void writeTrace(const Trace& trace, int out_trace_index, int out_n_samples, float out_dt);

private:
    segy_file* fp_ = nullptr;
    int format_ = SEGY_IEEE_FLOAT_4_BYTE;  // формат записи сэмплов
    long trace0_ = 0;
    int trace_bsize_ = 0;
};

#endif // SEGY_WRITER_HPP
