#include "segy_reader.hpp"

#include <cstring>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
int bytesPerSampleForFormat(int format) {
    switch (format & 0xFF) {
        case SEGY_IBM_FLOAT_4_BYTE:
        case SEGY_SIGNED_INTEGER_4_BYTE:
        case SEGY_IEEE_FLOAT_4_BYTE:
            return 4;
        case SEGY_IEEE_FLOAT_8_BYTE:
        case SEGY_UNSIGNED_INTEGER_8_BYTE:
            return 8;
        case SEGY_SIGNED_SHORT_2_BYTE:
        case SEGY_UNSIGNED_SHORT_2_BYTE:
            return 2;
        default:
            return 4;
    }
}

int32_t readInt32BigEndian(const char* data, int offset) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data + offset);
    return static_cast<int32_t>(
        (static_cast<uint32_t>(bytes[0]) << 24) |
        (static_cast<uint32_t>(bytes[1]) << 16) |
        (static_cast<uint32_t>(bytes[2]) << 8) |
        static_cast<uint32_t>(bytes[3]));
}
}  // namespace

SegyReader::SegyReader() = default;

SegyReader::~SegyReader() {
    close();
}

void SegyReader::open(const std::string& path) {
    close();
    segy_file* fp = segy_open(path.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("segy_reader: cannot open file: " + path);
    }
    fp_ = fp;

    char bin_header[kBinaryHeaderSize];
    if (segy_binheader(fp_, bin_header) != SEGY_OK) {
        segy_close(fp_);
        fp_ = nullptr;
        throw std::runtime_error("segy_reader: failed to read binary header");
    }

    int samples = segy_samples(bin_header);
    if (samples <= 0) {
        segy_close(fp_);
        fp_ = nullptr;
        throw std::runtime_error("segy_reader: invalid sample count");
    }

    format_ = segy_format(bin_header);
    if (format_ <= 0) {
        format_ = SEGY_IBM_FLOAT_4_BYTE;
    }

    int interval_us = 0;
    if (segy_get_bfield(bin_header, SEGY_BIN_INTERVAL, &interval_us) == SEGY_OK && interval_us > 0) {
        metadata_.dt = static_cast<float>(interval_us) * 1e-6f;
    } else {
        float fallback = 0.0f;
        segy_sample_interval(fp_, 0.0f, &fallback);
        metadata_.dt = fallback > 0.0f ? fallback : 0.002f;
    }

    long trace0 = segy_trace0(bin_header);
    int trace_bsize = segy_trsize(format_, samples);
    if (trace_bsize <= 0) {
        trace_bsize = segy_trace_bsize(samples);
    }

    int trace_count = 0;
    if (segy_traces(fp_, &trace_count, trace0, trace_bsize) != SEGY_OK) {
        segy_close(fp_);
        fp_ = nullptr;
        throw std::runtime_error("segy_reader: failed to determine trace count");
    }

    metadata_.n_samples = samples;
    metadata_.n_traces = trace_count;
    trace0_ = trace0;
    trace_bsize_ = trace_bsize;
    bytes_per_sample_ = bytesPerSampleForFormat(format_);
}

void SegyReader::close() {
    if (fp_) {
        segy_close(fp_);
        fp_ = nullptr;
    }
}

bool SegyReader::isOpen() const {
    return fp_ != nullptr;
}

int SegyReader::traceCount() const {
    return metadata_.n_traces;
}

int SegyReader::sampleCount() const {
    return metadata_.n_samples;
}

FileHeaders SegyReader::fileHeaders() const {
    if (!fp_)
        return {};
    FileHeaders h;
    if (segy_read_textheader(fp_, h.textual) != SEGY_OK)
        std::memset(h.textual, 0, kTextHeaderSize);
    if (segy_binheader(fp_, h.binary) != SEGY_OK)
        std::memset(h.binary, 0, kBinaryHeaderSize);
    return h;
}

Trace SegyReader::readTrace(int idx) {
    if (!fp_ || idx < 0 || idx >= metadata_.n_traces)
        throw std::runtime_error("segy_reader: readTrace invalid index");
    Trace t;
    t.original_index = idx;

    if (segy_traceheader(fp_, idx, t.header, trace0_, trace_bsize_) != SEGY_OK)
        throw std::runtime_error("segy_reader: read trace header failed");

    const int n = metadata_.n_samples;
    const int trace_size = segy_trsize(format_, n);
    std::vector<char> raw(static_cast<std::size_t>(trace_size));
    if (segy_readtrace(fp_, idx, raw.data(), trace0_, trace_bsize_) != SEGY_OK)
        throw std::runtime_error("segy_reader: read trace data failed");

    if (segy_to_native(format_, static_cast<long long>(n), raw.data()) != SEGY_OK)
        throw std::runtime_error("segy_reader: to_native failed");

    t.data.resize(static_cast<std::size_t>(n));
    if (bytes_per_sample_ == 4) {
        const float* src = reinterpret_cast<const float*>(raw.data());
        for (int i = 0; i < n; ++i) {
            t.data[static_cast<std::size_t>(i)] = src[i];
        }
    } else if (bytes_per_sample_ == 8) {
        const double* src = reinterpret_cast<const double*>(raw.data());
        for (int i = 0; i < n; ++i) {
            t.data[static_cast<std::size_t>(i)] = static_cast<float>(src[i]);
        }
    } else if (bytes_per_sample_ == 2) {
        const int16_t* src = reinterpret_cast<const int16_t*>(raw.data());
        for (int i = 0; i < n; ++i) {
            t.data[static_cast<std::size_t>(i)] = static_cast<float>(src[i]);
        }
    } else {
        throw std::runtime_error("segy_reader: unsupported sample format");
    }
    return t;
}

double SegyReader::getTraceHeaderValue(int idx, int byte_pos) const {
    if (!fp_ || idx < 0 || idx >= metadata_.n_traces)
        return 0.0;

    char trace_header[kTraceHeaderSize];
    if (segy_traceheader(fp_, idx, trace_header, trace0_, trace_bsize_) != SEGY_OK)
        return 0.0;

    int offset = byte_pos - 1;
    if (offset < 0 || offset + 4 > static_cast<int>(sizeof(trace_header)))
        return 0.0;

    return static_cast<double>(readInt32BigEndian(trace_header, offset));
}
