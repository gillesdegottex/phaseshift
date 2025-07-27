// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ola.h>
#include <phaseshift/sigproc/sigproc.h>

void phaseshift::ab::ola::proc_frame(const phaseshift::vector<float>& in, phaseshift::vector<float>* pout, const phaseshift::ab::ola::proc_status& status, phaseshift::globalcursor_t win_center_idx) {
    (void)status;
    (void)win_center_idx;
    phaseshift::vector<float>& out = *pout;
    PHASESHIFT_PROF(dbg_proc_frame_time.start();)

    // Do some processing here
    // Apply at least the window
    out = in;
    out *= win();  // TODO(GD) SPEEDUP Using Eigen

    PHASESHIFT_PROF(dbg_proc_frame_time.end(0.0f);)
}

phaseshift::ab::ola::ola() {
}

phaseshift::ab::ola::~ola() {
}

void phaseshift::ab::ola::proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_flush) {

    m_frame_input = m_frame_rolling;
    assert(m_frame_input.size() > 0 && "phaseshift::audio_block::ola::proc: The input frame is empty.");

    proc_frame(m_frame_input, &m_frame_output, m_status, m_win_center_idx);
    assert(m_frame_output.size() > 0 && "phaseshift::audio_block::ola::proc: The output frame is empty.");
    m_status.first_frame = false;

    #ifndef NDEBUG
        for (int n=0; n<static_cast<int>(m_frame_output.size()); ++n) {
            assert(!std::isnan(m_frame_output[n]));
            assert(!std::isinf(m_frame_output[n]));
            assert(std::abs(m_frame_output[n]) < 1000.0f);
        }
    #endif

    // Add the content of the window and its shape
    m_out_sum += m_frame_output;
    m_out_sum_win += m_win;

    // There are timestep samples that we can flush
    int nb_samples_to_flush_remains = nb_samples_to_flush;
    if (m_first_frame_at_t0_samples_to_skip > 0) {
        // TODO(GD) This skipping is only for offline mode. -> Make a separate dedicated class?
        int nb_topop = std::min<int>(m_first_frame_at_t0_samples_to_skip,nb_samples_to_flush);
        m_out_sum.pop_front(nb_topop);
        m_out_sum_win.pop_front(nb_topop);
        nb_samples_to_flush_remains -= nb_topop;
        m_first_frame_at_t0_samples_to_skip -= nb_topop;
    }

    // Flush the samples that can be flushed
    // TODO(GD) This doesnt respect perfect reconstruction.
    for (int n=0; n<nb_samples_to_flush_remains; ++n) {
        if (m_out_sum_win[n]<2*phaseshift::float32::eps()) {
            m_out_sum_win[n] = 1.0f;
        }
    }
    m_out_sum.divide_equal_range(m_out_sum_win, nb_samples_to_flush_remains);

    #ifndef NDEBUG
        for (int n=0; n<nb_samples_to_flush_remains; ++n) {
            assert(!std::isnan(m_out_sum[n]));
            assert(!std::isinf(m_out_sum[n]));
            assert(std::abs(m_out_sum[n]) < 1000.0f && "The output signal is suspiciously large. Did you forget to apply a window?");
        }
    #endif

    assert(pout->size()+nb_samples_to_flush_remains <= pout->size_max() && "phaseshift::audio_block::ola::proc_win: There is not enough space in the output buffer");

    pout->push_back(m_out_sum, 0, nb_samples_to_flush_remains);
    m_out_sum.pop_front(nb_samples_to_flush_remains);
    m_out_sum_win.pop_front(nb_samples_to_flush_remains);

    // Prepare for next one
    m_out_sum.push_back(0.0f, nb_samples_to_flush);
    m_out_sum_win.push_back(0.0f, nb_samples_to_flush);
    m_frame_rolling.pop_front(m_timestep);

    m_win_center_idx += m_timestep;
}

void phaseshift::ab::ola::proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    assert(pout->size_max() >= m_timestep && "phaseshift::ab::ola::proc: There is not enough space in the output buffer.");  // TODO(GD) uh? sure of the condition?
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

            proc_win(pout, m_timestep);
        }
    }

    proc_time_end(in.size()/fs());
}

void phaseshift::ab::ola::flush(phaseshift::ringbuffer<float>* pout) {

    assert(pout->size_max() >= m_frame_rolling.size() && "phaseshift::ab::ola::flush: There is not enough space in the output buffer.");

    if (m_frame_rolling.size() == 0)
        return;

    // Total number of samples of the previous inputs, which remains to be processed
    int nb_samples_to_flush_total = m_frame_rolling.size();
    nb_samples_to_flush_total += m_extra_samples_to_flush;

    // Bcs proc(.) will be called before, it will always be smaller than m_winlen
    assert((m_frame_rolling.size() < winlen()) && "phaseshift::ab::ola::flush: There are more samples in the internal buffer than winlen. Have you called proc(.) at least once before calling flush(.)?");

    // We know here that there are not enough samples to fill a full window
    // The chosen strategy in the following is to process extra uncomplete windows, as long as the number of samples to flush is smaller than the timestep.
    // This implies also to flush timestep samples, except for the last iteration, where is less or equal than timestep.
    int nb_samples_to_flush = m_timestep;
    do {
        // Add trailing zeros to fill a full window
        m_frame_rolling.push_back(0.0f, winlen() - m_frame_rolling.size());

        // Flush timestep samples, except for the last iteration
        if (nb_samples_to_flush_total <= m_timestep) {
            nb_samples_to_flush = nb_samples_to_flush_total;
            m_status.last_frame = true;
        }

        m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
        m_status.fully_covered_by_window = false;
        m_status.flushing = true;
        proc_win(pout, nb_samples_to_flush);

        nb_samples_to_flush_total -= nb_samples_to_flush;

    } while (nb_samples_to_flush_total > 0);
}


phaseshift::ab::ola* phaseshift::ab::ola_builder::build(phaseshift::ab::ola* pab) {
    build_time_start();
    phaseshift::audio_block_builder::build(pab);

    pab->m_fs = fs();

    if (m_timestep < 0)
        m_timestep = static_cast<int>(fs()*0.005);
    assert((timestep() > 0) && "time step has to be >0");
    pab->m_timestep = timestep();

    if (m_winlen < 0)
        m_winlen = static_cast<int>(fs()*0.010);
    assert((m_winlen > 0) && "phaseshift::ab::ola_builder::build: winlen has to be >0");
    assert((m_winlen > m_timestep) && "phaseshift::ab::ola_builder::build: time step has to be smaller or equal to window's length");

    pab->m_frame_rolling.resize_allocation(m_winlen);
    pab->m_frame_rolling.clear();

    pab->m_frame_input.resize_allocation(m_winlen);
    pab->m_frame_input.resize(m_winlen);
    pab->m_frame_input.clear();

    pab->m_frame_output.resize_allocation(m_winlen);
    pab->m_frame_output.resize(m_winlen);
    pab->m_frame_output.clear();

    pab->m_out_sum.resize_allocation(m_winlen);
    pab->m_out_sum.clear();
    pab->m_out_sum_win.resize_allocation(m_winlen);
    pab->m_out_sum_win.clear();

    pab->m_win.resize_allocation(m_winlen);
    phaseshift::win_hamming(&(pab->m_win), m_winlen);                                       // Default to Hamming window

    if (m_first_frame_at_t0) {
        pab->m_first_frame_at_t0_samples_to_skip = (m_winlen-1)/2;
        pab->m_frame_rolling.push_back(0.0f, pab->m_first_frame_at_t0_samples_to_skip);
    } else {
        pab->m_first_frame_at_t0_samples_to_skip = 0;
    }
    pab->m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;
    pab->m_extra_samples_to_flush = m_extra_samples_to_flush;

    pab->m_out_sum.push_back(0.0f, m_winlen);
    pab->m_out_sum_win.push_back(0.0f, m_winlen);

    pab->m_status.first_frame = true;
    pab->m_status.last_frame = false;
    pab->m_status.skipping_samples_at_start = pab->m_first_frame_at_t0_samples_to_skip > 0;
    pab->m_status.fully_covered_by_window = pab->m_first_frame_at_t0_samples_to_skip == 0;
    pab->m_status.flushing = false;
    pab->m_win_center_idx = 0;

    build_time_end();
    return pab;
}
