// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ol.h>
#include <phaseshift/sigproc/window_functions.h>
#include <phaseshift/sigproc/sigproc.h>

// TODO TODO TODO Remove win_center_idx, like in OLA
void phaseshift::ol::proc_frame(const phaseshift::vector<float>& in, const phaseshift::ol::proc_status& status, phaseshift::globalcursor_t win_center_idx) {
    (void)in;
    (void)status;
    (void)win_center_idx;
    PHASESHIFT_PROF(dbg_proc_frame_time.start();)

    PHASESHIFT_PROF(dbg_proc_frame_time.end(0.0f);)
}

phaseshift::ol::ol() {
}

phaseshift::ol::~ol() {
}

void phaseshift::ol::proc_win(int nb_samples_to_flush) {

    m_frame_input = m_frame_rolling;
    assert(m_frame_input.size() > 0 && "phaseshift::audio_block::ol::proc: The input frame is empty.");

    proc_frame(m_frame_input, m_status, m_win_center_idx);
    m_status.first_frame = false;

    // There are timestep samples that we can flush
    if (m_first_frame_at_t0_samples_to_skip > 0) {
        // TODO(GD) This skipping is only for offline mode. -> Make a separate dedicated class?
        int nb_topop = std::min<int>(m_first_frame_at_t0_samples_to_skip,nb_samples_to_flush);
        m_first_frame_at_t0_samples_to_skip -= nb_topop;
    }

    // Prepare for next one
    m_frame_rolling.pop_front(m_timestep);

    m_win_center_idx += m_timestep;
}

void phaseshift::ol::proc(const phaseshift::ringbuffer<float>& in) {
    proc_time_start();

    int in_n = 0;
    while (in_n < in.size()) {

        // Fill enough for a winlen, without over-reading `in`
        int nb_samples_for_winlen = std::min<int>(winlen() - m_frame_rolling.size(), in.size()-in_n);
        m_frame_rolling.push_back(in, in_n, nb_samples_for_winlen);
        in_n += nb_samples_for_winlen;

        if (m_frame_rolling.size() == winlen()) {
            m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
            m_status.fully_covered_by_window = m_first_frame_at_t0_samples_to_skip == 0;

            proc_win(m_timestep);
        }
    }

    proc_time_end(in.size()/fs());
}

void phaseshift::ol::flush() {

    if (m_frame_rolling.size() == 0)
        return;

    // Total number of samples of the previous inputs, which remains to be processed
    int nb_samples_to_flush_total = m_frame_rolling.size();
    nb_samples_to_flush_total += m_extra_samples_to_flush;

    // Bcs proc(.) will be called before, it will always be smaller than m_winlen
    assert((m_frame_rolling.size() < winlen()) && "phaseshift::ol::flush: There are more samples in the internal buffer than winlen. Have you called proc(.) at least once before calling flush(.)?");

    // We know here that there are not enough samples to fill a full window
    // The chosen strategy in the following is to process extra uncomplete windows, as long as the middle of window lands before or on the very last sample of the input signal.
    // This implies also to flush timestep samples, except for the last iteration.
    int nb_samples_to_flush = m_timestep;
    do {
        // Add trailing zeros to fill a full window
        m_frame_rolling.push_back(0.0f, winlen() - m_frame_rolling.size());

        // Flush timestep samples, except for the last iteration
        if (nb_samples_to_flush_total <= static_cast<int>(winlen()/2+m_timestep)) {  // TODO(GD) Not sure of winlen()/2 anymore
            nb_samples_to_flush = nb_samples_to_flush_total;
            m_status.last_frame = true;
        }

        m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
        m_status.fully_covered_by_window = false;
        m_status.flushing = true;
        proc_win(nb_samples_to_flush);

        nb_samples_to_flush_total -= nb_samples_to_flush;

    } while (nb_samples_to_flush_total > 0);
}

void phaseshift::ol::reset() {
    phaseshift::audio_block::reset();

    assert(m_frame_rolling.size_max() == winlen());
    m_frame_rolling.clear();

    assert(m_frame_input.size_max() == winlen());
    assert(m_frame_input.size() == winlen());
    m_frame_input.clear();

    assert(m_win.size_max() == winlen());
    // phaseshift::win_hamming(&(pab->m_win), m_winlen);  // No need to re-build it.

    if (m_first_frame_at_t0) {
        m_first_frame_at_t0_samples_to_skip = (winlen()-1)/2;
        m_frame_rolling.push_back(0.0f, m_first_frame_at_t0_samples_to_skip);
    } else {
        m_first_frame_at_t0_samples_to_skip = 0;
    }
    m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;

    m_status.first_frame = true;
    m_status.last_frame = false;
    m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
    m_status.fully_covered_by_window = m_first_frame_at_t0_samples_to_skip == 0;
    m_status.flushing = false;
    m_win_center_idx = 0;
}

phaseshift::ol* phaseshift::ol_builder::build(phaseshift::ol* pab) {
    build_time_start();
    phaseshift::audio_block_builder::build(pab);

    pab->m_fs = fs();

    if (m_timestep < 0)
        m_timestep = static_cast<int>(fs()*0.005);
    assert((timestep() > 0) && "time step has to be >0");
    pab->m_timestep = timestep();

    if (m_winlen < 0)
        m_winlen = static_cast<int>(fs()*0.010);
    assert((m_winlen > 0) && "winlen has to be >0");
    assert((m_winlen > m_timestep) && "phaseshift::ol_builder::build: time step has to be smaller or equal to window's length");

    pab->m_frame_rolling.resize_allocation(m_winlen);
    pab->m_frame_rolling.clear();

    pab->m_frame_input.resize_allocation(m_winlen);
    pab->m_frame_input.resize(m_winlen);
    pab->m_frame_input.clear();

    pab->m_win.resize_allocation(m_winlen);
    phaseshift::win_hamming(&(pab->m_win), m_winlen);                                       // Default to Hamming windows

    pab->m_first_frame_at_t0 = m_first_frame_at_t0;
    if (m_first_frame_at_t0) {
        pab->m_first_frame_at_t0_samples_to_skip = (m_winlen-1)/2;
        pab->m_frame_rolling.push_back(0.0f, pab->m_first_frame_at_t0_samples_to_skip);
    } else {
        pab->m_first_frame_at_t0_samples_to_skip = 0;
    }
    pab->m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;
    pab->m_extra_samples_to_flush = m_extra_samples_to_flush;

    pab->m_status.first_frame = true;
    pab->m_status.last_frame = false;
    pab->m_status.skipping_samples_at_start = pab->m_first_frame_at_t0_samples_to_skip > 0;
    pab->m_status.fully_covered_by_window = pab->m_first_frame_at_t0_samples_to_skip == 0;
    pab->m_status.flushing = false;
    pab->m_win_center_idx = 0;

    build_time_end();
    return pab;
}

// Tests ----------------------------------------------------------------------

// TODO(GD) Factor with audio_block_ola_test code?
void phaseshift::dev::audio_block_ol_test(phaseshift::ol* pab, int chunk_size) {

    float duration_s = 3.0f;

    // Static tests
    phaseshift::dev::test_require(pab->fs() > 0.0f, "audio_block_ol_test: fs() <= 0.0f");
    phaseshift::dev::test_require(pab->latency() >= 0, "audio_block_ol_test: latency() < 0");

    // std::random_device rd;   // a seed source for the random number engine
    // std::mt19937 gen(rd());        // Repeatable (otherwise us rd())
    std::mt19937 gen(0);        // Repeatable (otherwise us rd())

    float fs = pab->fs();

    enum {mode_offline, mode_streaming, mode_realtime};
    enum {synth_noise, synth_silence, synth_click, synth_saturated, synth_sin, synth_harmonics};

    for (int mode = mode_offline; mode <= mode_streaming; ++mode) {  // TODO(GD) Add mode_realtime

        for (int synth = synth_noise; synth <= synth_harmonics; ++synth) {

            for (int iter=1; iter <= 3; ++iter) {

                // DOUT << "mode=" << mode << ", synth=" << synth << ", iter=" << iter << std::endl;

                // Generate input signal ------------------------------

                phaseshift::ringbuffer<float> signal_in;
                signal_in.resize_allocation(fs * duration_s);
                signal_in.clear();
                if (synth == synth_noise) {
                    phaseshift::push_back_noise_normal(signal_in, signal_in.capacity(), gen, 0.0f, 0.2f, 0.99f);
                } else if (synth == synth_silence) {
                    signal_in.push_back(0.0f, signal_in.capacity());
                    signal_in[0] = 0.0f;
                } else if (synth == synth_click) {
                    signal_in.push_back(0.0f, signal_in.capacity());
                    signal_in[0] = 0.9f;
                } else if (synth == synth_saturated) {
                    signal_in.push_back(0.0f, signal_in.capacity());
                    signal_in[0] = 1.0f;
                } else if (synth == synth_sin) {
                    signal_in.push_back(0.0f, signal_in.capacity());
                    for (int n = 0; n < signal_in.size(); ++n) {
                        signal_in[n] = 0.9f * std::sin(2.0f * M_PI * 440.0f * n / fs);
                    }
                } else if (synth == synth_harmonics) {
                    signal_in.push_back(0.0f, signal_in.capacity());
                    float f0 = 110.0f;
                    float nb_harmonics = int((0.5*fs-f0)/f0);
                    float amplitude = 0.9f/nb_harmonics;
                    for (int n = 0; n < signal_in.size(); ++n) {
                        for (int h = 0; h <= nb_harmonics; ++h) {
                            signal_in[n] = amplitude * std::sin(2.0f * M_PI * h * f0 * n / fs);
                        }
                    }
                }

                // Initialize -----------------------------------------

                phaseshift::globalcursor_t nb_samples_total = 0;

                if (mode == int(mode_offline)) {

                    pab->proc(signal_in);
                    pab->flush();

                    nb_samples_total += signal_in.size();

                } else if (mode == int(mode_streaming)) {

                    phaseshift::ringbuffer<float> chunk_in;
                    chunk_in.resize_allocation(chunk_size);

                    while (nb_samples_total < signal_in.size()) {

                        chunk_in.clear();
                        int chunk_size_to_push = std::min<int>(chunk_size, signal_in.size() - nb_samples_total);
                        chunk_in.push_back(signal_in, nb_samples_total, chunk_size_to_push);
                        nb_samples_total += chunk_size_to_push;

                        pab->proc(chunk_in);
                    }
                    pab->flush();

                // } else if (mode == int(mode_realtime)) {  // TODO(GD) Implement

                //     phaseshift::ringbuffer<float> chunk_in, chunk_out;
                //     chunk_in.resize_allocation(chunk_size);
                //     chunk_out.resize_allocation(chunk_size);

                //     while (nb_samples_total < signal_in.size()) {

                //         chunk_in.clear();
                //         int chunk_size_to_push = std::min<int>(chunk_size, signal_in.size() - nb_samples_total);
                //         chunk_in.push_back(signal_in, nb_samples_total, chunk_size_to_push);
                //         nb_samples_total += chunk_size_to_push;

                //         chunk_out.clear();
                //         pab->proc_same_size(chunk_in, &chunk_out);
                //         // assert(chunk_out.size() == chunk_size);  // signal is not integer multiple of chunk size, so the last chunk will be smaller than chunk_size
                //         phaseshift::dev::test_require(chunk_out.size() == chunk_in.size(), "audio_block_ol_test: chunk_out.size() != chunk_in.size()");
            
                //         signal_out.push_back(chunk_out);
                //     }
                }


                // Finalize -------------------------------------------

                // phaseshift::dev::test_require(pab->stat_rt_nb_failed() == 0, "audio_block_ol_test: stat_rt_nb_failed() != 0");  // TODO(GD) Implement with proc_same_size(.)

                phaseshift::dev::test_require(signal_in.size() == nb_samples_total, "audio_block_ol_test: signal_in.size() != nb_samples_total");

                if ((mode == int(mode_offline)) || (mode == int(mode_streaming))) {
                    // Anything to test?
                } else if (mode == int(mode_realtime)) {
                    // Anything to test?
                }

                pab->reset();
            }
        }
    }
}

void phaseshift::dev::audio_block_ol_builder_test_singlethread() {

    struct test_params {
        float fs;
        int timestep;
        int winlen;
        int chunk_size;
    };

    const std::vector<test_params> test_combinations = {
        // Standard combinations
        {44100, 220, 882, 256},
        {16000, 64, 512, 32},

        // More edgy
        {8000,  1, 3, 2},
        {22050, 256, 384, 128},
        {96000, 96, 4800, 1024},
    };

    auto pbuilder = new phaseshift::ol_builder();

    for (const auto& [fs, timestep, winlen, chunk_size] : test_combinations) {

        pbuilder->set_fs(fs);
        pbuilder->set_timestep(timestep);
        pbuilder->set_winlen(winlen);
        // pbuilder->set_in_out_same_size_max(chunk_size);  // TODO(GD) Implement with proc_same_size(.)
        pbuilder->set_first_frame_at_t0(true);

        // DOUT << "fs=" << fs << ", winlen=" << winlen << ", timestep=" << timestep << ", chunk_size=" << chunk_size << std::endl;

        auto pab = pbuilder->build();

        phaseshift::dev::audio_block_ol_test(pab, chunk_size);

        delete pab;
    }

    delete pbuilder;
}

void phaseshift::dev::audio_block_ol_builder_test(int nb_threads) {
    phaseshift::dev::audio_block_builder_test(phaseshift::dev::audio_block_ol_builder_test_singlethread, nb_threads);
}
