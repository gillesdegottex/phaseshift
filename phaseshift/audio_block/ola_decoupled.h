// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_AUDIO_BLOCK_OLA_DECOUPLED_H_
#define PHASESHIFT_AUDIO_BLOCK_OLA_DECOUPLED_H_

#include <phaseshift/utils.h>
#include <phaseshift/containers/vector.h>
#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/sigproc/window_functions.h>
#include <phaseshift/audio_block/audio_block.h>

namespace phaseshift {

        // OverLap Add with Decoupled Input/Output control (OLA Decoupled):
        // Extends the standard OLA to allow daughter classes to control the input/output ratio.
        // This enables time stretching where:
        //   - Slowing down: multiple outputs per input (should_consume_input returns false)
        //   - Speeding up: skip outputs (should_output returns false)
        class ola_decoupled : public phaseshift::audio_block {

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
                long int nb_output_buffer_overflows = 0;  // Number of times the output buffer was full and samples were lost
                long int nb_imperfect_reconstruction = 0;  // Number of samples with insufficient window coverage
                failure_status() {
                    reset();
                }
                inline void reset() {
                    nb_output_buffer_overflows = 0;
                    nb_imperfect_reconstruction = 0;
                }
                inline std::string to_json() const {
                    return "{\"nb_output_buffer_overflows\":" + std::to_string(nb_output_buffer_overflows) +
                           ",\"nb_imperfect_reconstruction\":" + std::to_string(nb_imperfect_reconstruction) + "}";
                }
            } m_failure_status;

            inline const failure_status& get_failure_status() const { return m_failure_status; }

          protected:
            phaseshift::vector<float> m_win;

            // This function should be overwritten by the custom class that inherit phaseshift::ola_decoupled
            virtual void proc_frame(const phaseshift::vector<float>& in, phaseshift::vector<float>* pout, const phaseshift::ola_decoupled::proc_status& status);

            // =========================================================================
            // DECOUPLED CONTROL INTERFACE - Override these in daughter classes
            // =========================================================================

            // Called when input buffer is full. Should we produce an output frame?
            // Return false to skip output (for speeding up).
            // Default: always produce output.
            virtual bool should_output(const proc_status& status) {
                (void)status;
                return true;
            }

            // Called after output is produced. Should we consume the input (advance input cursor)?
            // Return false to produce another output from the same input (for slowing down).
            // Default: always consume input.
            virtual bool should_consume_input(const proc_status& status) {
                (void)status;
                return true;
            }

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

            phaseshift::globalcursor_t m_input_length = 0;
            phaseshift::globalcursor_t m_input_win_center_idx = 0;
            phaseshift::globalcursor_t m_input_win_center_idx_next = 0;
            phaseshift::globalcursor_t m_output_length = 0;
            phaseshift::globalcursor_t m_output_win_center_idx = 0;
            phaseshift::globalcursor_t m_target_output_length = -1;  // Absolute target output length in samples (-1 = disabled)

            // Core OLA output step: process frame and add to OLA accumulator
            int output_one_frame(phaseshift::ringbuffer<float>* pout, int nb_samples_to_output);
            
            // Advance input cursor by timestep
            void advance_input_cursor();

            // Member variables for real-time processing
            int m_realttime_prepad_latency_remaining = -1;
            int m_stat_realtime_out_size_min = std::numeric_limits<int>::max();

          protected:
            int m_timestep = -1;
            inline void set_target_output_length(phaseshift::globalcursor_t target) {
                m_target_output_length = target;
            }
            inline phaseshift::globalcursor_t target_output_length() const {
                return m_target_output_length;
            }
            inline const proc_status& status() const {
                return m_status;
            }

            ola_decoupled();

          public:

            virtual ~ola_decoupled();

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
            inline int min_output_chunk_size() const {
                return m_timestep;
            }
            //! For a given input chunk size, returns the maximum number of samples that can be outputted in one call to proc(.)
            //  In the worst case, the rolling buffer has winlen-1 samples, so 1 input sample triggers a window (outputs timestep).
            //  After that, each additional timestep input samples triggers another window.
            //  Thus max output = timestep * ceil(input_chunk_size / timestep).
            inline int max_output_chunk_size(int input_chunk_size) const {
                return m_timestep * static_cast<int>(std::ceil(static_cast<float>(input_chunk_size)/m_timestep));
            }

            //! Returns the number of samples that can be inputted in the next call to process(.), so that the internal output buffer doesn't blow up.
            //  WARNING: Note the assymetry with the other fonctions below. process_available() is about input samples, whereas all the other ones are about output samples.
            virtual int process_input_available();
            //! All input samples are always consumed. This function returns how many samples were outputted (either inside the internal buffer or in the custom output buffer pout).
            virtual int process(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout=nullptr);
            //! Returns the number of samples that remains to be flushed/outputted.
            inline int flush_available() {
                phaseshift::globalcursor_t target_output_length = m_input_length;
                if (m_target_output_length > 0) {
                    target_output_length = m_target_output_length;
                }
                return target_output_length - m_output_length;
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

            inline int stat_realtime_out_size_min() const {return m_stat_realtime_out_size_min;}

            PHASESHIFT_PROF(acbench::time_elapsed dbg_proc_frame_time;)

            friend class ola_decoupled_builder;
        };

        class ola_decoupled_builder : public phaseshift::audio_block_builder {
            protected:
            int m_winlen = -1;
            int m_timestep = -1;
            int m_extra_samples_to_skip = 0;  // Skip at start
            int m_extra_samples_to_flush = 0; // Flush at the end
            int m_max_input_chunk_size = -1;

            public:
            inline void set_winlen(int winlen) {
                assert(winlen > 0);
                m_winlen = winlen;
            }
            inline void set_timestep(int timestep) {
                assert(timestep > 0);
                m_timestep = timestep;
            }
            //! Set the max input chunk size to properly size the internal output buffer.
            //  The internal buffer will be sized to hold max_output_chunk_size(max_input_chunk_size) samples.
            inline void set_max_input_chunk_size(int max_input_chunk_size) {
                assert(max_input_chunk_size > 0);
                m_max_input_chunk_size = max_input_chunk_size;
            }
            inline void set_extra_samples_to_skip(int nbsamples) {
                m_extra_samples_to_skip = nbsamples;
            }
            inline void set_extra_samples_to_flush(int nbsamples) {
                m_extra_samples_to_flush = nbsamples;
            }

            inline int winlen() const {return m_winlen;}
            inline int timestep() const {return m_timestep;}

            ola_decoupled* build(ola_decoupled* pab);
            ola_decoupled* build() {return build(new phaseshift::ola_decoupled());}
        };

        namespace dev {
            // Test options (same as ola.h)
            enum {option_none_decoupled=0, option_test_latency_decoupled=1};
            // Test function for ola_decoupled
            void audio_block_ola_decoupled_test(phaseshift::ola_decoupled* pab, int chunk_size, float resynthesis_threshold=phaseshift::db2lin(-120.0f), int options=option_test_latency_decoupled);
            void audio_block_ola_decoupled_builder_test_singlethread();
            void audio_block_ola_decoupled_builder_test(int nb_threads=4);
        }

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_OLA_DECOUPLED_H_
