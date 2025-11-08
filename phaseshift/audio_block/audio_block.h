// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_AUDIO_BLOCK_H_
#define PHASESHIFT_AUDIO_BLOCK_H_

#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/utils.h>

#include <acbench/time_elapsed.h>

#include <string>
#include <functional>

namespace phaseshift {

    class audio_block {

     protected:
        float m_fs = -1.0f;  // Make it float since it will, in most use case, be converted to float anyway.

        #ifdef PHASESHIFT_DEV_PROFILING
         public:
            acbench::time_elapsed dbg_proc_time;
         protected:
            inline void proc_time_start() {dbg_proc_time.start();}
            inline void proc_time_end(float duration)   {dbg_proc_time.end(duration);}
            inline void proc_time_reset() {dbg_proc_time.reset();}
         public:
            const acbench::time_elapsed& time_elapsed() const {return dbg_proc_time;}  // TODO(GD) rm

        #else
         public:
            inline void proc_time_start() {}
            inline void proc_time_end(float duration)   {(void)duration;}
            inline void proc_time_reset() {}
        #endif

        audio_block() {}

     public:
        //! The sampling frequency of the processed signal. Of float type, because it will most often end up being converted to float anyway.
        inline float fs() const {return m_fs;}

        //! Main entry function for processing an input (without any given output)
        virtual void proc(const phaseshift::ringbuffer<float>& in) {
            proc_time_start();

            // Processing happens here

            proc_time_end(in.size()/fs());
        }
        virtual void flush() {
            // Processing any remaining input data here
        }

        //! Main entry function for transforming an input.
        //  This function does not realize the latency returned by latency(.)
        //  It is thus usefull for offline processing.
        virtual void proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
            assert(pout != nullptr);
            proc_time_start();

            // Processing happens here
            pout->push_back(in);

            proc_time_end(in.size()/fs());
        }

        //! This function always output in `pout` the same number of samples that are provided in `in`.
        //  It is quite handy for real-time processing.
        //  Because no sample is available as long as the latency is not expired, this function will prepend
        //  the output with the necessary zeros.
        virtual void proc_same_size(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
            proc(in, pout);
        }

        virtual void flush(phaseshift::ringbuffer<float>* pout) {
            // Processing any remaining input data here
            (void)pout;
        }

        //! When using proc_same_size(.), the latency is the delay an audio event (ex. a click)
        //  go through processing before re-appearing in the output.
        //  When using proc(.), this latency is not realized. I.e. the proc(.) function will not output any
        //  sample until the latency is expired.
        //  Ex. When working with an audio buffer of 256 samples (ex.), the audio system will have at least
        //      a latency of 256 samples. Adding a audio block, which inherits from phaseshift::audio_block,
        //      that has a latency of 882 (ex.), the final overall latency will be of 256+882=1138 samples.
        //  Warning: It has nothing to do with the computational time to compute the output (except if the
        //      execution time is greater than the audio system latency, but in that case, the incurred
        //      underruns will tell you about it).
        virtual int latency() const {return 0;}

        //! Reset the audio block, so that its state becomes exactly the same as when it was just built by
        //  its builder.
        //  The parameters set by the builder are preserved through a reset()
        //  It should not do any memory re-allocations.
        //  This function should not be virtual, since one reset() call should explicitly recall its
        //  parent reset() function.
        inline void reset() {
            // m_fs need to be preserved as is. So don't reset it to anything.
            // Carry the profiling statistics over the reset, thus do not reset them.
        }
    };

    class audio_block_builder {

        float m_fs = -1.0f;  // Make it float since it will most often end up being converted to float anyway.

        #ifdef PHASESHIFT_DEV_PROFILING
         protected:
            acbench::time_elapsed m_build_time;
            inline void build_time_start() {m_build_time.start();}
            inline void build_time_end()   {m_build_time.end(0.0f);}
            inline void build_time_reset() {m_build_time.reset();}
         public:
            const acbench::time_elapsed& time_elapsed() const {return m_build_time;}

        #else
         public:
            inline void build_time_start() {}
            inline void build_time_end()   {}
            inline void build_time_reset() {}
        #endif

     public:
        //! Set the sampling frequency.
        inline void set_fs(float fs) {m_fs = fs;}

        //! The sampling frequency of the processed signal. Of float type, since it will, in most use case, be converted to float anyway.
        inline float fs() const {return m_fs;}

        //! Build the corresponding audio block.
        //  And audio block is _not_ dependent on its builder. Thus, the builder can be discarded after being used.
        audio_block* build(audio_block* pab) {
            assert((fs() > 0) && "Sampling frequency must be >0. Hint: It is necessary to call set_fs(.) before calling build(.).");
            return pab;
        }
    };

    namespace dev {
        //! Run a function multiple time in parallel
        //  Usefull for running tests of builders and processing in multithreaded mode
        void audio_block_builder_test(std::function<void(void)> fn, int nb_threads=4);

    }  // namespace dev
}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_H_
