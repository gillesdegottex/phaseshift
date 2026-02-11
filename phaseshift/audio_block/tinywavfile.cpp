// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/tinywavfile.h>

#include <cstring>
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <limits>

phaseshift::tinywavfile::tinywavfile(int chunk_size_max) {
    assert(chunk_size_max > 0);
    m_chunk_size_max = chunk_size_max;
    m_chunk = new float[m_chunk_size_max];
    std::memset(&m_tw, 0, sizeof(m_tw));
}

void phaseshift::tinywavfile::close() {
    // Note: tinywav_close_read/write handles null file pointer
    std::memset(&m_tw, 0, sizeof(m_tw));
}

phaseshift::tinywavfile::~tinywavfile() {
    close();
    if (m_chunk != nullptr) {
        delete[] m_chunk;
        m_chunk = nullptr;
    }
}

// Helper function to open and read WAV header info using tinywav
static bool open_wav_for_info(const std::string& file_path, TinyWav* tw) {
    if (tinywav_open_read(tw, file_path.c_str(), TW_INTERLEAVED) != 0) {
        return false;
    }
    return true;
}

float phaseshift::tinywavfile_reader::get_fs(const std::string& file_path) {
    assert(file_path != "");

    TinyWav tw;
    if (!open_wav_for_info(file_path, &tw)) {
        return -1.0f;
    }

    float fs = static_cast<float>(tw.h.SampleRate);
    tinywav_close_read(&tw);
    return fs;
}

int phaseshift::tinywavfile_reader::get_nbchannels(const std::string& file_path) {
    assert(file_path != "");

    TinyWav tw;
    if (!open_wav_for_info(file_path, &tw)) {
        return -1;
    }

    int nbchannels = tw.numChannels;
    tinywav_close_read(&tw);
    return nbchannels;
}

int phaseshift::tinywavfile_reader::get_nbframes(const std::string& file_path) {
    assert(file_path != "");

    TinyWav tw;
    if (!open_wav_for_info(file_path, &tw)) {
        return -1;
    }

    int nbframes = tw.numFramesInHeader;
    tinywav_close_read(&tw);
    return nbframes;
}

int phaseshift::tinywavfile_reader::get_bits_per_sample(const std::string& file_path) {
    assert(file_path != "");

    TinyWav tw;
    if (!open_wav_for_info(file_path, &tw)) {
        return -1;
    }

    int bits = tw.h.BitsPerSample;
    tinywav_close_read(&tw);
    return bits;
}

phaseshift::tinywavfile_reader::tinywavfile_reader(int chunk_size_max)
    : phaseshift::tinywavfile(chunk_size_max) {
}

phaseshift::tinywavfile_reader* phaseshift::tinywavfile_reader_builder::open(const std::string& file_path, int chunk_size_max, int channel_id) {
    phaseshift::tinywavfile_reader_builder builder;
    builder.set_file_path(file_path);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_channel_id(channel_id);
    return builder.open();
}

phaseshift::tinywavfile_reader* phaseshift::tinywavfile_reader_builder::open(const std::string& file_path, int chunk_size_max) {
    phaseshift::tinywavfile_reader_builder builder;
    builder.set_file_path(file_path);
    builder.set_chunk_size_max(chunk_size_max);
    // channel_id not set - used for interleaved reading of all channels
    return builder.open();
}

phaseshift::tinywavfile_reader* phaseshift::tinywavfile_reader_builder::build(phaseshift::tinywavfile_reader* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    pab->m_file_path = m_file_path;

    if (tinywav_open_read(&pab->m_tw, m_file_path.c_str(), TW_INTERLEAVED) != 0) {
        delete pab;
        return nullptr;
    }

    pab->m_fs = static_cast<float>(pab->m_tw.h.SampleRate);
    pab->m_nbchannels = pab->m_tw.numChannels;
    pab->m_channel_id = m_channel_id;
    pab->m_bits_per_sample = pab->m_tw.h.BitsPerSample;

    return pab;
}


phaseshift::tinywavfile_writer::tinywavfile_writer(int chunk_size_max)
    : phaseshift::tinywavfile(chunk_size_max) {
}

phaseshift::tinywavfile_writer::~tinywavfile_writer() {
    close();
}

void phaseshift::tinywavfile_writer::close() {
    if (tinywav_isOpen(&m_tw)) {
        tinywav_close_write(&m_tw);
    }
}

phaseshift::tinywavfile_writer* phaseshift::tinywavfile_writer_builder::build(phaseshift::tinywavfile_writer* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    assert(m_fs > 0 && "fs has not been set");
    pab->m_file_path = m_file_path;
    pab->m_fs = m_fs;
    pab->m_bits_per_sample = m_bits_per_sample;
    pab->m_length = 0;
    pab->m_nbchannels = m_nbchannels;

    // Determine sample format
    TinyWavSampleFormat sampFmt;
    if (m_use_float && m_bits_per_sample == 32) {
        sampFmt = TW_FLOAT32;
    } else {
        sampFmt = TW_INT16;
    }

    // Open file for writing
    const int16_t nbchannels = static_cast<int16_t>(
        std::clamp(m_nbchannels, 1, static_cast<int>(std::numeric_limits<int16_t>::max())));
    const int32_t sample_rate = static_cast<int32_t>(m_fs);
    if (tinywav_open_write(&pab->m_tw, nbchannels, sample_rate, sampFmt, TW_INTERLEAVED, m_file_path.c_str()) != 0) {
        delete pab;
        return nullptr;
    }

    return pab;
}

phaseshift::tinywavfile_writer* phaseshift::tinywavfile_writer_builder::open(const std::string& file_path, float fs, int chunk_size_max, int nbchannels, int bits_per_sample, bool use_float) {
    tinywavfile_writer_builder builder;
    builder.set_file_path(file_path);
    builder.set_fs(fs);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_nbchannels(nbchannels);
    builder.set_bits_per_sample(bits_per_sample);
    builder.set_use_float(use_float);
    return builder.open();
}
