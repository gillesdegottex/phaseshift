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
    out *= win();

    PHASESHIFT_PROF(dbg_proc_frame_time.end(0.0f);)
}

phaseshift::ola::ola() {
}

phaseshift::ola::~ola() {
}

int phaseshift::ola::output_one_frame(phaseshift::ringbuffer<float>* pout, int nb_samples_to_output) {
    assert(pout != nullptr && "phaseshift::ola::output_one_frame: The output buffer is nullptr.");

    // Update status for this frame
    m_status.input_win_center_idx = m_input_win_center_idx;
    m_output_win_center_idx = -m_first_frame_at_t0_samples_to_skip + m_output_length + (winlen()-1)/2;
    m_status.output_win_center_idx = m_output_win_center_idx;

    // Process the frame
    proc_frame(m_frame_input, &m_frame_output, m_status);
    
    #ifndef NDEBUG
        for (int n = 0; n < static_cast<int>(m_frame_output.size()); ++n) {
            assert(!std::isnan(m_frame_output[n]));
            assert(!std::isinf(m_frame_output[n]));
            assert(std::abs(m_frame_output[n]) < 1000.0f && "The output signal is suspiciously large.");
        }
    #endif

    if (m_frame_output.size() == 0) {
        return 0;
    }

    // Add the content of the window and its shape (OLA accumulation)
    m_out_sum += m_frame_output;
    m_out_sum_win += m_win;

    // There are timestep samples that we can flush
    int nb_samples_to_output_remains = nb_samples_to_output;
    if (m_first_frame_at_t0_samples_to_skip > 0) {
        int nb_topop = std::min<int>(m_first_frame_at_t0_samples_to_skip, nb_samples_to_output);
        m_out_sum.pop_front(nb_topop);
        m_out_sum_win.pop_front(nb_topop);
        nb_samples_to_output_remains -= nb_topop;
        m_first_frame_at_t0_samples_to_skip -= nb_topop;
    } else {
        m_status.padding_start = false;
    }

    // Normalize and flush the samples
    for (int n = 0; n < nb_samples_to_output_remains; ++n) {
        if (m_out_sum_win[n] < 2*phaseshift::float32::eps()) {
            m_out_sum_win[n] = 1.0f;
            m_failure_status.nb_imperfect_reconstruction++;
        }
    }
    m_out_sum.divide_equal_range(m_out_sum_win, nb_samples_to_output_remains);

    #ifndef NDEBUG
        for (int n = 0; n < nb_samples_to_output_remains; ++n) {
            assert(!std::isnan(m_out_sum[n]));
            assert(!std::isinf(m_out_sum[n]));
            assert(std::abs(m_out_sum[n]) < 1000.0f && "The output signal is suspiciously large. Did you forget to apply a window?");
        }
    #endif

    // Push to output buffer
    if (pout->size() + nb_samples_to_output_remains > pout->size_max()) {
        m_failure_status.nb_output_buffer_overflows++;
        assert(pout->size() + nb_samples_to_output_remains <= pout->size_max() && 
               "phaseshift::ola::output_one_frame: There is not enough space in the output buffer");
    } else {
        pout->push_back(m_out_sum, 0, nb_samples_to_output_remains);
    }

    m_output_length += nb_samples_to_output_remains;
    m_out_sum.pop_front(nb_samples_to_output_remains);
    m_out_sum_win.pop_front(nb_samples_to_output_remains);

    // Prepare for next one
    m_out_sum.push_back(0.0f, nb_samples_to_output);
    m_out_sum_win.push_back(0.0f, nb_samples_to_output);

    return nb_samples_to_output;
}

void phaseshift::ola::advance_input_cursor() {
    m_frame_rolling.pop_front(m_timestep);
    m_input_win_center_idx_next += m_timestep;
    m_status.first_input_frame = false;
}

int phaseshift::ola::process_input_available() {
    // That's the expression just for a standard OLA processing.
    // TODO: For time stretching, this should account for the output/input ratio.
    int available_out_space = m_out.size_max() - m_out.size();
    int nb_frames_possible = std::floor(available_out_space / m_timestep);
    return nb_frames_possible * m_timestep;
}

int phaseshift::ola::process(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    proc_time_start();

    if (m_status.finished) {
        return 0;
    }

    if (pout == nullptr) {
        pout = &m_out;
    }

    m_input_length += in.size();

    int nb_output = 0;
    int in_n = 0;
    
    while (in_n < in.size()) {

        // Fill rolling buffer up to winlen, without over-reading `in`
        int nb_samples_for_winlen = std::min<int>(winlen() - m_frame_rolling.size(), in.size() - in_n);
        m_frame_rolling.push_back(in, in_n, nb_samples_for_winlen);
        in_n += nb_samples_for_winlen;

        // When buffer is full, enter the DECOUPLED CONTROL LOOP
        while (m_frame_rolling.size() == winlen()) {

            // Copy current frame for processing
            m_frame_input = m_frame_rolling;
            assert(m_frame_input.size() > 0 && "phaseshift::ola::process: The input frame is empty.");

            // Update input cursor for status
            m_input_win_center_idx = m_input_win_center_idx_next;
            assert(m_input_win_center_idx >= 0 && "phaseshift::ola::process: The input window center index is negative.");

            // Update status BEFORE calling decoupled control methods
            // They need to know the current input/output positions
            m_status.input_win_center_idx = m_input_win_center_idx;
            m_output_win_center_idx = -m_first_frame_at_t0_samples_to_skip + m_output_length + (winlen()-1)/2;
            m_status.output_win_center_idx = m_output_win_center_idx;

            // Check if output buffer has enough space BEFORE calling should_output()
            // (important for slow-down which produces multiple outputs per input)
            // should_output() modifies accumulator state, so we must check first
            if (pout->size() + m_timestep > pout->size_max()) {
                // Not enough space - return what we have, caller should drain and retry
                // Note: we don't consume input, so next call will continue from here
                proc_time_end(in.size() / fs());
                return nb_output;
            }

            // DECOUPLED DECISION 1: Should we produce output?
            if (should_output(m_status)) {
                // Determine output size for this frame
                int nb_samples_to_output = m_timestep;
                if (m_target_output_length > 0) {
                    // With time scaling target: limit to remaining target
                    phaseshift::globalcursor_t remaining = m_target_output_length - m_output_length;
                    if (remaining < nb_samples_to_output) {
                        nb_samples_to_output = static_cast<int>(remaining);
                    }
                }
                int nb_output_this_step = output_one_frame(pout, nb_samples_to_output);
                nb_output += nb_output_this_step;
            }

            // DECOUPLED DECISION 2: Should we consume input?
            if (should_consume_input(m_status)) {
                advance_input_cursor();
                break;  // Exit inner loop, need more input from outer loop
            }
            
            // If not consuming input, loop again to potentially produce more outputs
            // from the same input frame (for time stretching slow-down)
        }
    }

    proc_time_end(in.size() / fs());

    return nb_output;
}

int phaseshift::ola::flush(int chunk_size_max, phaseshift::ringbuffer<float>* pout) {
    proc_time_start();

    if (m_status.finished) {
        return 0;
    }

    if (pout == nullptr) {
        pout = &m_out;
    }

    // First-time flush initialization
    if (!m_status.flushing) {
        m_flush_nb_samples_total = m_frame_rolling.size() + m_extra_samples_to_flush;
        m_status.flushing = true;
    }

    int nb_output = 0;
    const bool has_target = (m_target_output_length > 0);

    while (true) {
        // === TERMINATION CONDITIONS ===
        const bool input_exhausted = (m_flush_nb_samples_total <= 0);
        const bool target_reached = has_target && (m_output_length >= m_target_output_length);

        // Stop if: target reached, OR input exhausted without a target to reach
        if (target_reached || (input_exhausted && !has_target)) {
            m_status.finished = true;
            m_frame_rolling.clear();
            break;
        }

        // Stop if chunk limit reached (streaming mode)
        if (chunk_size_max > 0 && nb_output >= chunk_size_max) {
            break;
        }

        // === PREPARE FRAME ===
        // Zero-pad rolling buffer to winlen if needed
        if (m_frame_rolling.size() < winlen()) {
            m_status.padding_end = true;
            m_frame_rolling.push_back(0.0f, winlen() - m_frame_rolling.size());
        }

        m_frame_input = m_frame_rolling;
        m_input_win_center_idx = m_input_win_center_idx_next;
        m_status.input_win_center_idx = m_input_win_center_idx;
        m_output_win_center_idx = -m_first_frame_at_t0_samples_to_skip + m_output_length + (winlen()-1)/2;
        m_status.output_win_center_idx = m_output_win_center_idx;

        // === COMPUTE OUTPUT SIZE ===
        int nb_samples_to_output = m_timestep;
        if (has_target) {
            // Limit to remaining target
            phaseshift::globalcursor_t remaining = m_target_output_length - m_output_length;
            if (remaining < nb_samples_to_output) {
                nb_samples_to_output = static_cast<int>(remaining);
                m_status.last_frame = true;
            }
        } else if (m_flush_nb_samples_total > 0 && m_flush_nb_samples_total < nb_samples_to_output) {
            // No target: limit to remaining input (1:1 ratio)
            nb_samples_to_output = m_flush_nb_samples_total;
            m_status.last_frame = true;
        }

        // === DECOUPLED CONTROL ===
        if (should_output(m_status)) {
            nb_output += output_one_frame(pout, nb_samples_to_output);
        }

        if (should_consume_input(m_status)) {
            // Consume input (normal or speed-up mode)
            if (input_exhausted) {
                // No more input to consume - we're done
                m_status.finished = true;
                m_frame_rolling.clear();
                break;
            }
            advance_input_cursor();
            m_flush_nb_samples_total -= std::min(m_timestep, m_flush_nb_samples_total);
        } else {
            // Repeat frame (slow-down mode) - only valid if we have a target
            if (input_exhausted && !has_target) {
                // Safety: avoid infinite loop if slow-down without target
                m_status.finished = true;
                m_frame_rolling.clear();
                break;
            }
        }
    }

    proc_time_end(nb_output / fs());
    return nb_output;
}

int phaseshift::ola::fetch(phaseshift::ringbuffer<float>* pout, int chunk_size_max) {

    if (m_out.size() == 0) {
        return 0;
    }

    int chunk_size = m_out.size();
    if (chunk_size_max > 0) {
        chunk_size = std::min<int>(chunk_size, chunk_size_max);
    }

    assert(pout->size() + chunk_size <= pout->size_max() && "phaseshift::ola::fetch: There is not enough space in the output buffer");

    pout->push_back(m_out, 0, chunk_size);
    m_out.pop_front(chunk_size);

    return chunk_size;
}

void phaseshift::ola::process_offline(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    process(in, pout);
    flush(-1, pout);
}

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

        while (fetch(pout) > 0) {}
    }

    // Flush remaining data in chunks
    int fetched = 1;
    while (fetched > 0) {
        flush(chunk_size);
        fetched = fetch(pout, chunk_size);
    }
}

void phaseshift::ola::process_realtime(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {

    int chunk_size_req = in.size();

    process(in);

    int available = fetch_available();

    assert(m_realttime_prepad_latency_remaining >= 0);
    if (m_realttime_prepad_latency_remaining > 0) {
        // Pre-pad: pad with zeros up to latency total
        int zeros_to_add = std::min(m_realttime_prepad_latency_remaining, chunk_size_req);
        int to_fetch = chunk_size_req - zeros_to_add;

        pout->push_back(0.0f, zeros_to_add);
        m_realttime_prepad_latency_remaining -= zeros_to_add;

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

    m_stat_realtime_out_size_min = std::min(m_stat_realtime_out_size_min, m_out.size());
}

void phaseshift::ola::reset() {
    phaseshift::audio_block::reset();

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

    m_status.reset();
    phaseshift::ola::m_failure_status.reset();

    m_first_frame_at_t0_samples_to_skip = (winlen()-1)/2;
    m_frame_rolling.push_back(0.0f, m_first_frame_at_t0_samples_to_skip);
    m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;

    m_status.padding_start = true;
    m_out_sum.push_back(0.0f, winlen());
    m_out_sum_win.push_back(0.0f, winlen());
    m_flush_nb_samples_total = 0;

    m_input_length = 0;
    m_input_win_center_idx = 0;
    m_input_win_center_idx_next = 0;
    m_output_win_center_idx = 0;
    m_output_length = 0;
    m_target_output_length = -1;

    m_realttime_prepad_latency_remaining = latency();

    m_stat_realtime_out_size_min = std::numeric_limits<int>::max();
}


// Builder --------------------------------------------------------------------

phaseshift::ola* phaseshift::ola_builder::build(phaseshift::ola* pab) {
    build_time_start();
    phaseshift::audio_block_builder::build(pab);

    pab->m_fs = fs();

    if (m_timestep < 0)
        m_timestep = static_cast<int>(fs() * 0.005);
    assert((timestep() > 0) && "time step has to be >0");
    pab->m_timestep = timestep();

    if (m_winlen < 0)
        m_winlen = static_cast<int>(fs() * 0.010);
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

    int output_buffer_size = m_winlen + m_timestep;  // minimum reasonable size
    if (m_max_input_chunk_size > 0) {
        output_buffer_size = std::max(output_buffer_size, pab->max_output_chunk_size(m_max_input_chunk_size));
    }
    pab->m_out.resize_allocation(2 * output_buffer_size);
    pab->m_out.clear();

    pab->m_win.resize_allocation(m_winlen);
    // Default to Hamming window
    phaseshift::win_hamming(&(pab->m_win), m_winlen);

    pab->m_extra_samples_to_skip = m_extra_samples_to_skip;
    pab->m_extra_samples_to_flush = m_extra_samples_to_flush;

    pab->m_out_sum.push_back(0.0f, m_winlen);
    pab->m_out_sum_win.push_back(0.0f, m_winlen);

    pab->m_status.first_input_frame = true;
    pab->m_status.last_frame = false;
    pab->m_status.flushing = false;
    pab->m_input_length = 0;
    pab->m_input_win_center_idx = 0;
    pab->m_input_win_center_idx_next = 0;
    pab->m_output_length = 0;
    pab->m_output_win_center_idx = 0;
    pab->m_flush_nb_samples_total = 0;

    pab->phaseshift::ola::reset();

    build_time_end();

    return pab;
}


// Tests ----------------------------------------------------------------------

void phaseshift::dev::audio_block_ola_test(phaseshift::ola* pab, int chunk_size, float resynthesis_threshold, int options) {

    float duration_s = 3.0f;

    // Static tests
    phaseshift::dev::test_require(pab->fs() > 0.0f, "audio_block_ola_test: fs() <= 0.0f");
    phaseshift::dev::test_require(pab->latency() >= 0, "audio_block_ola_test: latency() < 0");

    std::mt19937 gen(0);

    float fs = pab->fs();

    enum {mode_offline=0, mode_streaming=1, mode_realtime=2};
    enum {synth_noise=0, synth_silence=1, synth_click=2, synth_saturated=3, synth_sin=4, synth_harmonics=5};

    for (int mode = mode_offline; mode <= mode_realtime; ++mode) {

        for (int synth = synth_noise; synth <= synth_harmonics; ++synth) {

            for (int iter=1; iter <= 3; ++iter) {

                // Generate input signal
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

                // Process
                if (mode == int(mode_offline)) {
                    pab->process_offline(signal_in, &signal_out);

                } else if (mode == int(mode_streaming)) {
                    phaseshift::ringbuffer<float> chunk_in;
                    chunk_in.resize_allocation(chunk_size);

                    while (!pab->finished()) {
                        if (pab->input_length() < signal_in.size()) {
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

                    while (signal_out.size() < signal_in.size()) {
                        int chunk_size_req = std::min<int>(chunk_size, signal_in.size() - pab->input_length());
                        chunk_in.clear();
                        chunk_in.push_back(signal_in, pab->input_length(), chunk_size_req);
                        int signal_out_size_before = signal_out.size();

                        pab->process_realtime(chunk_in, &signal_out);

                        int signal_out_size_after = signal_out.size();
                        phaseshift::dev::test_require(chunk_in.size() == signal_out_size_after-signal_out_size_before, 
                            "audio_block_ola_test: chunk_in.size() != signal_out_size_after-signal_out_size_before");
                    }
                }

                // Verify
                phaseshift::dev::test_require(signal_out.size() > 0, "audio_block_ola_test: signal_out.size() == 0");
                phaseshift::dev::test_require(signal_out.size() == signal_in.size(), "audio_block_ola_test: signal_out.size() != signal_in.size()");

                phaseshift::dev::signals_check_nan_inf(signal_out);

                if ((mode == int(mode_offline)) || (mode == int(mode_streaming))) {
                    phaseshift::dev::test_require(phaseshift::dev::signals_equal_strictly(signal_in, signal_out, resynthesis_threshold), 
                        "audio_block_ola_test: signals_equal_strictly() failed");

                } else if (mode == int(mode_realtime)) {
                    phaseshift::dev::test_require(pab->stat_realtime_out_size_min() < chunk_size, 
                        "audio_block_ola_test: stat_realtime_out_size_min() >= chunk_size");

                    if (synth == synth_click) {
                        if (options & option_test_latency_decoupled) {
                            int measured_latency = 0;
                            for (; measured_latency < signal_out.size();) {
                                if (signal_out[measured_latency] > 0.33f) {
                                    break;
                                }
                                measured_latency++;
                            }
                            phaseshift::dev::test_require(measured_latency == pab->latency(), 
                                "audio_block_ola_test: measured_latency != latency()");
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
        pbuilder->set_max_input_chunk_size(chunk_size);

        auto pab = pbuilder->build();

        phaseshift::dev::audio_block_ola_test(pab, chunk_size);

        delete pab;
    }

    delete pbuilder;
}

void phaseshift::dev::audio_block_ola_builder_test(int nb_threads) {
    phaseshift::dev::audio_block_builder_test(phaseshift::dev::audio_block_ola_builder_test_singlethread, 1);
}
