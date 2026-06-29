#include "segy_writer.hpp"

#include <cstring>
#include <cmath>
#include <stdexcept>
#include <vector>

SegyWriter::SegyWriter() = default;

SegyWriter::~SegyWriter() {
    close();
}

void SegyWriter::open(const std::string& path) {
    close();
    segy_file* fp = segy_open(path.c_str(), "w+b");
    if (!fp) {
        throw std::runtime_error("segy_writer: cannot create file: " + path);
    }
    fp_ = fp;
}

void SegyWriter::close() {
    if (fp_) {
        segy_close(fp_);
        fp_ = nullptr;
    }
}

bool SegyWriter::isOpen() const {
    return fp_ != nullptr;
}

void SegyWriter::writeHeaders(const FileHeaders& headers, float out_dt,
                              int out_n_samples, int out_n_traces) {
    if (!fp_)
        throw std::runtime_error("segy_writer: file not open");

    if (segy_write_textheader(fp_, 0, headers.textual) != SEGY_OK)
        throw std::runtime_error("segy_writer: failed to write text header");

    char bin_header[kBinaryHeaderSize];
    std::memcpy(bin_header, headers.binary, kBinaryHeaderSize);

    int dt_micro = static_cast<int>(std::round(out_dt * 1e6f));
    segy_set_bfield(bin_header, SEGY_BIN_INTERVAL, dt_micro);
    segy_set_bfield(bin_header, SEGY_BIN_INTERVAL_ORIG, dt_micro);
    segy_set_bfield(bin_header, SEGY_BIN_SAMPLES, out_n_samples);
    segy_set_bfield(bin_header, SEGY_BIN_SAMPLES_ORIG, out_n_samples);
    segy_set_bfield(bin_header, SEGY_BIN_FORMAT, format_);
    segy_set_bfield(bin_header, SEGY_BIN_TRACES, out_n_traces);

    if (segy_write_binheader(fp_, bin_header) != SEGY_OK)
        throw std::runtime_error("segy_writer: failed to write binary header");

    trace0_ = segy_trace0(bin_header);
    trace_bsize_ = segy_trsize(format_, out_n_samples);
    if (trace_bsize_ <= 0) {
        trace_bsize_ = segy_trace_bsize(out_n_samples);
    }
}

void SegyWriter::writeTrace(const Trace& trace, int out_trace_index,
                            int out_n_samples, float out_dt) {
    if (!fp_)
        throw std::runtime_error("segy_writer: file not open");

    char trace_header[kTraceHeaderSize];
    std::memcpy(trace_header, trace.header, kTraceHeaderSize);

    int interval_us = static_cast<int>(std::round(out_dt * 1e6f));
    segy_set_field(trace_header, SEGY_TR_SAMPLE_INTER, interval_us);
    segy_set_field(trace_header, SEGY_TR_SAMPLE_COUNT, out_n_samples);

    const int trace_size = segy_trsize(format_, out_n_samples);
    std::vector<char> raw(static_cast<std::size_t>(trace_size));
    if (out_n_samples > 0) {
        float* dst = reinterpret_cast<float*>(raw.data());
        for (int i = 0; i < out_n_samples; ++i) {
            dst[i] = (i < static_cast<int>(trace.data.size())) ? trace.data[static_cast<std::size_t>(i)] : 0.0f;
        }
    }

    if (segy_from_native(format_, static_cast<long long>(out_n_samples), raw.data()) != SEGY_OK)
        throw std::runtime_error("segy_writer: from_native failed");

    if (segy_write_traceheader(fp_, out_trace_index, trace_header, trace0_, trace_bsize_) != SEGY_OK)
        throw std::runtime_error("segy_writer: failed to write trace header");

    if (segy_writetrace(fp_, out_trace_index, raw.data(), trace0_, trace_bsize_) != SEGY_OK)
        throw std::runtime_error("segy_writer: failed to write trace data");
}
