// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/wavfile.h>

#include <cstring>
#include <cassert>
#include <algorithm>

namespace {

// Portable little-endian read/write helpers
inline bool read_u16_le(FILE* f, uint16_t* val) {
    uint8_t buf[2];
    if (fread(buf, 1, 2, f) != 2) return false;
    *val = static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8);
    return true;
}

inline bool read_u32_le(FILE* f, uint32_t* val) {
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) return false;
    *val = static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
    return true;
}

inline bool write_u16_le(FILE* f, uint16_t val) {
    uint8_t buf[2] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF)
    };
    return fwrite(buf, 1, 2, f) == 2;
}

inline bool write_u32_le(FILE* f, uint32_t val) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(val & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 24) & 0xFF)
    };
    return fwrite(buf, 1, 4, f) == 4;
}

// inline bool read_i16_le(FILE* f, int16_t* val) {
//     uint16_t uval;
//     if (!read_u16_le(f, &uval)) return false;
//     *val = static_cast<int16_t>(uval);
//     return true;
// }

inline bool write_i16_le(FILE* f, int16_t val) {
    return write_u16_le(f, static_cast<uint16_t>(val));
}

// inline bool read_f32_le(FILE* f, float* val) {
//     uint32_t uval;
//     if (!read_u32_le(f, &uval)) return false;
//     std::memcpy(val, &uval, 4);
//     return true;
// }

inline bool write_f32_le(FILE* f, float val) {
    uint32_t uval;
    std::memcpy(&uval, &val, 4);
    return write_u32_le(f, uval);
}

}  // anonymous namespace

phaseshift::wavfile::wavfile(int chunk_size_max) {
    assert(chunk_size_max > 0);
    m_chunk_size_max = chunk_size_max;
    m_chunk = new float[m_chunk_size_max];
    std::memset(&m_header, 0, sizeof(m_header));
    std::memset(&m_fmt, 0, sizeof(m_fmt));
    std::memset(&m_data, 0, sizeof(m_data));
}

void phaseshift::wavfile::close() {
    if (m_file_handle) {
        fclose(m_file_handle);
        m_file_handle = nullptr;
    }
    std::memset(&m_header, 0, sizeof(m_header));
    std::memset(&m_fmt, 0, sizeof(m_fmt));
    std::memset(&m_data, 0, sizeof(m_data));
}

phaseshift::wavfile::~wavfile() {
    close();
    if (m_chunk != nullptr) {
        delete[] m_chunk;
        m_chunk = nullptr;
    }
}

// Helper function to open and parse WAV header (portable, little-endian)
static bool open_wav_for_reading(const std::string& file_path, FILE** pfile,
                                  phaseshift::wav_header* header,
                                  phaseshift::wav_fmt_chunk* fmt,
                                  phaseshift::wav_data_chunk* data,
                                  long* data_start_pos) {
    *pfile = fopen(file_path.c_str(), "rb");
    if (!*pfile) return false;

    // Read RIFF header
    if (fread(header->riff, 1, 4, *pfile) != 4) { fclose(*pfile); *pfile = nullptr; return false; }
    if (!read_u32_le(*pfile, &header->file_size)) { fclose(*pfile); *pfile = nullptr; return false; }
    if (fread(header->wave, 1, 4, *pfile) != 4) { fclose(*pfile); *pfile = nullptr; return false; }

    if (std::memcmp(header->riff, "RIFF", 4) != 0 || std::memcmp(header->wave, "WAVE", 4) != 0) {
        fclose(*pfile); *pfile = nullptr; return false;
    }

    // Find and read fmt chunk (skip unknown chunks)
    bool found_fmt = false;
    while (!found_fmt && !feof(*pfile)) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, *pfile) != 4) break;
        if (!read_u32_le(*pfile, &chunk_size)) break;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            std::memcpy(fmt->fmt, chunk_id, 4);
            fmt->chunk_size = chunk_size;

            // Read fmt chunk fields individually (portable)
            if (!read_u16_le(*pfile, &fmt->audio_format) ||
                !read_u16_le(*pfile, &fmt->num_channels) ||
                !read_u32_le(*pfile, &fmt->sample_rate) ||
                !read_u32_le(*pfile, &fmt->byte_rate) ||
                !read_u16_le(*pfile, &fmt->block_align) ||
                !read_u16_le(*pfile, &fmt->bits_per_sample)) {
                fclose(*pfile); *pfile = nullptr; return false;
            }

            // Skip extra fmt bytes if any (for extended formats)
            if (chunk_size > 16) {
                fseek(*pfile, chunk_size - 16, SEEK_CUR);
            }
            found_fmt = true;
        } else {
            fseek(*pfile, chunk_size, SEEK_CUR);
        }
    }
    if (!found_fmt) { fclose(*pfile); *pfile = nullptr; return false; }

    // Validate format
    if (fmt->audio_format != phaseshift::wav::FORMAT_PCM &&
        fmt->audio_format != phaseshift::wav::FORMAT_IEEE_FLOAT) {
        fclose(*pfile); *pfile = nullptr; return false;
    }

    // Find data chunk
    bool found_data = false;
    while (!found_data && !feof(*pfile)) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, *pfile) != 4) break;
        if (!read_u32_le(*pfile, &chunk_size)) break;

        if (std::memcmp(chunk_id, "data", 4) == 0) {
            std::memcpy(data->data, chunk_id, 4);
            data->data_size = chunk_size;
            *data_start_pos = ftell(*pfile);
            found_data = true;
        } else {
            fseek(*pfile, chunk_size, SEEK_CUR);
        }
    }
    if (!found_data) { fclose(*pfile); *pfile = nullptr; return false; }

    return true;
}

float phaseshift::wavfile_reader::get_fs(const std::string& file_path) {
    assert(file_path != "");

    FILE* file = nullptr;
    wav_header header;
    wav_fmt_chunk fmt;
    wav_data_chunk data;
    long data_pos;

    if (!open_wav_for_reading(file_path, &file, &header, &fmt, &data, &data_pos)) {
        return -1.0f;
    }

    float fs = static_cast<float>(fmt.sample_rate);
    fclose(file);
    return fs;
}

int phaseshift::wavfile_reader::get_nbchannels(const std::string& file_path) {
    assert(file_path != "");

    FILE* file = nullptr;
    wav_header header;
    wav_fmt_chunk fmt;
    wav_data_chunk data;
    long data_pos;

    if (!open_wav_for_reading(file_path, &file, &header, &fmt, &data, &data_pos)) {
        return -1;
    }

    int nbchannels = fmt.num_channels;
    fclose(file);
    return nbchannels;
}

int phaseshift::wavfile_reader::get_nbframes(const std::string& file_path) {
    assert(file_path != "");

    FILE* file = nullptr;
    wav_header header;
    wav_fmt_chunk fmt;
    wav_data_chunk data;
    long data_pos;

    if (!open_wav_for_reading(file_path, &file, &header, &fmt, &data, &data_pos)) {
        return -1;
    }

    int nbframes = data.data_size / (fmt.num_channels * fmt.bits_per_sample / 8);
    fclose(file);
    return nbframes;
}

int phaseshift::wavfile_reader::get_bits_per_sample(const std::string& file_path) {
    assert(file_path != "");

    FILE* file = nullptr;
    wav_header header;
    wav_fmt_chunk fmt;
    wav_data_chunk data;
    long data_pos;

    if (!open_wav_for_reading(file_path, &file, &header, &fmt, &data, &data_pos)) {
        return -1;
    }

    int bits = fmt.bits_per_sample;
    fclose(file);
    return bits;
}

phaseshift::wavfile_reader::wavfile_reader(int chunk_size_max)
    : phaseshift::wavfile(chunk_size_max) {
}

float phaseshift::wavfile_reader::convert_to_float(const uint8_t* sample_ptr) const {
    float sample = 0.0f;

    if (m_fmt.audio_format == wav::FORMAT_IEEE_FLOAT && m_fmt.bits_per_sample == 32) {
        // Little-endian float
        uint32_t uval = static_cast<uint32_t>(sample_ptr[0]) |
                        (static_cast<uint32_t>(sample_ptr[1]) << 8) |
                        (static_cast<uint32_t>(sample_ptr[2]) << 16) |
                        (static_cast<uint32_t>(sample_ptr[3]) << 24);
        std::memcpy(&sample, &uval, 4);
    } else if (m_fmt.audio_format == wav::FORMAT_PCM && m_fmt.bits_per_sample == 16) {
        // Little-endian signed 16-bit
        int16_t s16 = static_cast<int16_t>(
            static_cast<uint16_t>(sample_ptr[0]) |
            (static_cast<uint16_t>(sample_ptr[1]) << 8)
        );
        sample = s16 / 32768.0f;
    }

    return sample;
}

phaseshift::wavfile_reader* phaseshift::wavfile_reader_builder::open(const std::string& file_path, int chunk_size_max, int channel_id) {
    phaseshift::wavfile_reader_builder builder;
    builder.set_file_path(file_path);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_channel_id(channel_id);
    return builder.open();
}

phaseshift::wavfile_reader* phaseshift::wavfile_reader_builder::build(phaseshift::wavfile_reader* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    pab->m_file_path = m_file_path;

    if (!open_wav_for_reading(m_file_path, &pab->m_file_handle, &pab->m_header,
                               &pab->m_fmt, &pab->m_data, &pab->m_data_start_pos)) {
        delete pab;
        return nullptr;
    }

    pab->m_fs = static_cast<float>(pab->m_fmt.sample_rate);
    pab->m_nbchannels = pab->m_fmt.num_channels;
    pab->m_channel_id = m_channel_id;
    pab->m_bits_per_sample = pab->m_fmt.bits_per_sample;

    return pab;
}


phaseshift::wavfile_writer::wavfile_writer(int chunk_size_max)
    : phaseshift::wavfile(chunk_size_max) {
}

phaseshift::wavfile_writer::~wavfile_writer() {
    close();
}

void phaseshift::wavfile_writer::close() {
    if (m_file_handle) {
        finalize_header();
        fclose(m_file_handle);
        m_file_handle = nullptr;
    }
}

bool phaseshift::wavfile_writer::write_sample(float sample) {
    if (m_fmt.audio_format == wav::FORMAT_IEEE_FLOAT && m_fmt.bits_per_sample == 32) {
        if (!write_f32_le(m_file_handle, sample)) return false;
        m_written_bytes += 4;
    } else if (m_fmt.audio_format == wav::FORMAT_PCM && m_fmt.bits_per_sample == 16) {
        sample = std::max(-1.0f, std::min(1.0f, sample));
        int16_t s16 = static_cast<int16_t>(sample * 32767.0f);
        if (!write_i16_le(m_file_handle, s16)) return false;
        m_written_bytes += 2;
    } else {
        return false;
    }

    return true;
}

void phaseshift::wavfile_writer::finalize_header() {
    // Calculate sizes
    uint32_t data_size = m_written_bytes;
    uint32_t file_size = 4 + (8 + 16) + (8 + data_size);  // "WAVE" + fmt chunk + data chunk

    // Seek to beginning and write header
    fseek(m_file_handle, 0, SEEK_SET);

    // RIFF header
    fwrite("RIFF", 1, 4, m_file_handle);
    write_u32_le(m_file_handle, file_size);
    fwrite("WAVE", 1, 4, m_file_handle);

    // fmt chunk
    fwrite("fmt ", 1, 4, m_file_handle);
    write_u32_le(m_file_handle, 16);  // chunk size
    write_u16_le(m_file_handle, m_fmt.audio_format);
    write_u16_le(m_file_handle, m_fmt.num_channels);
    write_u32_le(m_file_handle, m_fmt.sample_rate);
    write_u32_le(m_file_handle, m_fmt.byte_rate);
    write_u16_le(m_file_handle, m_fmt.block_align);
    write_u16_le(m_file_handle, m_fmt.bits_per_sample);

    // data chunk header
    fwrite("data", 1, 4, m_file_handle);
    write_u32_le(m_file_handle, data_size);
}

phaseshift::wavfile_writer* phaseshift::wavfile_writer_builder::build(phaseshift::wavfile_writer* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    assert(m_fs > 0 && "fs has not been set");
    pab->m_file_path = m_file_path;
    pab->m_fs = m_fs;
    pab->m_bits_per_sample = m_bits_per_sample;

    pab->m_length = 0;
    pab->m_written_bytes = 0;
    pab->m_nbchannels = m_nbchannels;

    // Setup fmt chunk info
    std::memcpy(pab->m_fmt.fmt, "fmt ", 4);
    pab->m_fmt.chunk_size = 16;
    pab->m_fmt.audio_format = (m_use_float && m_bits_per_sample == 32) ? wav::FORMAT_IEEE_FLOAT : wav::FORMAT_PCM;
    pab->m_fmt.num_channels = m_nbchannels;
    pab->m_fmt.sample_rate = static_cast<uint32_t>(m_fs);
    pab->m_fmt.bits_per_sample = m_bits_per_sample;
    pab->m_fmt.block_align = m_nbchannels * m_bits_per_sample / 8;
    pab->m_fmt.byte_rate = pab->m_fmt.sample_rate * pab->m_fmt.block_align;

    // Open file
    pab->m_file_handle = fopen(m_file_path.c_str(), "wb");
    if (pab->m_file_handle == nullptr) {
        delete pab;
        return nullptr;
    }

    // Write initial header (will be updated on close)
    fwrite("RIFF", 1, 4, pab->m_file_handle);
    write_u32_le(pab->m_file_handle, 0);  // file size placeholder
    fwrite("WAVE", 1, 4, pab->m_file_handle);

    // fmt chunk
    fwrite("fmt ", 1, 4, pab->m_file_handle);
    write_u32_le(pab->m_file_handle, 16);
    write_u16_le(pab->m_file_handle, pab->m_fmt.audio_format);
    write_u16_le(pab->m_file_handle, pab->m_fmt.num_channels);
    write_u32_le(pab->m_file_handle, pab->m_fmt.sample_rate);
    write_u32_le(pab->m_file_handle, pab->m_fmt.byte_rate);
    write_u16_le(pab->m_file_handle, pab->m_fmt.block_align);
    write_u16_le(pab->m_file_handle, pab->m_fmt.bits_per_sample);

    // data chunk header
    fwrite("data", 1, 4, pab->m_file_handle);
    write_u32_le(pab->m_file_handle, 0);  // data size placeholder

    pab->m_data_start_pos = ftell(pab->m_file_handle);

    return pab;
}

phaseshift::wavfile_writer* phaseshift::wavfile_writer_builder::open(const std::string& file_path, float fs, int chunk_size_max, int nbchannels, int bits_per_sample, bool use_float) {
    wavfile_writer_builder builder;
    builder.set_file_path(file_path);
    builder.set_fs(fs);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_nbchannels(nbchannels);
    builder.set_bits_per_sample(bits_per_sample);
    builder.set_use_float(use_float);
    return builder.open();
}