// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_DEV_AUDIO_BLOCK_TESTER_H_
#define PHASESHIFT_DEV_AUDIO_BLOCK_TESTER_H_

#include <phaseshift/utils.h>
#include <acbench/time_elapsed.h>
#include <phaseshift/dev/time_elapsed_summary.h>
#include <phaseshift/audio_block/sndfile.h>

#include <string>

namespace phaseshift {
    namespace dev {
        namespace ab {

            class tester {
                int m_nb_iter;

                phaseshift::ab::sndfile_reader_builder m_file_reader_builder;

                float m_fs = -1.0f;

                std::string m_file_path_in;
                phaseshift::ringbuffer<float> m_file_in;
                phaseshift::ringbuffer<float> m_file_out;

                float m_test_resynthesis_err_threshold_db = 0.0f;  // If 0.0, no test

             protected:
                static std::string file_test_source_dir();
                PHASESHIFT_PROF(phaseshift::dev::time_elapsed_summary m_abs;)
                const std::string& file_path_in() const {return m_file_path_in;}
                const phaseshift::ringbuffer<float>& file_in() const {return m_file_in;}
                const phaseshift::ringbuffer<float>& file_out() const {return m_file_out;}

                int m_param_chunk_size;

                //  chunk_size_min=1 for bug tests, chunk_size_min=16 for speed tests
                //  chunk_size_max=16000 for bug tests, chunk_size_max=800 for speed tests
                virtual void randomize_params_chunk_size(std::mt19937* pgen, int iter, int chunk_size_min, int chunk_size_max);

                // To be overloaded by specific test sub-classes:
                //! Called before each iteration
                virtual void randomize_params(std::mt19937* pgen, int iter);
                //! For each iteration, called for initializing the thing to test
                virtual void iteration_initialize();
                //! For each iteration, and for each audio chunk, called for each processing call
                virtual void iteration_proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout);
                //! For each iteration, called for finalizing/freeling the thing to test
                virtual void iteration_finalize(phaseshift::ringbuffer<float>* pout);
                //! For each iteration, called after the finalization block
                virtual void iteration_tests();  // The tests to run after each iteration
                //! Called after all the iterations have been run
                virtual void final_tests();

             public:
                explicit tester(int nb_iter);
                inline float fs() const {return m_fs;}

                void set_test_resynthesis_err_threshold_db(float db) {
                    m_test_resynthesis_err_threshold_db = db;
                }

                void run();
            };

        }  // namespace ab
    }  // namespace dev
}  // namespace phaseshift

#endif  // PHASESHIFT_DEV_AUDIO_BLOCK_TESTER_H_
