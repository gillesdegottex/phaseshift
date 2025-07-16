// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/tester.h>
#include <phaseshift/dev/catch2_extra.h>

#include <catch2/catch_test_macros.hpp>

std::string phaseshift::dev::ab::tester::file_test_source_dir() {
    return PHASESHIFT_TEST_SOURCE_DIR;
}

phaseshift::dev::ab::tester::tester(int nb_iter) {
    m_nb_iter = nb_iter;
    // std::cout << "INFO: #Iterations: " << m_nb_iter << std::endl;
}

void phaseshift::dev::ab::tester::randomize_params_chunk_size(std::mt19937* pgen, int iter, int chunk_size_min, int chunk_size_max) {
    std::uniform_int_distribution<int> rnd_chunk_size(chunk_size_min, chunk_size_max);
    if (iter == 1)      m_param_chunk_size = 80;
    else if (iter == 2) m_param_chunk_size = 320;
    else                m_param_chunk_size = rnd_chunk_size(*pgen);
}

void phaseshift::dev::ab::tester::randomize_params(std::mt19937* pgen, int iter) {
    randomize_params_chunk_size(pgen, iter, 1, 16000);
}
void phaseshift::dev::ab::tester::iteration_initialize() {
}
void phaseshift::dev::ab::tester::iteration_proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    (void)in;
    (void)pout;
}
void phaseshift::dev::ab::tester::iteration_finalize(phaseshift::ringbuffer<float>* pout) {
    (void)pout;
}
void phaseshift::dev::ab::tester::iteration_tests() {
}
void phaseshift::dev::ab::tester::final_tests() {
}

void phaseshift::dev::ab::tester::run() {

    m_file_path_in = file_test_source_dir()+"/test_data/wav/arctic_a0204.wav";
    // std::cout << "INFO: file_path_in=" << m_file_path_in << std::endl;

    m_file_reader_builder.set_file_path(m_file_path_in);

    PHASESHIFT_PROF(acbench::time_elapsed m_file_reader_te;)  // To keep track for time elapsed across iterations

    m_fs = phaseshift::ab::sndfile_reader::get_fs(m_file_path_in);
    m_file_in.resize_allocation(10*m_fs);
    phaseshift::ab::sndfile_reader::read(m_file_path_in, &m_file_in);
    phaseshift::dev::signals_check_nan_inf(m_file_in);

    // std::random_device rd;   // a seed source for the random number engine
    std::mt19937 gen(0);     // Repeatable (otherwise us rd())
    for (int iter=1; iter <= m_nb_iter; ++iter) {

        randomize_params(&gen, iter);

        // Initialize -------------------------------------------------------------
        PHASESHIFT_PROF(m_abs.initialize.start();)

        auto file_reader = m_file_reader_builder.open();

        m_file_out.resize_allocation(10*fs());         // TODO(GD) 10s max

        phaseshift::ringbuffer<float> buffer_in;
        buffer_in.resize_allocation(m_param_chunk_size);

        PHASESHIFT_PROF(file_reader->dbg_proc_time.merge(m_file_reader_te);)

        iteration_initialize();

        PHASESHIFT_PROF(m_abs.initialize.end(0.0f);)

        // Loop -------------------------------------------------------------------
        PHASESHIFT_PROF(m_abs.loop.start();)
        phaseshift::globalcursor_t nb_samples_total = 0;
        while (file_reader->read(&buffer_in, m_param_chunk_size) > 0) {
            nb_samples_total += buffer_in.size();

            iteration_proc(buffer_in, &m_file_out);

            buffer_in.clear();
        }
        PHASESHIFT_PROF(m_abs.loop.end(m_file_in.size()/fs());)

        // Finalize ---------------------------------------------------------------
        PHASESHIFT_PROF(m_abs.finalize.start();)

        iteration_finalize(&m_file_out);

        REQUIRE_TS(file_reader->length() == nb_samples_total);

        PHASESHIFT_PROF(m_file_reader_te = file_reader->time_elapsed();)  // Remember what the statistics just before destroying it
        delete file_reader;

        PHASESHIFT_PROF(m_abs.finalize.end(0.0f);)

        phaseshift::dev::signals_check_nan_inf(m_file_out);

        // Additional tests in case of resynthesis
        if (m_test_resynthesis_err_threshold_db < 0.0f) {
            // All samples should be lower than threshold
            REQUIRE_TS(phaseshift::dev::signals_equal_strictly(m_file_in, m_file_out, phaseshift::db2lin(m_test_resynthesis_err_threshold_db)));
        }

        PHASESHIFT_PROF(m_abs.loop_add("ab_file_reader", &m_file_reader_te);)
        iteration_tests();

        m_file_out.clear();
    }

    PHASESHIFT_PROF(m_abs.print();)
    // REQUIRE(m_abs.initialize.median()*1e3 < 1.0);  // <1ms
    // REQUIRE(m_abs.finalize.median()*1e3 < 1.0);    // <1ms
}
