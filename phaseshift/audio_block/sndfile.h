// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_AUDIO_BLOCK_SNDFILE_H_
#define PHASESHIFT_AUDIO_BLOCK_SNDFILE_H_

#include <phaseshift/audio_block/audio_block.h>

extern "C" {
#include    <stdio.h>
#include    <sndfile.h>
}

#include <string>
#include <algorithm>

namespace phaseshift {

    namespace ab {

        class sndfile : public audio_block {
         protected:
            std::string m_file_path;
            SNDFILE* m_file_handle = nullptr;
            SF_INFO m_sf_info;

            int m_chunk_size_max = 0;
            float* m_chunk = nullptr;  // TODO(GD) Maybe libsndfile does use some internal buffer size and in this case it would be better to use them instead of adding another layer of buffer.
            int m_nbchannels = -1;
            int m_channel_id = -1;
            int m_bitrate = -1;

            explicit sndfile(int chunk_size_max = 1024);
            void close();
            virtual ~sndfile();

         public:
            //! Return libsndfile version
            static std::string version();
        };

        class sndfile_reader_builder;

        class sndfile_reader : public sndfile {

         protected:
            explicit sndfile_reader(int chunk_size_max = 1024);

         public:
            template<class ringbuffer>
            static int read(const std::string& file_path, ringbuffer* pout, int chunk_size_max = 1024, int channel_id = 0);
            static float get_fs(const std::string& file_path);
            static int get_nbchannels(const std::string& file_path);
            static int get_nbframes(const std::string& file_path);
            static int get_bitrate(const std::string& file_path);

            //! Return the number of frames field of the `SF_INFO` structure.
            inline phaseshift::globalcursor_t length() const {return m_sf_info.frames;}
            inline float duration() const {return length()/fs();}

            //! WARNING: Not multi-thread safe
            template<class ringbuffer>
            int read(ringbuffer* pout, int requested_size) {
                proc_time_start();

                assert(m_nbchannels > 0);
                assert((m_nbchannels > 0) && (m_channel_id >=0));

                int read_frames = -1;
                int read_frames_total = 0;
                int nbframes = std::min<int>(requested_size, m_chunk_size_max/m_nbchannels);
                while ((read_frames = sf_read_float(m_file_handle, m_chunk, nbframes))) {
                    read_frames_total += read_frames;
                    if (m_nbchannels == 1)
                        pout->push_back(m_chunk, read_frames);
                    else
                        for (int n=0; n < read_frames; ++n) {
                            if (m_channel_id == n%m_nbchannels)
                                pout->push_back(m_chunk[n]);
                        }

                    if (nbframes > requested_size-read_frames_total)
                        nbframes = requested_size-read_frames_total;
                }

                proc_time_end(read_frames_total/fs());
                return read_frames_total;
            }

            friend phaseshift::ab::sndfile_reader_builder;
        };

        class sndfile_reader_builder : public phaseshift::audio_block_builder {
         protected:
            std::string m_file_path = "";
            int m_chunk_size_max = 1024;
            int m_channel_id = 0;

            sndfile_reader* build(sndfile_reader* pab);

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

            sndfile_reader* open() {return build(new phaseshift::ab::sndfile_reader(m_chunk_size_max));}
            static sndfile_reader* open(const std::string& file_path, int chunk_size_max = 1024, int channel_id = 0);
        };

        template<class ringbuffer>
        int phaseshift::ab::sndfile_reader::read(const std::string& file_path, ringbuffer* pout, int chunk_size, int channel_id) {
            auto reader = phaseshift::ab::sndfile_reader_builder::open(file_path, chunk_size, channel_id);
            if (reader == nullptr) return 0;
            while (reader->read(pout, chunk_size) > 0) {}
            delete reader;
            return pout->size();
        }

        class sndfile_writer_builder;

        class sndfile_writer : public sndfile {
            phaseshift::globalcursor_t m_length = 0;

         protected:
            explicit sndfile_writer(int chunk_size_max = 1024);

         public:
            template<class ringbuffer>
            static int write(const std::string& file_path, float fs, const ringbuffer& pin, int chunk_size = 1024, int bitrate=-1);
            template<class ringbuffer>
            static int write(const std::string& file_path, float fs, const std::vector<ringbuffer*>& ins, int chunk_size = 1024, int bitrate=-1);

            //! Return the number of frames written
            inline phaseshift::globalcursor_t length() const  {return m_length;}
            //! Return the duration [s] of the frames written
            inline float duration() const {return length()/fs();}

            //! WARNING: Not multi-thread safe
            template<class ringbuffer>
            int write(const ringbuffer& in) {
                assert(fs() > 0);
                proc_time_start();

                int read_samples_total = 0;
                int written_samples_total = 0;
                while (read_samples_total < in.size()) {
                    int chunk_size = std::min(in.size()-read_samples_total, m_chunk_size_max);
                    for (int n=0; n < chunk_size; ++n, ++read_samples_total)
                        m_chunk[n] = in[read_samples_total];

                    written_samples_total += sf_write_float(m_file_handle, m_chunk, chunk_size);
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
                for (int n=0; n < int(ins.size()); ++n) {
                    assert(ins[n]->size() == ins[0]->size() && "All input ringbuffers must have the same size");
                }
                int wavlen = ins[0]->size();

                int read_frames_total = 0;
                int written_frames_total = 0;
                while (read_frames_total < wavlen) {
                    int nbframes = std::min(wavlen-read_frames_total, m_chunk_size_max/m_nbchannels);
                    for (int n=0; n < nbframes; ++n, ++read_frames_total) {
                        for (int c=0; c < m_nbchannels; ++c)
                            m_chunk[n*m_nbchannels + c] = (*(ins[c]))[read_frames_total];
                    }

                    written_frames_total += sf_write_float(m_file_handle, m_chunk, nbframes*m_nbchannels);
                }

                m_length += written_frames_total;

                proc_time_end(written_frames_total/fs());
                return written_frames_total;
            }

            friend phaseshift::ab::sndfile_writer_builder;
        };

        class sndfile_writer_builder : public phaseshift::audio_block_builder {
         protected:
            std::string m_file_path = "";
            float m_fs = -1.0f;
            int m_chunk_size_max = 1024;
            int m_nbchannels = 1;
            int m_bitrate = -1;

            sndfile_writer* build(sndfile_writer* pab);

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
            void set_bitrate(int bitrate) {
                m_bitrate = bitrate;
            }

            sndfile_writer* open() {return build(new phaseshift::ab::sndfile_writer(m_chunk_size_max));}
            static sndfile_writer* open(const std::string& file_path, float fs, int chunk_size_max = 1024, int nbchannels = 1, int bitrate=-1);
        };

        template<class ringbuffer>
        int phaseshift::ab::sndfile_writer::write(const std::string& file_path, float fs, const ringbuffer& in, int chunk_size, int bitrate) {
            assert(in.size() > 0 && "Audio channel is empty.");
            auto writer = sndfile_writer_builder::open(file_path, fs, chunk_size, 1, bitrate);
            if (writer == nullptr) return 0;
            int size = writer->write(in);
            delete writer;
            return size;
        }

        template<class ringbuffer>
        int phaseshift::ab::sndfile_writer::write(const std::string& file_path, float fs, const std::vector<ringbuffer*>& ins, int chunk_size, int bitrate) {
            assert(ins.size() > 0 && "No audio channels exist for writing.");
            auto writer = sndfile_writer_builder::open(file_path, fs, chunk_size, ins.size(), bitrate);
            if (writer == nullptr) return 0;
            int size = writer->write(ins);
            delete writer;
            return size;
        }

    }  // namespace ab
}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_SNDFILE_H_
