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
                bool fully_covered_by_window;
                bool skipping_samples_at_start;
                bool flushing;
                phaseshift::globalcursor_t input_win_center_idx = 0;
                phaseshift::globalcursor_t output_win_center_idx = 0;
                inline std::string to_string() const {
                    return "first_input_frame=" + std::to_string(first_input_frame) +
                            " last_frame=" + std::to_string(last_frame) +
                            " fully_covered_by_window=" + std::to_string(fully_covered_by_window) +
                            " skipping_samples_at_start=" + std::to_string(skipping_samples_at_start) +
                            " flushing=" + std::to_string(flushing);
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

            int m_extra_samples_to_skip = 0;
            int m_first_frame_at_t0_samples_to_skip = 0;
            int m_extra_samples_to_flush = 0;
            int m_rt_out_size_max = -1;
            int m_output_added_max = -1;
            bool m_flushing = false;
            int m_nb_samples_to_flush_total = 0;

            phaseshift::globalcursor_t m_input_length = 0;
            phaseshift::globalcursor_t m_input_win_center_idx = 0;
            phaseshift::globalcursor_t m_input_win_center_idx_next = 0;
            phaseshift::globalcursor_t m_output_length = 0;
            phaseshift::globalcursor_t m_output_win_center_idx = 0;

            void proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_output);

            // Member variables for real-time processing
            // input/output buffers to get output buffer same length as input buffer (often used for real-time use cases)
            phaseshift::ringbuffer<float> m_rt_out;
            bool m_rt_received_samples = false;

            int m_stat_rt_nb_post_underruns = 0;
            int m_stat_rt_nb_failed = 0;
            int m_stat_rt_out_size_min = phaseshift::int32::max();

          protected:
            int m_timestep = -1;
            phaseshift::globalcursor_t input_length() const {
                return m_input_length;
            }
            inline void set_extra_samples_to_flush(int nbsamples) {
                m_extra_samples_to_flush = nbsamples;
            }
            inline int extra_samples_to_flush() const {
                return m_extra_samples_to_flush;
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

            ola();

          public:

            virtual ~ola();

            inline int winlen() const {return m_win.size();}
            inline const phaseshift::vector<float>& win() const {return m_win;}
            inline int timestep() const {return m_timestep;}

            //! Limit the number of samples that can be added to the output buffer in one call to proc(.) or flush(.)
            inline void set_output_added_max(int size) {
                m_output_added_max = size;
            }
            inline int output_added_max() const {
                return m_output_added_max;
            }
            //! Returns the minimum number of samples (if not 0) that can be outputted in one call to proc(.)
            inline int min_output_size() const {
                return m_timestep;
            }
            //! For a given chunk size, returns the maximum number of samples that can be outputted in one call to proc(.)
            inline int max_output_size(int chunk_size) const {
                return m_timestep * std::ceil(static_cast<float>(chunk_size)/m_timestep);
            }

            virtual void proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);

            virtual void proc_same_size(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);

            virtual void flush(phaseshift::ringbuffer<float>* pout);

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
            int m_extra_samples_to_skip = 0;
            int m_extra_samples_to_flush = 0;
            int m_rt_out_size_max = -1;

            public:
            inline void set_winlen(int winlen) {
                assert(winlen > 0);
                m_winlen = winlen;
            }
            inline void set_timestep(int timestep) {
                assert(timestep > 0);
                m_timestep = timestep;
            }
            inline void set_extra_samples_to_skip(int nbsamples) {
                m_extra_samples_to_skip = nbsamples;
            }
            inline void set_extra_samples_to_flush(int nbsamples) {
                m_extra_samples_to_flush = nbsamples;
            }
            inline void set_in_out_same_size_max(int size_max) {
                m_rt_out_size_max = size_max;
            }

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
