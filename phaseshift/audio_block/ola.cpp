// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ola.h>
#include <phaseshift/audio_block/tinywavfile.h>
#include <phaseshift/sigproc/sigproc.h>

void phaseshift::ola::proc_frame(const phaseshift::vector<float>& in, phaseshift::vector<float>* pout, const phaseshift::ola::proc_status& status) {
    (void)status;
    phaseshift::vector<float>& out = *pout;
    PHASESHIFT_PROF(dbg_proc_frame_time.start();)

    // Do some processing here
    // Apply at least the window
    out = in;
    out *= win();  // TODO(GD) SPEEDUP Using Eigen

    // assert(false && "phaseshift::ola::proc_frame: Not implemented. It must be implemented in the derived class.");  // TODO(GD) It is used in the tests.

    PHASESHIFT_PROF(dbg_proc_frame_time.end(0.0f);)
}

phaseshift::ola::ola() {
}

phaseshift::ola::~ola() {
}

void phaseshift::ola::proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_output) {
    assert(pout!=nullptr && "phaseshift::ola::proc_win: The output buffer is nullptr.");

    m_frame_input = m_frame_rolling;
    assert(m_frame_input.size() > 0 && "phaseshift::ola::proc: The input frame is empty.");

    m_input_win_center_idx = m_input_win_center_idx_next;  // TODO Simplify in status_t
    assert(m_input_win_center_idx >= 0 && "phaseshift::ola::proc: The input window center index is negative.");
    m_status.input_win_center_idx = m_input_win_center_idx;
    m_output_win_center_idx = -m_first_frame_at_t0_samples_to_skip + m_output_length + (winlen()-1)/2;
    m_status.output_win_center_idx = m_output_win_center_idx;

    proc_frame(m_frame_input, &m_frame_output, m_status);
    #ifndef NDEBUG
        for (int n=0; n<static_cast<int>(m_frame_output.size()); ++n) {
            assert(!std::isnan(m_frame_output[n]));
            assert(!std::isinf(m_frame_output[n]));
            assert(std::abs(m_frame_output[n]) < 1000.0f);
        }
    #endif

    m_status.first_input_frame = false;
    m_frame_rolling.pop_front(m_timestep);

    if (m_frame_output.size() > 0) {

        // Add the content of the window and its shape
        m_out_sum += m_frame_output;
        m_out_sum_win += m_win;

        // There are timestep samples that we can flush
        int nb_samples_to_output_remains = nb_samples_to_output;
        if (m_first_frame_at_t0_samples_to_skip > 0) {
            // TODO(GD) This skipping is only for offline mode. -> Make a separate dedicated class?
            int nb_topop = std::min<int>(m_first_frame_at_t0_samples_to_skip,nb_samples_to_output);
            m_out_sum.pop_front(nb_topop);
            m_out_sum_win.pop_front(nb_topop);
            nb_samples_to_output_remains -= nb_topop;
            m_first_frame_at_t0_samples_to_skip -= nb_topop;
        }

        // Flush the samples that can be flushed
        // TODO(GD) Need to ensure perfect reconstruction by:
        //          - Cover the full input past beyond one step size after the last sample of the input signal. Not sure that absolutely necessary since the 2nd condition might be enough.
        //          - Forbid timestep/windlen that do not overlap enough.
        for (int n=0; n<nb_samples_to_output_remains; ++n) {
            if (m_out_sum_win[n] < 2*phaseshift::float32::eps()) {
                m_out_sum_win[n] = 1.0f;
                m_failure_status.nb_imperfect_reconstruction++;
            }
        }
        m_out_sum.divide_equal_range(m_out_sum_win, nb_samples_to_output_remains);

        #ifndef NDEBUG
            for (int n=0; n<nb_samples_to_output_remains; ++n) {
                assert(!std::isnan(m_out_sum[n]));
                assert(!std::isinf(m_out_sum[n]));
                assert(std::abs(m_out_sum[n]) < 1000.0f && "The output signal is suspiciously large. Did you forget to apply a window?");
            }
        #endif

        assert(pout->size()+nb_samples_to_output_remains <= pout->size_max() && "phaseshift::ola::proc_win: There is not enough space in the output buffer");

        // assert((m_output_added_max==-1) || (nb_samples_to_output_remains <= m_output_added_max));
        pout->push_back(m_out_sum, 0, nb_samples_to_output_remains);
        m_output_length += nb_samples_to_output_remains;
        m_out_sum.pop_front(nb_samples_to_output_remains);
        m_out_sum_win.pop_front(nb_samples_to_output_remains);

        // Prepare for next one
        m_out_sum.push_back(0.0f, nb_samples_to_output);
        m_out_sum_win.push_back(0.0f, nb_samples_to_output);
    }
}

void phaseshift::ola::process(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    proc_time_start();

    // assert(!m_status.finished && "phaseshift::ola::process: The audio stream is already finished, there should be any more calls to process(.) nor flush(.)");
    if (m_status.finished) {
        // TODO Gentle warning?
        return;
    }

    if (pout == nullptr) {
        pout = &m_out;
    }

    m_input_length += in.size();

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

            m_input_win_center_idx_next += m_timestep;  // Rdy for next window
        }
    }

    proc_time_end(in.size()/fs());
}

int phaseshift::ola::flush_available() {
    return m_flush_nb_samples_total;
}

void phaseshift::ola::flush(int chunk_size, phaseshift::ringbuffer<float>* pout) {
    proc_time_start();

    // assert(!m_status.finished && "phaseshift::ola::flush: The audio stream is already finished, there should be any more calls to process(.) nor flush(.)");
    if (m_status.finished) {
        // TODO Gentle warning?
        return;
    }

    // If no output buffer is provided, use the internal output buffer, and expect the use of fetch(.) to empty it.
    if (pout == nullptr) {
        pout = &m_out;
    }

    if (!m_status.flushing) {  // First time flushing
        // Number of output samples that remains to be flushed
        m_flush_nb_samples_total = m_frame_rolling.size() + m_extra_samples_to_flush;
        m_status.flushing = true;
    }

    int nb_samples_output = 0;

    // Process windows until we've output enough or finished
    while (m_flush_nb_samples_total > 0) {

        // Check chunk_size limit
        if (chunk_size > 0 && nb_samples_output >= chunk_size) {
            break;
        }

        // Fill rolling buffer to winlen with zeros (limited by chunk_size if set)
        int zeros_needed = winlen() - m_frame_rolling.size();
        if (zeros_needed > 0) {
            if (chunk_size > 0) {
                zeros_needed = std::min(zeros_needed, chunk_size);
            }
            m_frame_rolling.push_back(0.0f, zeros_needed);
        }

        if (m_frame_rolling.size() == winlen()) {
            m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
            m_status.fully_covered_by_window = m_first_frame_at_t0_samples_to_skip == 0;

            // Determine how many samples to output for this window
            int nb_samples_to_flush = m_timestep;
            if (m_flush_nb_samples_total <= m_timestep) {
                nb_samples_to_flush = m_flush_nb_samples_total;
                m_status.last_frame = true;
            }

            proc_win(pout, nb_samples_to_flush);

            m_input_win_center_idx_next += m_timestep;  // Rdy for next window
            nb_samples_output += nb_samples_to_flush;
            m_flush_nb_samples_total -= nb_samples_to_flush;

        } else {
            // Rolling buffer not full yet (zeros limited by chunk_size), continue in next call
            break;
        }
    }

    if (m_flush_nb_samples_total == 0) {
        // Reached the end of the audio stream
        m_frame_rolling.clear();  // flush discontinues the audio stream, so clear the internal buffer.
        // m_extra_samples_to_flush = 0;  // Do not clear this, bcs it is used for reset()
        m_status.finished = true;
        assert(m_flush_nb_samples_total == 0 && "m_flush_nb_samples_total should be 0 when the audio stream is finished");
    }

    proc_time_end(nb_samples_output/fs());
}

int phaseshift::ola::fetch_available() {
    return m_out.size();
}
int phaseshift::ola::fetch(phaseshift::ringbuffer<float>* pout, int chunk_size_max) {

    if (m_out.size() == 0) {
        return 0;
    }

    // int chunk_size = std::min<int>({m_out.size(), pout->size_max() - pout->size()});
    int chunk_size = std::min<int>({m_out.size()});
    if (chunk_size_max > 0) {
        chunk_size = std::min<int>(chunk_size, chunk_size_max);
    }

    pout->push_back(m_out, 0, chunk_size);
    m_out.pop_front(chunk_size);

    return chunk_size;
}

//! WARNING: This function allocates a temporary buffer for building the chunk.
void phaseshift::ola::process_offline(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout, int chunk_size) {

    phaseshift::ringbuffer<float> chunk_in;
    chunk_in.resize_allocation(chunk_size);

    int in_n = 0;
    while (in_n < in.size()) {
        int chunk_to_process = std::min<int>(chunk_size, in.size() - in_n);
        chunk_in.clear();
        chunk_in.push_back(in, in_n, chunk_to_process);
        in_n += chunk_to_process;

        process(chunk_in);

        fetch(pout); // Empty the internal output buffer as much as possible
    }

    // Flush remaining data in chunks
    int fetched = 1;
    while (fetched > 0) {
        flush(chunk_size);
        fetched = fetch(pout, chunk_size);
    }
}
void phaseshift::ola::process_offline(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {

    process(in, pout);
    flush(-1, pout);
}

void phaseshift::ola::process_realtime(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {

    int chunk_size_req = in.size();

    process(in);

    int available = fetch_available();

    assert(m_rt_prepad_latency_remaining >= 0);
    if (m_rt_prepad_latency_remaining > 0) {
        // Pre-pad: pad with zeros up to latency total
        int zeros_to_add = std::min(m_rt_prepad_latency_remaining, chunk_size_req);
        int to_fetch = chunk_size_req - zeros_to_add;

        pout->push_back(0.0f, zeros_to_add);
        m_rt_prepad_latency_remaining -= zeros_to_add;

        if (to_fetch > 0 && available >= to_fetch) {
            fetch(pout, to_fetch);
        }

    } else if (available >= chunk_size_req) {
        // Normal fetch
        fetch(pout, chunk_size_req);

    } else {
        // Post-pad: fetch what's available, pad the rest
        int to_fetch = std::min(chunk_size_req, available);
        fetch(pout, to_fetch);

        int zeros_to_add = chunk_size_req - to_fetch;
        if (zeros_to_add > 0) {
            pout->push_back(0.0f, zeros_to_add);
        }
    }

    m_stat_rt_out_size_min = std::min(m_stat_rt_out_size_min, m_out.size());
}    

void phaseshift::ola::reset() {
    phaseshift::audio_block::reset();

    // Use asserts to verify that the capacity and some of the sizes should not have changed.

    assert(m_frame_rolling.size_max() == winlen());
    m_frame_rolling.clear();

    assert(m_frame_input.size_max() == winlen());
    assert(m_frame_input.size() == winlen());

    assert(m_frame_output.size_max() == winlen());
    assert(m_frame_output.size() == winlen());

    assert(m_out_sum.size_max() == winlen());
    m_out_sum.clear();
    assert(m_out_sum_win.size_max() == winlen());
    m_out_sum_win.clear();

    m_out.clear();

    assert(m_win.size_max() == winlen());
    // phaseshift::win_hamming(&(m_win), winlen);  // Should not be changed, no need to re-build it.

    m_first_frame_at_t0_samples_to_skip = (winlen()-1)/2;
    m_frame_rolling.push_back(0.0f, m_first_frame_at_t0_samples_to_skip);
    m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;

    m_out_sum.push_back(0.0f, winlen());
    m_out_sum_win.push_back(0.0f, winlen());
    m_flush_nb_samples_total = 0;

    m_status.reset();
    m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
    m_status.fully_covered_by_window = m_first_frame_at_t0_samples_to_skip == 0;
    m_input_length = 0;
    m_input_win_center_idx = 0;
    m_input_win_center_idx_next = 0;
    m_output_win_center_idx = 0;
    m_output_length = 0;

    m_rt_prepad_latency_remaining = latency();

    m_stat_rt_nb_failed = 0;
    m_stat_rt_nb_post_underruns = 0;
    m_stat_rt_out_size_min = phaseshift::int32::max();

    failure_status_reset();
}


// Builder --------------------------------------------------------------------

phaseshift::ola* phaseshift::ola_builder::build(phaseshift::ola* pab) {
    build_time_start();
    phaseshift::audio_block_builder::build(pab);

    pab->m_fs = fs();

    if (m_timestep < 0)
        m_timestep = static_cast<int>(fs()*0.005);
    assert((timestep() > 0) && "time step has to be >0");
    pab->m_timestep = timestep();

    if (m_winlen < 0)
        m_winlen = static_cast<int>(fs()*0.010);
    assert((m_winlen > 0) && "phaseshift::ola_builder::build: winlen has to be >0");
    assert((m_winlen > m_timestep) && "phaseshift::ola_builder::build: time step has to be smaller or equal to window's length");

    pab->m_frame_rolling.resize_allocation(m_winlen);
    pab->m_frame_rolling.clear();

    pab->m_frame_input.resize_allocation(m_winlen);
    pab->m_frame_input.resize(m_winlen);

    pab->m_frame_output.resize_allocation(m_winlen);
    pab->m_frame_output.resize(m_winlen);

    pab->m_out_sum.resize_allocation(m_winlen);
    pab->m_out_sum.clear();
    pab->m_out_sum_win.resize_allocation(m_winlen);
    pab->m_out_sum_win.clear();

    int output_buffer_size_max = m_output_buffer_size_max;
    output_buffer_size_max = std::max(output_buffer_size_max, m_winlen+m_timestep);  // TODO Could be smaller?
    pab->m_out.resize_allocation(output_buffer_size_max);
    pab->m_out.clear();

    pab->m_win.resize_allocation(m_winlen);
    // Default to Hamming window, to avoid amplitude modulation by winsum normalisation, and thus gives perfect reconstruction
    phaseshift::win_hamming(&(pab->m_win), m_winlen);

    pab->m_first_frame_at_t0_samples_to_skip = (m_winlen-1)/2;
    pab->m_frame_rolling.push_back(0.0f, pab->m_first_frame_at_t0_samples_to_skip);
    pab->m_extra_samples_to_skip = m_extra_samples_to_skip;
    pab->m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;
    pab->m_extra_samples_to_flush = m_extra_samples_to_flush;

    pab->m_out_sum.push_back(0.0f, m_winlen);
    pab->m_out_sum_win.push_back(0.0f, m_winlen);

    pab->m_status.first_input_frame = true;
    pab->m_status.last_frame = false;
    pab->m_status.skipping_samples_at_start = pab->m_first_frame_at_t0_samples_to_skip > 0;
    pab->m_status.fully_covered_by_window = pab->m_first_frame_at_t0_samples_to_skip == 0;
    pab->m_status.flushing = false;
    pab->m_input_length = 0;
    pab->m_input_win_center_idx = 0;
    pab->m_input_win_center_idx_next = 0;
    pab->m_output_length = 0;
    pab->m_output_win_center_idx = 0;
    pab->m_flush_nb_samples_total = 0;

    // // Only usefull when using proc_same_size(.)
    // pab->m_rt_out_size_max = m_rt_out_size_max;
    // pab->m_rt_out.resize_allocation(2*std::max<int>(m_winlen+m_timestep, m_rt_out_size_max));
    // pab->m_rt_out.clear();
    // // This should NOT be dependent on m_rt_out_size_max.
    // // Otherwise the latency will be dependent on it.
    // // TODO Remaining optimisation: How to minimize test_m_rt_out_size_min using m_winlen and/or m_timestep, but without knowing m_rt_out_size_max? ... is it actually possible?
    // pab->m_rt_out.push_back(0.0f, m_winlen);
    pab->m_stat_rt_nb_failed = 0;
    pab->m_stat_rt_nb_post_underruns = 0;

    pab->phaseshift::ola::reset();

    build_time_end();

    return pab;
}


// Tests ----------------------------------------------------------------------

// TODO(GD) Factor with audio_block_ol_test code?
void phaseshift::dev::audio_block_ola_test(phaseshift::ola* pab, int chunk_size, float resynthesis_threshold, int options) {

    float duration_s = 3.0f;

    // Static tests
    phaseshift::dev::test_require(pab->fs() > 0.0f, "audio_block_ola_test: fs() <= 0.0f");
    phaseshift::dev::test_require(pab->latency() >= 0, "audio_block_ola_test: latency() < 0");

    // std::random_device rd;   // a seed source for the random number engine
    // std::mt19937 gen(rd());        // Repeatable (otherwise us rd())
    std::mt19937 gen(0);        // Repeatable (otherwise us rd())

    float fs = pab->fs();

    enum {mode_offline=0, mode_streaming=1, mode_realtime=2};
    enum {synth_noise=0, synth_silence=1, synth_click=2, synth_saturated=3, synth_sin=4, synth_harmonics=5};

    for (int mode = mode_offline; mode <= mode_realtime; ++mode) {

        for (int synth = synth_noise; synth <= synth_harmonics; ++synth) {

            for (int iter=1; iter <= 3; ++iter) {

                // DOUT << "mode=" << mode << ", synth=" << synth << ", iter=" << iter << std::endl;

                // Generate input signal ------------------------------

                std::uniform_real_distribution<float> phase_dist(0.0f, 1.0f);
                phaseshift::ringbuffer<float> signal_in;
                signal_in.resize_allocation(fs * duration_s);
                signal_in.clear();
                if (synth == synth_noise) {
                    phaseshift::push_back_noise_normal(signal_in, signal_in.size_max(), gen, 0.0f, 0.2f, 0.99f);
                } else if (synth == synth_silence) {
                    signal_in.push_back(0.0f, signal_in.size_max());
                    signal_in[0] = 0.0f;
                } else if (synth == synth_click) {
                    signal_in.push_back(0.0f, signal_in.size_max());
                    signal_in[0] = 0.9f;
                } else if (synth == synth_saturated) {
                    signal_in.push_back(0.0f, signal_in.size_max());
                    signal_in[0] = 1.0f;
                } else if (synth == synth_sin) {
                    signal_in.push_back(0.0f, signal_in.size_max());
                    float phase = 2.0f * M_PI * phase_dist(gen);
                    for (int n = 0; n < signal_in.size(); ++n) {
                        signal_in[n] = 0.9f * std::sin(2.0f * M_PI * 440.0f * n / fs + phase);
                    }
                } else if (synth == synth_harmonics) {
                    signal_in.push_back(0.0f, signal_in.size_max());
                    float f0 = 110.0f;
                    float nb_harmonics = int((0.5*fs-f0)/f0);
                    float amplitude = 0.9f/nb_harmonics;
                    for (int h = 0; h <= nb_harmonics; ++h) {
                        float phase = 2.0f * M_PI * phase_dist(gen);
                        for (int n = 0; n < signal_in.size(); ++n) {
                                signal_in[n] += amplitude * std::sin(2.0f * M_PI * h * f0 * n / fs + phase);
                        }
                    }
                }

                phaseshift::ringbuffer<float> signal_out;
                signal_out.resize_allocation(signal_in.size_max());
                signal_out.clear();

                // Initialize -----------------------------------------

                phaseshift::globalcursor_t nb_samples_total = 0;

                if (mode == int(mode_offline)) {

                    pab->process_offline(signal_in, &signal_out);

                    nb_samples_total = signal_out.size();

                } else if (mode == int(mode_streaming)) {

                    phaseshift::ringbuffer<float> chunk_in;
                    chunk_in.resize_allocation(chunk_size);

                    // Single loop that simulate a callback
                    while (!pab->finished()) {

                        if (pab->input_length() < signal_in.size()) {
                            // Feed input data
                            int chunk_to_process = std::min<int>(chunk_size, signal_in.size() - pab->input_length());
                            chunk_in.clear();
                            chunk_in.push_back(signal_in, pab->input_length(), chunk_to_process);

                            pab->process(chunk_in);

                        } else {
                            pab->flush(chunk_size);
                        }

                        while (pab->fetch_available() > 0) {
                            pab->fetch(&signal_out, chunk_size);
                        }
                    }
                    
                } else if (mode == int(mode_realtime)) {

                    phaseshift::ringbuffer<float> chunk_in;
                    chunk_in.resize_allocation(chunk_size);

                    // Single loop that simulate a callback (ignore flushing)
                    while (signal_out.size() < signal_in.size()) {

                        int chunk_size_req = std::min<int>(chunk_size, signal_in.size() - pab->input_length());
                        chunk_in.clear();
                        chunk_in.push_back(signal_in, pab->input_length(), chunk_size_req);
                        int signal_out_size_before = signal_out.size();

                        pab->process_realtime(chunk_in, &signal_out);

                        int signal_out_size_after = signal_out.size();
                        phaseshift::dev::test_require(chunk_in.size() == signal_out_size_after-signal_out_size_before, "audio_block_ola_test: chunk_in.size() != signal_out_size_after-signal_out_size_before");
                    }
                }

                // Finalize -------------------------------------------

                #if 0
                    DOUT << "signal_in.size()=" << signal_in.size() << ", signal_out.size()=" << signal_out.size() << std::endl;
                    phaseshift::tinywavfile_writer::write("signal_in.wav", fs, signal_in);
                    phaseshift::tinywavfile_writer::write("signal_out.wav", fs, signal_out);
                    if (signal_out.size() == signal_in.size()) {
                        phaseshift::ringbuffer<float> residual;
                        residual.resize_allocation(signal_in.size());
                        residual.clear();
                        residual = signal_in;
                        residual -= signal_out;
                        phaseshift::tinywavfile_writer::write("signal_res.wav", fs, residual);
                    }
                #endif

                phaseshift::dev::test_require(pab->stat_rt_nb_failed() == 0, "audio_block_ola_test: stat_rt_nb_failed() != 0");

                phaseshift::dev::test_require(signal_out.size() > 0, "audio_block_ola_test: signal_out.size() == 0");
                phaseshift::dev::test_require(signal_out.size() == signal_in.size(), "audio_block_ola_test: signal_out.size() != signal_in.size()");

                phaseshift::dev::signals_check_nan_inf(signal_out);

                if ((mode == int(mode_offline)) || (mode == int(mode_streaming))) {

                    phaseshift::dev::test_require(phaseshift::dev::signals_equal_strictly(signal_in, signal_out, resynthesis_threshold), "audio_block_ola_test: signals_equal_strictly() failed");

                } else if (mode == int(mode_realtime)) {

                    phaseshift::dev::test_require(pab->stat_rt_out_size_min() < chunk_size, "audio_block_ola_test: stat_rt_out_size_min() >= chunk_size");

                    if (synth == synth_click) {

                        if (options & option_test_latency) {
                            // When the audio is a click, use it to measure the latency

                            int measured_latency = 0;
                            for (; measured_latency < signal_out.size();) {
                                if (signal_out[measured_latency] > 0.33f) {
                                    break;
                                }
                                measured_latency++;
                            }

                            phaseshift::dev::test_require(measured_latency == pab->latency(), "audio_block_ola_test: measured_latency != latency()");
                        }
                    }
                }

                pab->reset();
            }
        }
    }
}

void phaseshift::dev::audio_block_ola_builder_test_singlethread() {

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

    auto pbuilder = new phaseshift::ola_builder();

    for (const auto& [fs, timestep, winlen, chunk_size] : test_combinations) {

        pbuilder->set_fs(fs);
        pbuilder->set_timestep(timestep);
        pbuilder->set_winlen(winlen);
        pbuilder->set_output_buffer_size_max(chunk_size);

        auto pab = pbuilder->build();

        phaseshift::dev::audio_block_ola_test(pab, chunk_size);

        delete pab;
    }

    delete pbuilder;
}

void phaseshift::dev::audio_block_ola_builder_test(int nb_threads) {
    phaseshift::dev::audio_block_builder_test(phaseshift::dev::audio_block_ola_builder_test_singlethread, nb_threads);
}
