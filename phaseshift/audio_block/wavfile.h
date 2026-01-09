// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift
//
// Minimalist WAV file reader/writer (no external dependencies)
// Supports: PCM 16-bit and IEEE Float 32-bit, mono and multi-channel

#ifndef PHASESHIFT_AUDIO_BLOCK_WAVFILE_H_
#define PHASESHIFT_AUDIO_BLOCK_WAVFILE_H_

#include <phaseshift/audio_block/audio_block.h>

#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>

namespace phaseshift {

    // WAV file format constants
    namespace wav {
        constexpr uint16_t FORMAT_PCM = 1;
        constexpr uint16_t FORMAT_IEEE_FLOAT = 3;
    }

    // WAV file header structures (not packed - use read/write helpers for I/O)
    struct wav_header {
        char riff[4];           // "RIFF"
        uint32_t file_size;     // File size - 8
        char wave[4];           // "WAVE"
    };

    struct wav_fmt_chunk {
        char fmt[4];            // "fmt "
        uint32_t chunk_size;    // 16 for PCM
        uint16_t audio_format;  // 1=PCM, 3=IEEE float
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;     // sample_rate * num_channels * bits_per_sample/8
        uint16_t block_align;   // num_channels * bits_per_sample/8
        uint16_t bits_per_sample;
    };

    struct wav_data_chunk {
        char data[4];           // "data"
        uint32_t data_size;     // num_samples * num_channels * bits_per_sample/8
    };

    class wavfile : public audio_block {
     protected:
        std::string m_file_path;
        FILE* m_file_handle = nullptr;
        wav_header m_header;
        wav_fmt_chunk m_fmt;
        wav_data_chunk m_data;
        long m_data_start_pos = 0;

        int m_chunk_size_max = 0;
        float* m_chunk = nullptr;
        int m_nbchannels = -1;
        int m_channel_id = -1;
        int m_bits_per_sample = -1;

        explicit wavfile(int chunk_size_max = 1024);
        void close();
        virtual ~wavfile();

     public:
    };

    class wavfile_reader_builder;

    class wavfile_reader : public wavfile {
     protected:
        explicit wavfile_reader(int chunk_size_max = 1024);

     public:
        template<class ringbuffer>
        static int read(const std::string& file_path, ringbuffer* pout, int chunk_size_max = 1024, int channel_id = 0);
        static float get_fs(const std::string& file_path);
        static int get_nbchannels(const std::string& file_path);
        static int get_nbframes(const std::string& file_path);
        static int get_bits_per_sample(const std::string& file_path);

        //! Return the number of frames in the file
        inline phaseshift::globalcursor_t length() const {
            return m_data.data_size / (m_fmt.num_channels * m_fmt.bits_per_sample / 8);
        }
        inline float duration() const {return length()/fs();}

        //! WARNING: Not multi-thread safe
        template<class ringbuffer>
        int read(ringbuffer* pout, int requested_size) {
            proc_time_start();

            assert(m_nbchannels > 0);
            assert((m_nbchannels > 0) && (m_channel_id >= 0));

            int bytes_per_sample = m_fmt.bits_per_sample / 8;
            int frame_size = m_nbchannels * bytes_per_sample;
            int nbframes = std::min<int>(requested_size, m_chunk_size_max / m_nbchannels);

            int read_frames_total = 0;
            while (read_frames_total < requested_size) {
                int frames_to_read = std::min(nbframes, requested_size - read_frames_total);
                size_t bytes_read = fread(m_chunk, 1, frames_to_read * frame_size, m_file_handle);
                int frames_read = bytes_read / frame_size;
                if (frames_read == 0) break;

                // Convert and extract channel
                const uint8_t* raw = reinterpret_cast<const uint8_t*>(m_chunk);
                for (int n = 0; n < frames_read; ++n) {
                    const uint8_t* sample_ptr = raw + n * frame_size + m_channel_id * bytes_per_sample;
                    float sample = convert_to_float(sample_ptr);
                    pout->push_back(sample);
                }

                read_frames_total += frames_read;
            }

            proc_time_end(read_frames_total/fs());
            return read_frames_total;
        }

        friend phaseshift::wavfile_reader_builder;

     private:
        float convert_to_float(const uint8_t* sample_ptr) const;
    };

    class wavfile_reader_builder : public phaseshift::audio_block_builder {
     protected:
        std::string m_file_path = "";
        int m_chunk_size_max = 1024;
        int m_channel_id = 0;

        wavfile_reader* build(wavfile_reader* pab);

     public:
        inline void set_file_path(const std::string& file_path) {
            m_file_path = file_path;
        }
        inline void set_chunk_size_max(int chunk_size_max) {
            m_chunk_size_max = chunk_size_max;
        }
        inline void set_channel_id(int channel_id) {
            m_channel_id = channel_id;
        }

        wavfile_reader* open() {return build(new phaseshift::wavfile_reader(m_chunk_size_max));}
        static wavfile_reader* open(const std::string& file_path, int chunk_size_max = 1024, int channel_id = 0);
    };

    template<class ringbuffer>
    int phaseshift::wavfile_reader::read(const std::string& file_path, ringbuffer* pout, int chunk_size, int channel_id) {
        auto reader = phaseshift::wavfile_reader_builder::open(file_path, chunk_size, channel_id);
        if (reader == nullptr) return 0;
        while (reader->read(pout, chunk_size) > 0) {}
        delete reader;
        return pout->size();
    }

    class wavfile_writer_builder;

    class wavfile_writer : public wavfile {
        phaseshift::globalcursor_t m_length = 0;
        uint32_t m_written_bytes = 0;

     protected:
        explicit wavfile_writer(int chunk_size_max = 1024);

     public:
        ~wavfile_writer() override;
        void close();
        template<class ringbuffer>
        static int write(const std::string& file_path, float fs, const ringbuffer& pin, int chunk_size = 1024, int bits_per_sample = 16, bool use_float = false);
        template<class ringbuffer>
        static int write(const std::string& file_path, float fs, const std::vector<ringbuffer*>& ins, int chunk_size = 1024, int bits_per_sample = 16, bool use_float = false);

        //! Return the number of frames written
        inline phaseshift::globalcursor_t length() const {return m_length;}
        //! Return the duration [s] of the frames written
        inline float duration() const {return length()/fs();}

        //! WARNING: Not multi-thread safe
        template<class ringbuffer>
        int write(const ringbuffer& in) {
            assert(fs() > 0);
            proc_time_start();

            size_t read_samples_total = 0;
            int written_samples_total = 0;
            while (read_samples_total < in.size()) {
                int chunk_size = std::min<size_t>(in.size()-read_samples_total, m_chunk_size_max);
                for (int n = 0; n < chunk_size; ++n, ++read_samples_total) {
                    if (write_sample(in[read_samples_total]))
                        ++written_samples_total;
                }
            }

            m_length += written_samples_total;

            proc_time_end(written_samples_total/fs());
            return written_samples_total;
        }

        //! WARNING: Not multi-thread safe
        template<class ringbuffer>
        int write(const std::vector<ringbuffer*>& ins) {
            assert(fs() > 0);
            proc_time_start();

            m_nbchannels = ins.size();
            for (int n = 0; n < int(ins.size()); ++n) {
                assert(ins[n]->size() == ins[0]->size() && "All input ringbuffers must have the same size");
            }
            int wavlen = ins[0]->size();

            int read_frames_total = 0;
            int written_frames_total = 0;
            while (read_frames_total < wavlen) {
                int nbframes = std::min(wavlen-read_frames_total, m_chunk_size_max/m_nbchannels);
                for (int n = 0; n < nbframes; ++n, ++read_frames_total) {
                    for (int c = 0; c < m_nbchannels; ++c) {
                        if (write_sample((*(ins[c]))[read_frames_total]))
                            ++written_frames_total;
                    }
                }
            }

            m_length += written_frames_total / m_nbchannels;

            proc_time_end(written_frames_total/m_nbchannels/fs());
            return written_frames_total / m_nbchannels;
        }

        friend phaseshift::wavfile_writer_builder;

     private:
        bool write_sample(float sample);
        void finalize_header();
    };

    class wavfile_writer_builder : public phaseshift::audio_block_builder {
     protected:
        std::string m_file_path = "";
        float m_fs = -1.0f;
        int m_chunk_size_max = 1024;
        int m_nbchannels = 1;
        int m_bits_per_sample = 16;
        bool m_use_float = false;

        wavfile_writer* build(wavfile_writer* pab);

     public:
        void set_file_path(const std::string& file_path) {
            m_file_path = file_path;
        }
        void set_fs(float fs) {
            m_fs = fs;
        }
        void set_chunk_size_max(int chunk_size_max) {
            m_chunk_size_max = chunk_size_max;
        }
        void set_nbchannels(int nbchannels) {
            m_nbchannels = nbchannels;
        }
        void set_bits_per_sample(int bits_per_sample) {
            m_bits_per_sample = bits_per_sample;
        }
        void set_use_float(bool use_float) {
            m_use_float = use_float;
        }

        wavfile_writer* open() {return build(new phaseshift::wavfile_writer(m_chunk_size_max));}
        static wavfile_writer* open(const std::string& file_path, float fs, int chunk_size_max = 1024, int nbchannels = 1, int bits_per_sample = 16, bool use_float = false);
    };

    template<class ringbuffer>
    int phaseshift::wavfile_writer::write(const std::string& file_path, float fs, const ringbuffer& in, int chunk_size, int bits_per_sample, bool use_float) {
        assert(in.size() > 0 && "Audio channel is empty.");
        auto writer = wavfile_writer_builder::open(file_path, fs, chunk_size, 1, bits_per_sample, use_float);
        if (writer == nullptr) return 0;
        int size = writer->write(in);
        delete writer;
        return size;
    }

    template<class ringbuffer>
    int phaseshift::wavfile_writer::write(const std::string& file_path, float fs, const std::vector<ringbuffer*>& ins, int chunk_size, int bits_per_sample, bool use_float) {
        assert(ins.size() > 0 && "No audio channels exist for writing.");
        auto writer = wavfile_writer_builder::open(file_path, fs, chunk_size, ins.size(), bits_per_sample, use_float);
        if (writer == nullptr) return 0;
        int size = writer->write(ins);
        delete writer;
        return size;
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_WAVFILE_H_
