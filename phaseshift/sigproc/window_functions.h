// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_SIGPROC_WINDOW_FUNCTIONS_H_
#define PHASESHIFT_SIGPROC_WINDOW_FUNCTIONS_H_

#include <phaseshift/utils.h>
#include <phaseshift/containers/vector.h>

#include <cmath>

namespace phaseshift {

    // Hamming window ----------------------------------------------------------
    template<typename value_type>
    value_type win_hamming_function(value_type nf, int N, bool center_at_zero=true) {

        float a0 = 25.0f/46.0f;

        if (center_at_zero)
            nf += static_cast<float>(N-1)/2;

        return a0 - (1.0f-a0)*std::cos(2*M_PI*nf/(N-1));  // TODO(GD) cosf
    }

    template<typename value_type>
    void win_hamming(value_type* win, int N, bool norm_sum=true) {

        float a0 = 25.0f/46.0f;

        float win_sum = 0.0f;
        for (int n = 0; n < N; ++n) {
            win[n] = a0 - (1.0f-a0)*std::cos(2*M_PI*n/(N-1));  // TODO(GD) cosf
            win_sum += win[n];
        }

        if (norm_sum) {
            for (int n = 0; n < N; ++n)
                win[n] /= win_sum;
        }
    }

    template<typename value_type>
    void win_hamming(phaseshift::vector<value_type>* pwin, int N, bool norm_sum=true) {
        phaseshift::vector<value_type>& win = *pwin;
        win.resize(N);
        win_hamming(win.data(), N, norm_sum);
    }


    // Hann window -------------------------------------------------------------
    template<typename value_type>
    value_type win_hann_function(value_type nf, int N, bool center_at_zero=true) {

        float a0 = 0.5f;

        if (center_at_zero)
            nf += static_cast<float>(N-1)/2;

        return a0 - (1.0f-a0)*std::cos(2*M_PI*nf/(N-1));  // TODO(GD) cosf
    }

    template<typename value_type>
    void win_hann(value_type* win, int N, bool norm_sum=true) {

        float a0 = 0.5f;

        float win_sum = 0.0f;
        for (int n = 0; n < N; ++n) {
            win[n] = a0 - (1.0f-a0)*std::cos(2*M_PI*n/(N-1));  // TODO(GD) cosf
            win_sum += win[n];
        }

        if (norm_sum) {
            for (int n = 0; n < N; ++n)
                win[n] /= win_sum;
        }
    }

    template<typename value_type>
    void win_hann(phaseshift::vector<value_type>* pwin, int N, bool norm_sum=true) {
        phaseshift::vector<value_type>& win = *pwin;
        win.resize(N);
        win_hann(win.data(), N, norm_sum);
    }


    // Blackman window ---------------------------------------------------------
    template<typename value_type>
    void win_blackman(value_type* win, int N, bool norm_sum=true) {

        float a = 0.16f;

        float a0 = (1.0f-a)*0.5f;

        float win_sum = 0.0f;
        for (int n = 0; n < N; ++n) {
            win[n] = a0 - 0.5f*std::cos(2*M_PI*n/(N-1)) + 0.5f*a*std::cos(4*M_PI*n/(N-1));  // TODO(GD) cosf
            win_sum += win[n];
        }

        if (norm_sum) {
            for (int n = 0; n < N; ++n)
                win[n] /= win_sum;
        }
    }

    template<typename value_type>
    void win_blackman(phaseshift::vector<value_type>* pwin, int N, bool norm_sum=true) {
        phaseshift::vector<value_type>& win = *pwin;
        win.resize(N);
        win_blackman(win.data(), N, norm_sum);
    }


    // Gaussian window ---------------------------------------------------------
    template<typename value_type>
    void win_gaussian(value_type* win, int N, bool norm_sum=true, float sigma=0.5f) {

        float win_sum = 0.0f;
        for (int n = 0; n < N; ++n) {
            float d = (n-N/2) / (sigma*N/2);
            win[n] = std::exp(-0.5f*d*d);
            win_sum += win[n];
        }

        if (norm_sum) {
            for (int n = 0; n < N; ++n)
                win[n] /= win_sum;
        }
    }

    template<typename value_type>
    void win_gaussian(phaseshift::vector<value_type>* pwin, int N, bool norm_sum=true, float sigma=0.5f) {
        phaseshift::vector<value_type>& win = *pwin;
        win.resize(N);
        win_gaussian(win.data(), N, norm_sum);
    }


    // Kaiser window -----------------------------------------------------------

    //! 0-order modified bessel function [1,2]
    //  The ininite sum will stop as soon as the square of the additive term is smaller than eps.
    //  [1] https://en.wikipedia.org/wiki/Bessel_function#Modified_Bessel_functions:_I%CE%B1,_K%CE%B1
    //  [2] https://mathworld.wolfram.com/ModifiedBesselFunctionoftheFirstKind.html
    template<class value_type>
    value_type modified_bessel_firstkind_zeroorder(value_type x, double eps, int K_max=24){
        if (x==static_cast<value_type>(0))
            return static_cast<value_type>(1);

        value_type sum = static_cast<value_type>(1);
        value_type frac_pow = static_cast<value_type>(1);
        value_type frac_facto = static_cast<value_type>(1);
        value_type frac;
        for (int k=1; k < K_max; ++k) {
            frac_pow *= 0.5*x;
            frac_facto *= k;
            frac = frac_pow/frac_facto;
            sum += frac*frac;
            if (frac*frac<eps)
                break;
        }

        return sum;
    }

    //! [1] https://en.wikipedia.org/wiki/Kaiser_window
    template<typename vector>
    void win_kaiser(vector* pwin, int N, float alpha=2.55, bool norm_sum=true) {
        vector& win = *pwin;
        win.resize(N);

        double pialpha = M_PI * alpha;
        double eps = phaseshift::float32::eps();

        double oneover_mbfk_pialpha = 1.0f / modified_bessel_firstkind_zeroorder<double>(pialpha, eps);
        double win_sum = 0.0f;
        for(int n=0; n < N; ++n) {
            double root = 2 * (n-float(N-1)/2) / float(N-1);  // Definition is over [-N/2,N/2] not [0,N]
            double mbfk_arg = pialpha * std::sqrt(1-root*root);
            win[n] = modified_bessel_firstkind_zeroorder<double>(mbfk_arg, eps) * oneover_mbfk_pialpha;
            win_sum += win[n];
        }

        if (norm_sum) {
            for (int n = 0; n < N; ++n)
                win[n] /= win_sum;
        }
    }

    // Measure bandwidth of a window: The width of the window's main lobe at -6dB
    // [1] https://en.wikipedia.org/wiki/Bandwidth_(signal_processing)
    // x16 Ensures 2 decimals precision after dot for a rectangular window
    template<class array_type>
    inline float window_bandwidth_6db(const array_type& win, float fs, phaseshift::vector<std::complex<float>>* pwin_rfft, int dftlen_factor=16) {
        phaseshift::vector<std::complex<float>>& win_rfft = *pwin_rfft;
        assert(win.size() >= 4);
        assert(fs > 0.0f);

        float thresh = phaseshift::db2lin(-6.0f);  // -6dB threshold

        int dftlen = phaseshift::nextpow2(win.size()) * dftlen_factor;

        win_rfft.resize(dftlen/2+1);
        fftscarf::planmanagerf().rfft(win, win_rfft, dftlen);
        int bwk = 0;
        float amp0 = std::abs(win_rfft[bwk]);
        float amp = std::abs(win_rfft[bwk])/amp0;
        float amp_next = std::abs(win_rfft[bwk+1])/amp0;
        while ((amp_next >= thresh) && (bwk+2 < win_rfft.size())) {
            amp = amp_next;
            bwk++;
            amp_next = std::abs(win_rfft[bwk+1])/amp0;
        }
        // From here: W[bwk] > thresh <= W[bwk+1]
        float g = (thresh - amp_next) / (amp - amp_next);
        float bwkinterp = bwk + (1.0f-g);  // Interpolate the bin index
        float bw_hz = 2* fs * float(bwkinterp) / dftlen;
        return bw_hz;
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_SIGPROC_WINDOW_FUNCTIONS_H_