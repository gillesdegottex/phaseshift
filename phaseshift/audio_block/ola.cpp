// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/ola.h>
#include <phaseshift/sigproc/sigproc.h>

// #include <phaseshift/audio_block/sndfile.h>  // For debug (see #if 0 below)

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

void phaseshift::ola::proc_win(phaseshift::ringbuffer<float>* pout, int nb_samples_to_flush) {

    m_frame_input = m_frame_rolling;
    assert(m_frame_input.size() > 0 && "phaseshift::ola::proc: The input frame is empty.");

    m_input_win_center_idx = m_input_win_center_idx_next;  // TODO Simplify in status_t
    assert(m_input_win_center_idx >= 0 && "phaseshift::ola::proc: The input window center index is negative.");
    m_status.input_win_center_idx = m_input_win_center_idx;
    m_status.output_win_center_idx = -m_first_frame_at_t0_samples_to_skip + m_output_length + (winlen()-1)/2;

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
        // TODO(GD) Need to ensure perfect reconstruction by:
        //          - Cover the full input past beyond one step size after the last sample of the input signal. Not sure that absolutely necessary since the 2nd condition might be enough.
        //          - Forbid timestep/windlen that do not overlap enough.
        for (int n=0; n<nb_samples_to_flush_remains; ++n) {
            if (m_out_sum_win[n] < 2*phaseshift::float32::eps()) {
                m_out_sum_win[n] = 1.0f;
                m_failure_status.nb_imperfect_reconstruction++;
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

        assert(pout->size()+nb_samples_to_flush_remains <= pout->size_max() && "phaseshift::ola::proc_win: There is not enough space in the output buffer");

        pout->push_back(m_out_sum, 0, nb_samples_to_flush_remains);
        m_output_length += nb_samples_to_flush_remains;
        m_out_sum.pop_front(nb_samples_to_flush_remains);
        m_out_sum_win.pop_front(nb_samples_to_flush_remains);

        // Prepare for next one
        m_out_sum.push_back(0.0f, nb_samples_to_flush);
        m_out_sum_win.push_back(0.0f, nb_samples_to_flush);
    }
}

void phaseshift::ola::proc(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {
    proc_time_start();

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

void phaseshift::ola::flush(phaseshift::ringbuffer<float>* pout, int additional_samples_to_flush) {

    if (m_status.flushing)  // Prevent running flush multiple times. Once must be enough.
        return;
    m_status.flushing = true;


    // Number of user input samples that remains to be processed
    int nb_samples_to_flush_total = m_frame_rolling.size() + m_extra_samples_to_flush + additional_samples_to_flush;

    // Avoid blowing up the output buffer in case of flushing
    if (pout->size() + nb_samples_to_flush_total > pout->size_max()) {
        nb_samples_to_flush_total = pout->size_max() - pout->size();
        assert(false && "phaseshift::ola::flush: There is not enough space in the output buffer.");
    }
    
    // Bcs proc(.) will be called before, it will always be smaller than m_winlen
    assert((m_frame_rolling.size() < winlen()) && "phaseshift::ola::flush: There are more samples in the internal buffer than winlen. Have you called proc(.) at least once before calling flush(.)?");

    // We know here that there are not enough samples to fill a full window
    // The chosen strategy in the following is to process extra uncomplete windows, as long as the number of samples to flush is smaller than the timestep.
    // This implies also to flush timestep samples, except for the last iteration, where is less or equal than timestep.
    // TODO(GD) It should go one timestep beyond the last sample of the input signal. That would ensure always good window normalisation.
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
        proc_win(pout, nb_samples_to_flush);

        // Do not go past the end of the input signal
        // TODO(GD) Make it independent of global index
        if (m_input_win_center_idx + m_timestep < m_input_length) {
            m_input_win_center_idx_next += m_timestep;  // ... make it rdy for next window
        }

        nb_samples_to_flush_total -= nb_samples_to_flush;

    } while (nb_samples_to_flush_total > 0);

    m_frame_rolling.clear();  // flush discontinues the audio stream, so clear the internal buffer.
    // m_extra_samples_to_flush = 0;  // Do not clear this, bcs it is used for reset()
}


void phaseshift::ola::proc_same_size(const phaseshift::ringbuffer<float>& in, phaseshift::ringbuffer<float>* pout) {

    int out_size_requested = in.size();

    proc(in, &m_rt_out);

    if (m_rt_out.size() < out_size_requested) {

        if (!m_rt_received_samples) {
            // Beginning of the stream, there is no yet enough data processed to get enough input.
            // So pre-fill all of the requested output with zeros.
            
            // We don't know how many zeros are necessary to avoid ever coming back here.
            // TODO It should be possible to calculate it based on the winlen and the timestep.

            pout->push_back(0.0f, out_size_requested);

        } else {
            // We are at the end of the stream.

            // So first flush any internal buffers
            flush(&m_rt_out);
            // At that point m_rt_out.size() could be larger than out_size_requested.
            int to_push = std::min<int>(out_size_requested, m_rt_out.size());
            pout->push_back(m_rt_out, 0, to_push);  // So push only what is requested.
            m_rt_out.pop_front(to_push);            // and pop the same amount.

            // And post-fill with as many zeros as necessary.
            int nbzeros = out_size_requested - to_push;
            if (nbzeros > 0) {
                pout->push_back(0.0f, nbzeros);
                // A real post-underrun only happens when we need to add zeros.
                // I.e. if the flush(.) allowed to recover from this underruns, it is not counted as one.
                m_stat_rt_nb_post_underruns++;
            }
        }

    } else {
        // Normal case
        pout->push_back(m_rt_out, 0, out_size_requested);
        m_rt_out.pop_front(out_size_requested);
        m_rt_received_samples = true;

        if (m_stat_rt_nb_post_underruns > 0) {
            m_stat_rt_nb_failed++;  // This should never happen and is tested for.
        }
    }

    m_stat_rt_out_size_min = std::min(m_stat_rt_out_size_min, m_rt_out.size());
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

    assert(m_win.size_max() == winlen());
    // phaseshift::win_hamming(&(m_win), winlen);  // Should not be changed, no need to re-build it.

    m_first_frame_at_t0_samples_to_skip = (winlen()-1)/2;
    m_frame_rolling.push_back(0.0f, m_first_frame_at_t0_samples_to_skip);
    m_first_frame_at_t0_samples_to_skip += m_extra_samples_to_skip;

    m_out_sum.push_back(0.0f, winlen());
    m_out_sum_win.push_back(0.0f, winlen());

    m_status.first_input_frame = true;
    m_status.last_frame = false;
    m_status.skipping_samples_at_start = m_first_frame_at_t0_samples_to_skip > 0;
    m_status.fully_covered_by_window = m_first_frame_at_t0_samples_to_skip == 0;
    m_status.flushing = false;
    m_input_length = 0;
    m_input_win_center_idx = 0;
    m_input_win_center_idx_next = 0;
    m_output_length = 0;

    assert(m_rt_out.size_max() == 2*std::max<int>(winlen()+m_timestep, m_rt_out_size_max));
    m_rt_out.clear();
    m_rt_out.push_back(0.0f, winlen());
    m_rt_received_samples = false;
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

    // Only usefull when using proc_same_size(.)
    pab->m_rt_out_size_max = m_rt_out_size_max;
    pab->m_rt_out.resize_allocation(2*std::max<int>(m_winlen+m_timestep, m_rt_out_size_max));
    pab->m_rt_out.clear();
    // This should NOT be dependent on m_rt_out_size_max.
    // Otherwise the latency will be dependent on it.
    // TODO Remaining optimisation: How to minimize test_m_rt_out_size_min using m_winlen and/or m_timestep, but without knowing m_rt_out_size_max? ... is it actually possible?
    pab->m_rt_out.push_back(0.0f, m_winlen);
    pab->m_rt_received_samples = false;
    pab->m_stat_rt_nb_failed = 0;
    pab->m_stat_rt_nb_post_underruns = 0;
    pab->m_stat_rt_out_size_min = phaseshift::int32::max();

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
                    float phase = 2.0f * M_PI * phase_dist(gen);
                    for (int n = 0; n < signal_in.size(); ++n) {
                        signal_in[n] = 0.9f * std::sin(2.0f * M_PI * 440.0f * n / fs + phase);
                    }
                } else if (synth == synth_harmonics) {
                    signal_in.push_back(0.0f, signal_in.capacity());
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
                signal_out.resize_allocation(signal_in.capacity());

                // Initialize -----------------------------------------

                phaseshift::globalcursor_t nb_samples_total = 0;

                if (mode == int(mode_offline)) {

                    signal_out.clear();
                    pab->proc(signal_in, &signal_out);
                    pab->flush(&signal_out);
                    nb_samples_total = signal_out.size();

                } else if (mode == int(mode_streaming)) {

                    phaseshift::ringbuffer<float> chunk_in, chunk_out;
                    chunk_in.resize_allocation(chunk_size);
                    chunk_out.resize_allocation(pab->max_output_size(chunk_size));

                    while (nb_samples_total < signal_in.size()) {

                        chunk_in.clear();
                        int chunk_size_to_push = std::min<int>(chunk_size, signal_in.size() - nb_samples_total);
                        chunk_in.push_back(signal_in, nb_samples_total, chunk_size_to_push);
                        nb_samples_total += chunk_size_to_push;

                        chunk_out.clear();
                        pab->proc(chunk_in, &chunk_out);
            
                        signal_out.push_back(chunk_out);
                    }
                    pab->flush(&signal_out);

                } else if (mode == int(mode_realtime)) {

                    phaseshift::ringbuffer<float> chunk_in, chunk_out;
                    chunk_in.resize_allocation(chunk_size);
                    chunk_out.resize_allocation(chunk_size);

                    while (nb_samples_total < signal_in.size()) {

                        chunk_in.clear();
                        int chunk_size_to_push = std::min<int>(chunk_size, signal_in.size() - nb_samples_total);
                        chunk_in.push_back(signal_in, nb_samples_total, chunk_size_to_push);
                        nb_samples_total += chunk_size_to_push;

                        chunk_out.clear();
                        pab->proc_same_size(chunk_in, &chunk_out);
                        // assert(chunk_out.size() == chunk_size);  // signal is not integer multiple of chunk size, so the last chunk will be smaller than chunk_size
                        phaseshift::dev::test_require(chunk_out.size() == chunk_in.size(), "audio_block_ola_test: chunk_out.size() != chunk_in.size()");
            
                        signal_out.push_back(chunk_out);
                    }
                }


                // Finalize -------------------------------------------

                phaseshift::dev::test_require(pab->stat_rt_nb_failed() == 0, "audio_block_ola_test: stat_rt_nb_failed() != 0");

                phaseshift::dev::test_require(signal_out.size() == signal_in.size(), "audio_block_ola_test: signal_out.size() != signal_in.size()");
                phaseshift::dev::test_require(signal_in.size() == nb_samples_total, "audio_block_ola_test: signal_in.size() != nb_samples_total");

                phaseshift::dev::signals_check_nan_inf(signal_out);

                #if 0
                    phaseshift::sndfile_writer::write("flop.in.wav", fs, signal_in);
                    phaseshift::sndfile_writer::write("flop.out.wav", fs, signal_out);
                    phaseshift::ringbuffer<float> residual;
                    residual.resize_allocation(signal_in.size());
                    residual.clear();
                    residual = signal_in;
                    residual -= signal_out;
                    phaseshift::sndfile_writer::write("flop.res.wav", fs, residual);
                #endif

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
        pbuilder->set_in_out_same_size_max(chunk_size);

        // DOUT << "fs=" << fs << ", winlen=" << winlen << ", timestep=" << timestep << ", chunk_size=" << chunk_size << std::endl;

        auto pab = pbuilder->build();

        phaseshift::dev::audio_block_ola_test(pab, chunk_size);

        delete pab;
    }

    delete pbuilder;
}

void phaseshift::dev::audio_block_ola_builder_test(int nb_threads) {
    phaseshift::dev::audio_block_builder_test(phaseshift::dev::audio_block_ola_builder_test_singlethread, nb_threads);
}
