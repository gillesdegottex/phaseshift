// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_DEV_TIME_ELAPSED_SUMMARY_H_
#define PHASESHIFT_DEV_TIME_ELAPSED_SUMMARY_H_

#ifdef PHASESHIFT_DEV_PROFILING

#include <acbench/time_elapsed.h>

#include <map>
#include <string>

namespace phaseshift {
    namespace dev {

        /*! This class helps tracking the time usage of a process, which consists most of the time of:
         * 1) An initialization step (memory allocation, pre-computing of look-up tables, etc.).
         * 2) A loop repeating a process (ex. chunks of audio)
         * 3) A final step (de-allocating memory, maybe writing an output file, etc.)
         *
         * Step (2) is assumed to be decomposed in a sequence of sub-steps that are added by using `loop_add(.)`
         *   Ex. 4 sub-steps:
         *     1) read a chunk of 1024 samples from a file;
         *     2) filtering the chunk through an FFT;
         *     3) re-assembling the filtered chunks;
         *     4) writing the chunk in a file.
         * 
         * See ola_test.cpp for a usage example.
         */
        class time_elapsed_summary {

            // The time_elapsed objects segmenting the looped process.
            std::map<std::string,const acbench::time_elapsed*> m_loop_tes;

          public:
            time_elapsed_summary() {
            }

            //! The sum of the elements added in here should cover the full loop
            inline void loop_add(const std::string name, const acbench::time_elapsed* te) {
                m_loop_tes[name] = te;
            }

            acbench::time_elapsed initialize;   // The time measure of the whole initialisation.
            acbench::time_elapsed loop;         // The time measure of the overall loop execution (not each iteration inside the loop!)
            acbench::time_elapsed finalize;     // The time measure of the whole finalisation.

            void print(std::ostream& out=std::cerr) const;
        };

    }  // namespace dev
}  // namespace phaseshift

#endif  // PHASESHIFT_DEV_PROFILING

#endif  // PHASESHIFT_DEV_TIME_ELAPSED_SUMMARY_H_
