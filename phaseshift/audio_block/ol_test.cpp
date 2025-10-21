// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ol.h>
#include <phaseshift/audio_block/tester.h>
#include <phaseshift/dev/catch2_extra.h>

#include <thread>
#include <atomic>
#include <iostream>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

class ol_with_extra_tests : public phaseshift::ab::ol {
 public:
    int nbcalls = 0;
    phaseshift::globalcursor_t wavsize = 0;
    phaseshift::globalcursor_t win_center_idx_prev = 0;

    ol_with_extra_tests() {}

    virtual void proc_frame(const phaseshift::vector<float>& in, const phaseshift::ab::ol::proc_status& status, phaseshift::globalcursor_t win_center_idx) {
        (void)status;
        (void)win_center_idx;

        // Distances from one window's center to the next has to be of timestep samples
        if (win_center_idx > 0) {
            REQUIRE_TS((win_center_idx-win_center_idx_prev) == static_cast<phaseshift::globalcursor_t>(timestep()));
        }
        win_center_idx_prev = win_center_idx;

        if (status.last_frame) {
            // DOUT << "LAST FRAME: win_center_idx=" << win_center_idx << " wavsize=" << wavsize << " timestep=" << timestep() << std::endl;
            REQUIRE_TS(win_center_idx >= static_cast<phaseshift::globalcursor_t>(wavsize-1-timestep()));
        }
        // DOUT << "win_center_idx=" << win_center_idx << " wavsize=" << wavsize << " winlen=" << winlen() << " timestep=" << timestep() << std::endl;
        REQUIRE_TS(win_center_idx >= 0); // Window's center are all on or after first sample
        REQUIRE_TS(win_center_idx <= wavsize+winlen()/2+1); // Window's centers are all on or before last sample

        nbcalls++;
    }
};

class ol_with_extra_tests_builder : public phaseshift::ab::ol_builder {
 public:
    ol_with_extra_tests* build(ol_with_extra_tests* pab) {
        phaseshift::ab::ol_builder::build(pab);
        pab->nbcalls = 0;
        pab->wavsize = 0;
        pab->win_center_idx_prev = 0;
        return pab;
    }
    ol_with_extra_tests* build() {return build(new ol_with_extra_tests());}
};

struct tester_ol : public phaseshift::dev::ab::tester {
    tester_ol()
        : phaseshift::dev::ab::tester(100) {  // Number of iterations
    }

    // Da stuff to test
    ol_with_extra_tests_builder m_audio_block_builder;
    ol_with_extra_tests* m_audio_block = nullptr;
    PHASESHIFT_PROF(acbench::time_elapsed m_ab_te;)  // To keep track for time elapsed of ab across iterations

    // The parameters to randomize
    int m_param_winlen;
    int m_param_timestep;

    virtual void randomize_params(std::mt19937* pgen, int iter)=0;

    virtual void randomize_params_limits(std::mt19937* pgen, int iter, int chunk_size_min, int chunk_size_max, int winlen_min, int winlen_max, int timestep_min) {
        phaseshift::dev::ab::tester::randomize_params_chunk_size(pgen, iter, chunk_size_min, chunk_size_max);

        std::uniform_int_distribution<int> rnd_winlen(winlen_min, winlen_max);
        if (iter%51==0)         m_param_winlen = winlen_min;
        else if (iter%52==0)    m_param_winlen = winlen_max;
        else                    m_param_winlen = rnd_winlen(*pgen);

        std::uniform_int_distribution<int> rnd_timestep(timestep_min, int((m_param_winlen-1)/2));
        if (iter%100==0)        m_param_timestep = timestep_min;
        else                    m_param_timestep = rnd_timestep(*pgen);

        // TODO(GD) Build parameters combo

        // DOUT << "INFO: iter=" << iter << " chunk_size=" << m_param_chunk_size << " winlen=" << m_param_winlen << " timestep=" << m_param_timestep << std::endl;
    }

    virtual void iteration_initialize() {
        m_audio_block_builder.set_fs(fs());
        m_audio_block_builder.set_winlen(m_param_winlen);
        m_audio_block_builder.set_timestep(m_param_timestep);

        m_audio_block = m_audio_block_builder.build();
        // DOUT << "DEBUG: m_audio_block.fs()=" << m_audio_block->fs() << std::endl;
        m_audio_block->wavsize = file_in().size();
        PHASESHIFT_PROF(m_audio_block->dbg_proc_time.merge(m_ab_te);)
    }
    virtual void iteration_proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
        m_audio_block->proc(in);
    }
    virtual void iteration_finalize(phaseshift::ringbuffer<float>* pout) {
        m_audio_block->flush();

        PHASESHIFT_PROF(m_ab_te = m_audio_block->time_elapsed();)  // Remember what the statistics just before destroying it, in order to accumulate them later one.

        REQUIRE_TS(m_audio_block->nbcalls > 0);

        delete m_audio_block;
    }
    virtual void iteration_tests() {
        PHASESHIFT_PROF(m_abs.loop_add("ab_ol", &m_ab_te);)
    }

    virtual void final_tests() {
        // REQUIRE(audio_block.time_elapsed().median() < 10*audio_block_file_reader.time_elapsed().median()); // Sometimes break // TODO(GD) Test not reliable, a lot of false alarms
    }
};

struct tester_ol_bugs : public tester_ol {
    virtual void randomize_params(std::mt19937* pgen, int iter) {
        randomize_params_limits(pgen, iter, 1, 16000,   3, 1600,   1);
    }
};
TEST_CASE("audio_block_ol_bugs", "[audio_block_ol_bugs]") {
    phaseshift::dev::check_compilation_options();
    tester_ol_bugs().run();
}

// struct tester_ol_speed : public tester_ol {
//     virtual void randomize_params(std::mt19937* pgen, int iter) {
//         randomize_params_limits(pgen, iter, 16,  800, 160,  800,  16);
//     }
// };
// TEST_CASE("audio_block_ol_speed", "[audio_block_ol_speed]") {
//     phaseshift::dev::check_compilation_options();
//     tester_ol_speed().run();
// }

struct tester_ol_multithread : public tester_ol {
    virtual void randomize_params(std::mt19937* pgen, int iter) {
        randomize_params_limits(pgen, iter, 64, 512, 320, 400, 64);
    }
};
std::atomic<bool> g_tester_ol_multithread_go = false;
std::atomic<int> g_tester_ol_multithread_nb_ready = 0;
void tester_ol_multithread_thread() {
    // Wait for all thread to be ready before starting init and proc
    g_tester_ol_multithread_nb_ready++;
    while ( !g_tester_ol_multithread_go ) {
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
    }
    tester_ol_multithread().run();
}
TEST_CASE("audio_block_ol_multithread", "[audio_block_ol_multithread]") {
    phaseshift::dev::check_compilation_options();

    g_tester_ol_multithread_go = false;
    std::vector<std::thread> threads;
    for (int nt=0; nt<8; ++nt) {
        threads.push_back(std::thread(tester_ol_multithread_thread));

        // Wait for the new thread to start before starting the next one
        // to be sure that
        while ( !(threads[nt].joinable()) ) {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
        }
    }

    // Wait for all to get ready...
    while ( g_tester_ol_multithread_nb_ready < threads.size() ) {
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
    }
    g_tester_ol_multithread_go = true;

    // Wait for all to finish
    for (auto&& thread : threads) {
        thread.join();
    }
}

TEST_CASE("audio_block_ol_proc_reset", "[audio_block_ol_proc_reset]") {
    phaseshift::dev::check_compilation_options();
    
    int repeat = 5;

    std::vector<int> fss = {8000, 16000, 32000, 44100, 48000, 96000};
    for (const int fs : fss) {

        const int winlen = fs*0.020;
        const int timestep = fs*0.005;
        const int test_signal_length = 3*fs;  // 3 seconds
        
        // Create OLA instance
        ol_with_extra_tests_builder builder;
        builder.set_fs(fs);
        builder.set_winlen(winlen);
        builder.set_timestep(timestep);
        builder.set_first_frame_at_t0(true);

        std::vector<int> chunk_sizes = {8, 64, 128, 384, 512, 1024, 4096};
        for (const int chunk_size : chunk_sizes) {

            ol_with_extra_tests* ol_instance = builder.build();
            ol_instance->wavsize = test_signal_length;

            // Create test signal - a sine wave
            phaseshift::ringbuffer<float> input_signal;
            input_signal.resize_allocation(test_signal_length);
            input_signal.clear();

            const float frequency = 440.0f; // A4 note
            for (int i = 0; i < test_signal_length; ++i) {
                float sample = 0.5f * std::sin(2.0f * M_PI * frequency * i / fs);
                input_signal.push_back(sample);
            }

            phaseshift::ringbuffer<float> output_signal_ref;
            output_signal_ref.resize_allocation(test_signal_length);

            for (int rep=0; rep<repeat; ++rep) {
                // DOUT << "fs=" << fs << ", chunk_size=" << chunk_size << ", repeat=" << rep << std::endl;

                // Test proc_in_out_same_size - processing chunk by chunk
                phaseshift::ringbuffer<float> output_signal;
                output_signal.resize_allocation(test_signal_length);
                output_signal.clear();

                // Process input signal chunk by chunk
                for (int i = 0; i < test_signal_length; i += chunk_size) {
                    int current_chunk_size = std::min(chunk_size, test_signal_length - i);
                    
                    phaseshift::ringbuffer<float> input_chunk, output_chunk;
                    input_chunk.resize_allocation(current_chunk_size);
                    input_chunk.push_back(input_signal, i, current_chunk_size);

                    ol_instance->proc(input_chunk);
                }

                // Verify that the OLA instance was called (frame processing occurred)
                REQUIRE_TS(ol_instance->nbcalls > 0);

                // phaseshift::ab::sndfile_writer::write("flop.in.chunk." + std::to_string(chunk_size) + ".wav", fs, input_signal);
                // phaseshift::ab::sndfile_writer::write("flop.out.chunk." + std::to_string(chunk_size) + ".wav", fs, output_signal);

                ol_instance->reset();
            }

            delete ol_instance;
        }
    }
}
