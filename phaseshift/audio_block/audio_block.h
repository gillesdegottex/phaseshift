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

namespace phaseshift {

    class audio_block {

     protected:
        float m_fs = -1.0f;  // Make it float since it will most often end up being converted to float anyway.

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

        virtual void proc(const phaseshift::ringbuffer<float>& in) {
            proc_time_start();

            proc_time_end(in.size()/fs());
        }
        virtual void flush() {
        }

        virtual void proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
            proc_time_start();

            pout->push_back(in);

            proc_time_end(in.size()/fs());
        }

        virtual void flush(phaseshift::ringbuffer<float>* pout) {
            (void)pout;
        }

        //! [samples]
        virtual int latency() const {return 0;}

        virtual void reset() {
            // m_fs need to be preserved as is.
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

        //! The sampling frequency of the processed signal. Of float type, because it will most often end up being converted to float anyway.
        inline float fs() const {return m_fs;}

        audio_block* build(audio_block* pab) {
            assert((fs() > 0) && "Sampling frequency must be >0. Hint: It is necessary to call set_fs(.) before calling build(.).");
            return pab;
        }
    };

}  // namespace phaseshift

#endif  // PHASESHIFT_AUDIO_BLOCK_H_
