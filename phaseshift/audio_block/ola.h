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

    namespace ab {

        // OverLap Add (OLA): Segment the input signal into frames and reconstruct a new signal based on the processed frames.
        class ola : public phaseshift::audio_block {

         public:
            struct proc_status {
                bool first_frame;
                bool last_frame;
                bool fully_covered_by_window;
                bool skipping_samples_at_start;
                bool flushing;
                inline std::string to_string() const {
                    return "first_frame=" + std::to_string(first_frame) +
                           " last_frame=" + std::to_string(last_frame) +
                           " fully_covered_by_window=" + std::to_string(fully_covered_by_window) +
                           " skipping_samples_at_start=" + std::to_string(skipping_samples_at_start) +
                           " flushing=" + std::to_string(flushing);
                }
            };

         protected:
            phaseshift::vector<float> m_win;

            // This function should be overwritten by the custom class that inherit phaseshift::ola
            virtual void proc_frame(const phaseshift::vector<float>& in, phaseshift::vector<float>* pout, const phaseshift::ab::ola::proc_status& status, phaseshift::globalcursor_t win_center_idx);

         private:
            proc_status m_status;

            phaseshift::ringbuffer<float> m_frame_rolling;
            phaseshift::vector<float> m_frame_input;
            phaseshift::vector<float> m_frame_output;

            phaseshift::ringbuffer<float> m_out_sum;
            phaseshift::ringbuffer<float> m_out_sum_win;

            bool m_first_frame_at_t0 = true;
            int m_extra_samples_to_skip = 0;
            int m_first_frame_at_t0_samples_to_skip = 0;
            int m_extra_samples_to_flush = 0;
            int m_rt_out_size_max = -1;

            phaseshift::globalcursor_t m_win_center_idx = 0;

            void proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_flush);

            // Member variables for real-time processing
            // input/output buffers to get output buffer same length as input buffer (often used for real-time use cases)
            phaseshift::ringbuffer<float> m_rt_out;
            bool m_rt_received_samples = false;

            int m_stat_rt_nb_post_underruns = 0;
            int m_stat_rt_nb_failed = 0;
            int m_stat_rt_out_size_min = phaseshift::int32::max();

         protected:
            int m_timestep = -1;

            ola();

         public:

            virtual ~ola();

            inline int winlen() const {return m_win.size();}
            inline const phaseshift::vector<float>& win() const {return m_win;}
            inline int timestep() const {return m_timestep;}

            virtual void proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);

            virtual void proc_same_size(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);

            virtual void flush(phaseshift::ringbuffer<float>* pout);

            virtual void reset();

            inline int stat_rt_nb_failed() const {return m_stat_rt_nb_failed;}
            inline int stat_rt_out_size_min() const {return m_stat_rt_out_size_min;}

            PHASESHIFT_PROF(acbench::time_elapsed dbg_proc_frame_time;)

            friend class ola_builder;
        };

        class ola_builder : public phaseshift::audio_block_builder {
         protected:
            int m_winlen = -1;
            int m_timestep = -1;
            bool m_first_frame_at_t0 = true;
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
            inline void set_first_frame_at_t0(bool first_frame_at_t0) {
                m_first_frame_at_t0 = first_frame_at_t0;
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
            ola* build() {return build(new phaseshift::ab::ola());}
        };
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_OLA_H_
