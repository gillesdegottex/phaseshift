// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ol.h>
#include <phaseshift/sigproc/window_functions.h>

void phaseshift::ab::ol::proc_frame(const phaseshift::vector<float>& in, const phaseshift::ab::ol::proc_status& status, phaseshift::globalcursor_t win_center_idx) {
    (void)in;
    (void)status;
    (void)win_center_idx;
    PHASESHIFT_PROF(dbg_proc_frame_time.start();)

    PHASESHIFT_PROF(dbg_proc_frame_time.end(0.0f);)
}

phaseshift::ab::ol::ol() {
}

phaseshift::ab::ol::~ol() {
}

void phaseshift::ab::ol::proc_win(int nb_samples_to_flush) {

    m_frame_input = m_frame_rolling;
    assert(m_frame_input.size() > 0 && "phaseshift::audio_block::ol::proc: The input frame is empty.");

    proc_frame(m_frame_input, m_status, m_win_center_idx);
    m_status.first_frame = false;

    // There are timestep samples that we can flush
    int nb_samples_to_flush_remains = nb_samples_to_flush;
    if (m_first_frame_at_t0_samples_to_skip > 0) {
        // TODO(GD) This skipping is only for offline mode. -> Make a separate dedicated class?
        int nb_topop = std::min<int>(m_first_frame_at_t0_samples_to_skip,nb_samples_to_flush);
        nb_samples_to_flush_remains -= nb_topop;
        m_first_frame_at_t0_samples_to_skip -= nb_topop;
    }

    // Prepare for next one
    m_frame_rolling.pop_front(m_timestep);

    m_win_center_idx += m_timestep;
}

void phaseshift::ab::ol::proc(const phaseshift::ringbuffer<float>& in) {
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

void phaseshift::ab::ol::flush() {

    if (m_frame_rolling.size() == 0)
        return;

    // Total number of samples of the previous inputs, which remains to be processed
    int nb_samples_to_flush_total = m_frame_rolling.size();
    nb_samples_to_flush_total += m_extra_samples_to_flush;

    // Bcs proc(.) will be called before, it will always be smaller than m_winlen
    assert((m_frame_rolling.size() < winlen()) && "phaseshift::ab::ol::flush: There are more samples in the internal buffer than winlen. Have you called proc(.) at least once before calling flush(.)?");

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

void phaseshift::ab::ol::reset() {
    m_frame_rolling.clear();
    m_frame_input.clear();

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

phaseshift::ab::ol* phaseshift::ab::ol_builder::build(phaseshift::ab::ol* pab) {
    build_time_start();
    phaseshift::audio_block_builder::build(pab);

    pab->m_fs = fs();

    if (m_timestep < 0)
        m_timestep = static_cast<int>(fs()*0.005);
    assert((timestep() > 0) && "time step has to be >0");
    pab->m_timestep = timestep();

    if (m_winlen < 0)
        m_winlen = static_cast<int>(fs()*0.010);
    assert((m_winlen > 0) && "phaseshift::ab::ol_builder::build: winlen has to be >0");
    assert((m_winlen > m_timestep) && "phaseshift::ab::ol_builder::build: time step has to be smaller or equal to window's length");

    pab->m_frame_rolling.resize_allocation(m_winlen);
    pab->m_frame_rolling.clear();

    pab->m_frame_input.resize_allocation(m_winlen);
    pab->m_frame_input.clear();

    pab->m_win.resize_allocation(m_winlen);
    phaseshift::win_hamming(&(pab->m_win), m_winlen);           // Default to Hamming windows

    pab->m_first_frame_at_t0 = m_first_frame_at_t0;
    pab->m_extra_samples_to_skip = m_extra_samples_to_skip;
    pab->m_extra_samples_to_flush = m_extra_samples_to_flush;

    pab->reset();

    build_time_end();
    return pab;
}
