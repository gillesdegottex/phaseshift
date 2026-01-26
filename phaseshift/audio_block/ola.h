// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_AUDIO_BLOCK_OLA_H_
#define PHASESHIFT_AUDIO_BLOCK_OLA_H_

#include <phaseshift/utils.h>
#include <phaseshift/containers/vector.h>
#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/sigproc/window_functions.h>
#include <phaseshift/audio_block/audio_block.h>

#include <algorithm>

namespace phaseshift {

        // OverLap Add (OLA): Segment the input signal into frames and reconstruct a new signal based on the processed frames.
        class ola : public phaseshift::audio_block {

          public:
            struct proc_status {
                bool first_input_frame;
                bool last_frame;
                bool padding_start;
                bool padding_end;
                bool flushing;
                bool finished;
                phaseshift::globalcursor_t input_win_center_idx = 0;
                phaseshift::globalcursor_t output_win_center_idx = 0;
                inline std::string to_string() const {
                    return "first_input_frame=" + std::to_string(first_input_frame) +
                            " last_frame=" + std::to_string(last_frame) +
                            " padding_start=" + std::to_string(padding_start) +
                            " padding_end=" + std::to_string(padding_end) +
                            " flushing=" + std::to_string(flushing);
                }
                inline void reset() {
                    first_input_frame = true;
                    last_frame = false;
                    padding_start = false;
                    padding_end = false;
                    flushing = false;
                    finished = false;
                    input_win_center_idx = 0;
                    output_win_center_idx = 0;
                }
            };

            struct failure_status {
                long int nb_imperfect_reconstruction = 0;  // Number of samples with insufficient window coverage
            } m_failure_status;

          protected:
            phaseshift::vector<float> m_win;

            // This function should be overwritten by the custom class that inherit phaseshift::ola
            virtual void proc_frame(const phaseshift::vector<float>& in, phaseshift::vector<float>* pout, const phaseshift::ola::proc_status& status);

          private:
            proc_status m_status;

            phaseshift::ringbuffer<float> m_frame_rolling;
            phaseshift::vector<float> m_frame_input;
            phaseshift::vector<float> m_frame_output;

            phaseshift::ringbuffer<float> m_out_sum;
            phaseshift::ringbuffer<float> m_out_sum_win;
            phaseshift::ringbuffer<float> m_out;

            int m_extra_samples_to_skip = 0;
            int m_first_frame_at_t0_samples_to_skip = 0;
            int m_extra_samples_to_flush = 0;
            int m_flush_nb_samples_total = 0;
            phaseshift::globalcursor_t m_target_output_length = -1;  // Absolute target output length in samples (-1 = disabled)

            phaseshift::globalcursor_t m_input_length = 0;
            phaseshift::globalcursor_t m_input_win_center_idx = 0;
            phaseshift::globalcursor_t m_input_win_center_idx_next = 0;
            phaseshift::globalcursor_t m_output_length = 0;
            phaseshift::globalcursor_t m_output_win_center_idx = 0;

            int proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_output);

            // Member variables for real-time processing
            int m_rt_prepad_latency_remaining = -1;
            int m_stat_rt_nb_post_underruns = 0;
            int m_stat_rt_nb_failed = 0;
            int m_stat_rt_out_size_min = std::numeric_limits<int>::max();

          protected:
            int m_timestep = -1;
            inline void set_extra_samples_to_flush(int nbsamples) {
                m_extra_samples_to_flush = nbsamples;
            }
            inline int extra_samples_to_flush() const {
                return m_extra_samples_to_flush;
            }
            inline void set_target_output_length(phaseshift::globalcursor_t target) {
                m_target_output_length = target;
            }
            inline phaseshift::globalcursor_t target_output_length() const {
                return m_target_output_length;
            }
            inline proc_status status() const {
                return m_status;
            }

            ola();

          public:

            virtual ~ola();

            inline int winlen() const {return m_win.size();}
            inline const phaseshift::vector<float>& win() const {return m_win;}
            inline int timestep() const {return m_timestep;}

            phaseshift::globalcursor_t input_length() const {
                return m_input_length;
            }
            phaseshift::globalcursor_t input_win_center_idx() const {
                return m_input_win_center_idx;
            }
            phaseshift::globalcursor_t output_length() const {
                return m_output_length;
            }
            phaseshift::globalcursor_t output_win_center_idx() const {
                return m_output_win_center_idx;
            }
            inline bool flushing() const {
                return m_status.flushing;
            }
            inline bool finished() const {
                return m_status.finished;
            }

            //! Returns the minimum number of samples, bigger than zero, that can be outputted in one call to proc(.)
            inline int min_output_size() const {
                return m_timestep;
            }
            //! For a given chunk size, returns the maximum number of samples that can be outputted in one call to proc(.)
            inline int max_output_size(int chunk_size) const {
                return m_timestep * std::ceil(static_cast<float>(chunk_size)/m_timestep);
            }

            //! Returns the number of samples that can be inputted in the next call to process(.), so that the internal output buffer doesn't blow up.
            //  WARNING: Note the assymetry with the other fonctions below. process_available() is about input samples, whereas all the other ones are about output samples.
            virtual int process_input_available();
            //! All input samples are always consumed. This function returns how many samples were outputted (either inside the internal buffer or in the custom output buffer pout).
            virtual int process(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout=nullptr);
            //! Returns the number of samples that remains to be flushed/outputted.
            inline int flush_available() {

                if (m_status.flushing) {
                    return m_flush_nb_samples_total;
                } else {
                    // Number of output samples that remains to be flushed
                    m_flush_nb_samples_total = m_frame_rolling.size();
                    // We absolutely need to flush at least m_frame_rolling.size()
                    if (m_extra_samples_to_flush > 0) {
                        // So add the extra samples to flush only if positive.
                        // If they are eventually too many samples, daughter classes should handle the case.
                        m_flush_nb_samples_total += m_extra_samples_to_flush;
                    }
                }
                return m_flush_nb_samples_total;
            }
            //! flushing might trigger a lot of calls for processing output frames. In a non-offline scenario, it might be better to call flush(.) with a chunk size
            //  Returns how many samples were outputted (either inside the internal buffer or in the custom output buffer pout).
            virtual int flush(int chunk_size_max=-1, phaseshift::ringbuffer<float>* pout=nullptr);
            //! Returns the number of samples ready for output, that can be fetched in a single call to fetch(.)
            inline int fetch_available() const {return m_out.size();}
            //! Return the number of samples actually fetched.
            int fetch(phaseshift::ringbuffer<float>* pout, int chunk_size_max=-1);

            //! Convenience function for offline processing calling the primitives in the right order
            virtual void process_offline(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);

            //! Example function (see its code), for offline processing with a fixed chunk size
            //  WARNING: This function allocates a temporary buffer for building the chunk.
            virtual void process_offline(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout, int chunk_size);

            //! Convenience function for real-time processing calling the primitives in the right order
            //  With this function, pout will always receive exactly in.size() samples
            virtual void process_realtime(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);


            //! [samples]
            virtual int latency() const {return winlen();}

            virtual void reset();

            inline void failure_status_reset() {
                m_failure_status.nb_imperfect_reconstruction = 0;
            }

            inline int stat_rt_nb_failed() const {return m_stat_rt_nb_failed;}
            inline int stat_rt_out_size_min() const {return m_stat_rt_out_size_min;}

            PHASESHIFT_PROF(acbench::time_elapsed dbg_proc_frame_time;)

            friend class ola_builder;
        };

        namespace dev {
            // This function implements all the possible tests an OLA block should pass:
            //    - Can process noise, silence, click, saturated signal, sinusoid, harmonics
            //    - TODO Test speed?
            //    - (audio_block_ola_builder_test_singlethread() tests for singlethreaded building and processing)
            //    - (audio_block_builder_test() tests for multithreading)
            enum {option_none, option_test_latency=1};
            void audio_block_ola_test(phaseshift::ola* pab, int chunk_size, float resynthesis_threshold=phaseshift::db2lin(-120.0f), int options=option_test_latency);
        }

        class ola_builder : public phaseshift::audio_block_builder {
            protected:
            int m_winlen = -1;
            int m_timestep = -1;
            int m_extra_samples_to_skip = 0;  // Skip at start
            int m_extra_samples_to_flush = 0; // Flush at the end
            // int m_rt_out_size_max = -1;
            int m_output_buffer_size_max = -1;

            public:
            inline void set_winlen(int winlen) {
                assert(winlen > 0);
                m_winlen = winlen;
            }
            inline void set_timestep(int timestep) {
                assert(timestep > 0);
                m_timestep = timestep;
            }
            inline void set_output_buffer_size_max(int out_size_max) {
                assert(out_size_max > 0);
                m_output_buffer_size_max = out_size_max;
            }
            inline void set_extra_samples_to_skip(int nbsamples) {
                m_extra_samples_to_skip = nbsamples;
            }
            inline void set_extra_samples_to_flush(int nbsamples) {
                m_extra_samples_to_flush = nbsamples;
            }
            // inline void set_in_out_same_size_max(int size_max) {
            //     m_rt_out_size_max = size_max;
            // }

            inline int winlen() const {return m_winlen;}
            inline int timestep() const {return m_timestep;}

            ola* build(ola* pab);
            ola* build() {return build(new phaseshift::ola());}
        };

        namespace dev {
            // This function implements all the possible tests an OLA block builder should pass:
            //    - Handles multithreaded building and processing
            //    - Handles various fs
            //    - Handles valid combinations of window lengths and timesteps
            //    - Handles various chunk sizes
            void audio_block_ola_builder_test_singlethread();
            void audio_block_ola_builder_test(int nb_threads=4);
        }

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_OLA_H_
