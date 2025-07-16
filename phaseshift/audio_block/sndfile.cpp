// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/sndfile.h>

#include <cstring>  // For memset(.)
#include <cassert>
#include <string_view>

// Until C++20 is widely available, we need to define these functions
inline bool ps_ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}
inline bool ps_starts_with(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}


std::string phaseshift::ab::sndfile::version() {
    char version[128];
    sf_command(nullptr, SFC_GET_LIB_VERSION, version, sizeof(version));
    std::string version_str = version;
    version_str = version_str.substr(version_str.find_first_of("-")+1);
    return version_str;
}

phaseshift::ab::sndfile::sndfile(int chunk_size_max) {
    assert(chunk_size_max > 0);
    m_chunk_size_max = chunk_size_max;
    m_chunk = new float[m_chunk_size_max];
    memset(&m_sf_info, 0, sizeof(m_sf_info));
}

void phaseshift::ab::sndfile::close() {
    if (m_file_handle) {
        sf_close(m_file_handle);
        m_file_handle = nullptr;
    }
    memset(&m_sf_info, 0, sizeof(m_sf_info));
}
phaseshift::ab::sndfile::~sndfile() {
    close();
    if (m_chunk!=nullptr) {
        delete[] m_chunk;
        m_chunk = nullptr;
    }
}

float phaseshift::ab::sndfile_reader::get_fs(const std::string& file_path) {
    assert(file_path != "");

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    SNDFILE* file_handle = sf_open(file_path.c_str(), SFM_READ, &sf_info);
    if (file_handle == nullptr) {
        // sf_strerror(.)  // TODO(GD) Return error type?
        return -1.0f;
    }

    float fs = sf_info.samplerate;

    if (file_handle) {
        sf_close(file_handle);
    }

    return fs;
}

int phaseshift::ab::sndfile_reader::get_nbchannels(const std::string& file_path) {
    assert(file_path != "");

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    SNDFILE* file_handle = nullptr;
    file_handle = sf_open(file_path.c_str(), SFM_READ, &sf_info);
    if (file_handle == nullptr) {
        // sf_strerror(.)  // TODO(GD) Return error type?
        return -1;
    }

    int nbchannels = sf_info.channels;

    if (file_handle) {
        sf_close(file_handle);
    }

    return nbchannels;
}

int phaseshift::ab::sndfile_reader::get_bitrate(const std::string& file_path) {
    assert(file_path != "");

    SF_INFO sf_info;
    memset(&sf_info, 0, sizeof(sf_info));
    SNDFILE* file_handle = nullptr;
    file_handle = sf_open(file_path.c_str(), SFM_READ, &sf_info);
    if (file_handle == nullptr) {
        // sf_strerror(.)  // TODO(GD) Return error type?
        return -1.0f;
    }

    int byterate = sf_current_byterate(file_handle);

    if (file_handle) {
        sf_close(file_handle);
    }

    return byterate * 8;
}

phaseshift::ab::sndfile_reader::sndfile_reader(int chunk_size_max)
    : phaseshift::ab::sndfile(chunk_size_max) {
}

phaseshift::ab::sndfile_reader* phaseshift::ab::sndfile_reader_builder::open(const std::string& file_path, int chunk_size_max, int channel_id) {
    phaseshift::ab::sndfile_reader_builder builder;
    builder.set_file_path(file_path);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_channel_id(channel_id);
    return builder.open();
}

phaseshift::ab::sndfile_reader* phaseshift::ab::sndfile_reader_builder::build(phaseshift::ab::sndfile_reader* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    pab->m_file_path = m_file_path;

    pab->m_file_handle = sf_open(pab->m_file_path.c_str(), SFM_READ, &(pab->m_sf_info));
    if (pab->m_file_handle == nullptr) {
        // sf_strerror(.)  // TODO(GD) Return error type?
        return nullptr;
    }

    int byterate = sf_current_byterate(pab->m_file_handle);
    pab->m_bitrate = byterate * 8;

    pab->m_fs = pab->m_sf_info.samplerate;
    pab->m_nbchannels = pab->m_sf_info.channels;
    pab->m_channel_id = m_channel_id;

    return pab;
}


phaseshift::ab::sndfile_writer::sndfile_writer(int chunk_size_max)
    : phaseshift::ab::sndfile(chunk_size_max) {
}

phaseshift::ab::sndfile_writer* phaseshift::ab::sndfile_writer_builder::build(phaseshift::ab::sndfile_writer* pab) {
    assert(m_file_path != "" && "file_path has not been set");
    assert(m_fs > 0 && "fs has not been set");
    pab->m_file_path = m_file_path;
    pab->m_fs = m_fs;
    pab->m_bitrate = m_bitrate;

    pab->m_length = 0;
    pab->m_nbchannels = m_nbchannels;

    memset(&(pab->m_sf_info), 0, sizeof(pab->m_sf_info));
    #ifdef SF_FORMAT_MPEG
    if (ps_ends_with(m_file_path, ".mp3")) {
        pab->m_sf_info.format     = (SF_FORMAT_MPEG | SF_FORMAT_MPEG_LAYER_III);
    } else if (ps_ends_with(m_file_path, ".flac")) {
    #else
    if (ps_ends_with(m_file_path, ".flac")) {
    #endif
        pab->m_sf_info.format     = (SF_FORMAT_FLAC | SF_FORMAT_PCM_16);
    } else {
        pab->m_sf_info.format     = (SF_FORMAT_WAV | SF_FORMAT_PCM_16);
    }
    pab->m_sf_info.samplerate = pab->fs();
    pab->m_sf_info.channels   = m_nbchannels;

    pab->m_file_handle = sf_open(pab->m_file_path.c_str(), SFM_WRITE, &(pab->m_sf_info));
    if (pab->m_file_handle == nullptr) {
        // sf_strerror(.)  // TODO(GD) Return error type?
        return nullptr;
    }

    #ifdef SF_FORMAT_MPEG
    if ((pab->m_sf_info.format & SF_FORMAT_SUBMASK) == SF_FORMAT_MPEG_LAYER_III) {
        double level = 1.0;  // Always max it
        int ret = sf_command(pab->m_file_handle, SFC_SET_VBR_ENCODING_QUALITY, &level, sizeof(level));
        if (ret == SF_FALSE) {
            // sf_strerror(.)  // TODO(GD) Return error type?
            return nullptr;
        }
    }
    #endif

    return pab;
}

phaseshift::ab::sndfile_writer* phaseshift::ab::sndfile_writer_builder::open(const std::string& file_path, float fs, int chunk_size_max, int nbchannels, int bitrate) {
    sndfile_writer_builder builder;
    builder.set_file_path(file_path);
    builder.set_fs(fs);
    builder.set_chunk_size_max(chunk_size_max);
    builder.set_nbchannels(nbchannels);
    builder.set_bitrate(bitrate);
    return builder.open();
}
