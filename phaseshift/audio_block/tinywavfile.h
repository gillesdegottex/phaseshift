// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift
//
// WAV file reader/writer using tinywav library

#ifndef PHASESHIFT_AUDIO_BLOCK_TINYWAVFILE_H_
#define PHASESHIFT_AUDIO_BLOCK_TINYWAVFILE_H_

#include <phaseshift/audio_block/audio_block.h>

#include <cstdint>
#include <string>
#include <algorithm>
#include <vector>

#include <tinywav/tinywav.h>

namespace phaseshift {

    // WAV file format constants
    namespace wav {
        constexpr uint16_t FORMAT_PCM = 1;
        constexpr uint16_t FORMAT_IEEE_FLOAT = 3;
    }

    class tinywavfile : public audio_block {
     protected:
        std::string m_file_path;
        TinyWav m_tw;

        int m_chunk_size_max = 0;
        float* m_chunk = nullptr;
        int m_nbchannels = -1;
        int m_channel_id = -1;
        int m_bits_per_sample = -1;

        explicit tinywavfile(int chunk_size_max = 1024);
        void close();
        virtual ~tinywavfile();

     public:
    };

    class tinywavfile_reader_builder;

    class tinywavfile_reader : public tinywavfile {
     protected:
        explicit tinywavfile_reader(int chunk_size_max = 1024);

     public:
        //! Read a single channel from a WAV file
        template<class ringbuffer>
        static int read(const std::string& file_path, ringbuffer* pout, int chunk_size_max = 1024, int channel_id = 0);
        //! Read all channels from a WAV file as interleaved samples
        template<class ringbuffer>
        static int read_interleaved(const std::string& file_path, ringbuffer* pout, int chunk_size_max = 1024);
        static float get_fs(const std::string& file_path);
        static int get_nbchannels(const std::string& file_path);
        static int get_nbframes(const std::string& file_path);
        static int get_bits_per_sample(const std::string& file_path);

        //! Return the number of frames in the file
        inline phaseshift::globalcursor_t length() const {
            return m_tw.numFramesInHeader;
        }
        inline float duration() const {return length()/fs();}

        //! WARNING: Not multi-thread safe
        //! Read a single channel, extracting from interleaved data
        template<class ringbuffer>
        int read(ringbuffer* pout, int requested_size) {
            proc_time_start();

            assert(m_nbchannels > 0);
            assert((m_nbchannels > 0) && (m_channel_id >= 0));

            int nbframes = std::min<int>(requested_size, m_chunk_size_max / m_nbchannels);

            int read_frames_total = 0;
            while (read_frames_total < requested_size) {
                int frames_to_read = std::min(nbframes, requested_size - read_frames_total);
                int frames_read = tinywav_read_f(&m_tw, m_chunk, frames_to_read);
                if (frames_read <= 0) break;

                // Extract the requested channel from interleaved data
                for (int n = 0; n < frames_read; ++n) {
                    float sample = m_chunk[n * m_nbchannels + m_channel_id];
                    pout->push_back(sample);
                }

                read_frames_total += frames_read;
            }

            proc_time_end(read_frames_total/fs());
            return read_frames_total;
        }

        //! WARNING: Not multi-thread safe
        //! Read all channels as interleaved samples
        template<class ringbuffer>
        int read_interleaved(ringbuffer* pout, int requested_frames) {
            proc_time_start();

            assert(m_nbchannels > 0);

            int nbframes = std::min<int>(requested_frames, m_chunk_size_max / m_nbchannels);

            int read_frames_total = 0;
            while (read_frames_total < requested_frames) {
                int frames_to_read = std::min(nbframes, requested_frames - read_frames_total);
                int frames_read = tinywav_read_f(&m_tw, m_chunk, frames_to_read);
                if (frames_read <= 0) break;

                // Copy all interleaved samples
                for (int n = 0; n < frames_read * m_nbchannels; ++n) {
                    pout->push_back(m_chunk[n]);
                }

                read_frames_total += frames_read;
            }

            proc_time_end(read_frames_total/fs());
            return read_frames_total;
        }

        friend phaseshift::tinywavfile_reader_builder;
    };

    class tinywavfile_reader_builder : public phaseshift::audio_block_builder {
     protected:
        std::string m_file_path = "";
        int m_chunk_size_max = 1024;
        int m_channel_id = 0;

        tinywavfile_reader* build(tinywavfile_reader* pab);

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

        tinywavfile_reader* open() {return build(new phaseshift::tinywavfile_reader(m_chunk_size_max));}
        //! Open for reading a single channel
        static tinywavfile_reader* open(const std::string& file_path, int chunk_size_max, int channel_id);
        //! Open for reading all channels (interleaved)
        static tinywavfile_reader* open(const std::string& file_path, int chunk_size_max = 1024);
    };

    template<class ringbuffer>
    int phaseshift::tinywavfile_reader::read(const std::string& file_path, ringbuffer* pout, int chunk_size, int channel_id) {
        auto reader = phaseshift::tinywavfile_reader_builder::open(file_path, chunk_size, channel_id);
        if (reader == nullptr) return 0;
        while (reader->read(pout, chunk_size) > 0) {}
        delete reader;
        return pout->size();
    }

    template<class ringbuffer>
    int phaseshift::tinywavfile_reader::read_interleaved(const std::string& file_path, ringbuffer* pout, int chunk_size) {
        auto reader = phaseshift::tinywavfile_reader_builder::open(file_path, chunk_size);
        if (reader == nullptr) return 0;
        while (reader->read_interleaved(pout, chunk_size) > 0) {}
        delete reader;
        return pout->size();  // Returns total samples (frames * channels)
    }

    class tinywavfile_writer_builder;

    class tinywavfile_writer : public tinywavfile {
        phaseshift::globalcursor_t m_length = 0;

     protected:
        explicit tinywavfile_writer(int chunk_size_max = 1024);

     public:
        ~tinywavfile_writer() override;
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
            int written_frames_total = 0;
            while (read_samples_total < in.size()) {
                int chunk_size = std::min<size_t>(in.size()-read_samples_total, m_chunk_size_max);
                for (int n = 0; n < chunk_size; ++n) {
                    m_chunk[n] = in[read_samples_total + n];
                }
                int frames_written = tinywav_write_f(&m_tw, m_chunk, chunk_size);
                if (frames_written > 0) {
                    written_frames_total += frames_written;
                }
                read_samples_total += chunk_size;
            }

            m_length += written_frames_total;

            proc_time_end(written_frames_total/fs());
            return written_frames_total;
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
                // Interleave channels into m_chunk
                for (int n = 0; n < nbframes; ++n) {
                    for (int c = 0; c < m_nbchannels; ++c) {
                        m_chunk[n * m_nbchannels + c] = (*(ins[c]))[read_frames_total + n];
                    }
                }
                int frames_written = tinywav_write_f(&m_tw, m_chunk, nbframes);
                if (frames_written > 0) {
                    written_frames_total += frames_written;
                }
                read_frames_total += nbframes;
            }

            m_length += written_frames_total;

            proc_time_end(written_frames_total/fs());
            return written_frames_total;
        }

        friend phaseshift::tinywavfile_writer_builder;
    };

    class tinywavfile_writer_builder : public phaseshift::audio_block_builder {
     protected:
        std::string m_file_path = "";
        float m_fs = -1.0f;
        int m_chunk_size_max = 1024;
        int m_nbchannels = 1;
        int m_bits_per_sample = 16;
        bool m_use_float = false;

        tinywavfile_writer* build(tinywavfile_writer* pab);

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

        tinywavfile_writer* open() {return build(new phaseshift::tinywavfile_writer(m_chunk_size_max));}
        static tinywavfile_writer* open(const std::string& file_path, float fs, int chunk_size_max = 1024, int nbchannels = 1, int bits_per_sample = 16, bool use_float = false);
    };

    template<class ringbuffer>
    int phaseshift::tinywavfile_writer::write(const std::string& file_path, float fs, const ringbuffer& in, int chunk_size, int bits_per_sample, bool use_float) {
        assert(in.size() > 0 && "Audio channel is empty.");
        auto writer = tinywavfile_writer_builder::open(file_path, fs, chunk_size, 1, bits_per_sample, use_float);
        if (writer == nullptr) return 0;
        int size = writer->write(in);
        delete writer;
        return size;
    }

    template<class ringbuffer>
    int phaseshift::tinywavfile_writer::write(const std::string& file_path, float fs, const std::vector<ringbuffer*>& ins, int chunk_size, int bits_per_sample, bool use_float) {
        assert(ins.size() > 0 && "No audio channels exist for writing.");
        auto writer = tinywavfile_writer_builder::open(file_path, fs, chunk_size, ins.size(), bits_per_sample, use_float);
        if (writer == nullptr) return 0;
        int size = writer->write(ins);
        delete writer;
        return size;
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_TINYWAVFILE_H_
