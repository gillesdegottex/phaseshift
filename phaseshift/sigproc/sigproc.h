// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_SIGPROC_SIGPROC_H_
#define PHASESHIFT_SIGPROC_SIGPROC_H_

#include <phaseshift/utils.h>
#include <phaseshift/lookup_table.h>

#include <algorithm>
#include <complex>

namespace phaseshift {

    template<typename T, typename array_type>
    inline int argmin(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return 0;

        T val = array[0];
        assert(!std::isnan(val));
        assert(!std::isinf(val));
        int idx = 0;
        for (int n=1; n < static_cast<int>(array.size()); ++n){
            if (array[n] < val) {
                val = array[n];
                assert(!std::isnan(val));
                assert(!std::isinf(val));
                idx = n;
            }
        }

        return idx;
    }

    template<typename T, typename array_type>
    inline T min(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return array[0];

        T val = array[0];
        assert(!std::isnan(val));
        assert(!std::isinf(val));
        for (int n=1; n < static_cast<int>(array.size()); ++n) {
            val = std::min<T>(val, array[n]);
            assert(!std::isnan(val));
            assert(!std::isinf(val));
        }

        return val;
    }

    template<typename T, typename array_type>
    inline int argmax(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return 0;

        T val = array[0];
        assert(!std::isnan(val));
        assert(!std::isinf(val));
        int idx = 0;
        for (int n=1; n < static_cast<int>(array.size()); ++n){
            if (array[n] > val) {
                val = array[n];
                assert(!std::isnan(val));
                assert(!std::isinf(val));
                idx = n;
            }
        }

        return idx;
    }

    template<typename T, typename array_type>
    inline T max(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return array[0];

        T val = array[0];
        assert(!std::isnan(val));
        assert(!std::isinf(val));
        for (int n=1; n < static_cast<int>(array.size()); ++n) {
            val = std::max<T>(val, array[n]);
            assert(!std::isnan(val));
            assert(!std::isinf(val));
        }

        return val;
    }

    template<typename T, typename array_type>
    inline T sum(const array_type& array) {
        if (array.size() == 0)  return static_cast<T>(0);
        if (array.size() == 1)  return array[0];

        T val = static_cast<T>(0);
        for (int n=1; n < static_cast<int>(array.size()); ++n)
            val += array[n];

        return val;
    }

    template<typename T, typename array_type>
    inline T prod(const array_type& array) {
        if (array.size() == 0)  return static_cast<T>(1);
        if (array.size() == 1)  return array[0];

        T val = static_cast<T>(1);
        for (int n=1; n < static_cast<int>(array.size()); ++n)
            val *= array[n];

        return val;
    }

    template<typename T>
    inline T mean(const T* parray, int size) {
        assert(size > 0);
        if (size == 1)  return parray[0];

        T mean_sum = parray[0];
        for (int n=1; n < size; ++n) {
            mean_sum += parray[n];
        }

        return mean_sum/size;
    }

    template<typename T>
    inline T mean(const std::deque<T>& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return *(array.begin());

        T mean_sum = static_cast<T>(0);
        for (auto it=array.begin(); it!=array.end(); ++it) {
            mean_sum += *it;
        }

        return mean_sum/array.size();
    }

    template<typename T, typename array_type>
    inline T mean(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return array[0];

        T mean_sum = array[0];
        for (int n=1; n < static_cast<int>(array.size()); ++n) {
            mean_sum += array[n];
        }

        return mean_sum/array.size();
    }

    template<typename T, typename array_type>
    inline T std(const array_type& array, T meanv) {
        assert(array.size() > 0);
        if (array.size() == 1)  return static_cast<T>(0.0);

        T diff = array[0] - meanv;
        T var_sum = diff * diff;
        for (int n=0; n < static_cast<int>(array.size()); ++n) {
            diff = array[n] - meanv;
            var_sum += diff * diff;
        }

        return std::sqrt(var_sum / (array.size()-1));
    }
    template<typename T, typename array_type>
    inline T std(const array_type& array) {
        assert(array.size() > 0);
        if (array.size() == 1)  return static_cast<T>(0.0);

        return std(array, phaseshift::mean<T>(array));
    }

    template<typename T, typename array_type>
    inline T norm(const array_type& array) {
        assert(array.size() > 0);

        T* pvalue = array.data();

        T sum = static_cast<T>(0.0);
        for (int n=0; n < static_cast<int>(array.size()); ++n) {
            sum += *pvalue * *pvalue;
            ++pvalue;
        }

        return sum;
    }

    //! Sigmoid transition
    template<typename array_t>
    inline void sigmoid(array_t* parray, float cf, float vc) {
        for (int k=0; k < parray->size(); ++k) {
            (*parray)[k] = 1.0f/(1.0f+std::exp(-(k-cf)/vc));
        }
    }

    template<class array_type>
    inline void lowpass_hspec(array_type* parray, float fs, int kcut, float slope_dbhz) {
        int dftlen = (parray->size()-1)*2;

        // Move the cutting bin to the next valley
        // TODO(GD) add threshold maybe to prevent kcut to drift away for too much if kcut ison a very slow slope.
        while ((kcut+1 < dftlen/2+1) && (std::norm((*parray)[kcut+1]) < std::norm((*parray)[kcut])))
            ++kcut;

        float slope = slope_dbhz*fs/dftlen;
        for (int k=kcut; k < dftlen/2+1; ++k) {
            (*parray)[k] *= phaseshift::db2lin01_ltf(slope*(k-kcut));
        }
    }

    // Rotate/Shift bins for delay equal to the half of the array's size
    // (similar to numpy.fft.fftshift)
    template<class array_type>
    inline void shift_half_size(array_type* parray) {
        array_type& array = *parray;
        for (int k=0; k < static_cast<int>(array.size())/2; ++k) {
            std::swap(array[k], array[(k+array.size()/2)%array.size()]);
        }
    }

    //! Shift the signal by nbsample samples.
    template<class array_type>
    inline void timeshift_sig(array_type* parray, int delay) {
        if (delay == 0)
            return;

        if ((-delay) < 0) {
            for (int n=parray->size()-1; n >= delay; --n) {
                (*parray)[n] = (*parray)[n-delay];
            }
            std::memset(parray->data(), 0, sizeof(float)*delay);
        } else {
            for (int n=0; n < parray->size() - (-delay); ++n) {
                (*parray)[n] = (*parray)[n+(-delay)];
            }
            std::memset(parray->data()+(parray->size() - (-delay)), 0, sizeof(float)*(-delay));
        }
    }

    //! nbsamplef : Number of sample to shift the signal. Can be non-integer.
    template<class array_type>
    inline void timeshift_hspec(array_type* parray, float nbsamplef) {
        int dftlen = (parray->size()-1)*2;
        const float two_pi = static_cast<float>(2.0 * M_PI);
        float phase_shift_coef = nbsamplef * two_pi / static_cast<float>(dftlen);

        #if 1
            float phase_shift_coef_idxf = phase_shift_coef*g_lt_cos_x2i;
            float twopi_idxf = phaseshift::twopi*g_lt_cos_x2i;
            float* pdst = reinterpret_cast<float*>(parray->data());
            int size = parray->size();
            float a, b, c, d, phiidxf, x, y, z;
            int cossinidx;
            phiidxf = 0.5f;  // +0.5 for rounding to nearest neighbor
            if (phase_shift_coef > 0) {
                for (int k = 0; k < size; ++k) {

                    while (phiidxf > twopi_idxf)
                        phiidxf -= twopi_idxf;
                    cossinidx = static_cast<int>(phiidxf);
                    c = g_lt_cos_values[cossinidx];
                    d = g_lt_sin_values[cossinidx];

                    a = *pdst;
                    b = *(pdst+1);
                    x = a * (c - d);
                    y = a + b;
                    z = a - b;
                    *pdst++ = z * d + x;
                    *pdst++ = y * c - x;

                    phiidxf += phase_shift_coef_idxf;
                }
            } else {
                phase_shift_coef_idxf = -phase_shift_coef_idxf;
                for (int k = 0; k < size; ++k) {

                    while (phiidxf > twopi_idxf)
                        phiidxf -= twopi_idxf;
                    cossinidx = static_cast<int>(phiidxf);
                    c = g_lt_cos_values[cossinidx];
                    d = g_lt_sin_values[cossinidx];
                    // d = -d;  // The only diff from the positive version above (merged in eqs. below)

                    a = *pdst;
                    b = *(pdst+1);
                    x = a * (c + d);
                    y = a + b;
                    z = a - b;
                    *pdst++ = x - z * d;
                    *pdst++ = y * c - x;

                    phiidxf += phase_shift_coef_idxf;
                }
            }
        #elif 0
            float phi = 0.0f;
            float* pdst = reinterpret_cast<float*>(parray->data());
            int size = parray->size();
            for (int k = 0; k < size; ++k) {
                float a = *pdst;
                float b = *(pdst+1);
                float c = phaseshift::cos_ltf(phi);
                float d = phaseshift::sin_ltf(phi);
                // float c = std::cos(phi);
                // float d = std::sin(phi);
                float x = a * (c-d);
                float y = a + b;
                float z = a - b;
                *pdst++ = z*d + x;
                *pdst++ = y*c - x;

                phi += phase_shift_coef;
            }
        #else
            for (int k = 0; k < parray->size(); ++k) {
                float phi = k*phase_shift_coef;
                (*parray)[k] *= std::complex<float>(std::cos(phi), std::sin(phi));
            }
        #endif
    }

    //! Fill ringbuffer with Gaussian noise
    template<typename ringbuffer_t>
    inline void push_back_noise_normal(ringbuffer_t& rb, int n, std::mt19937& gen, typename ringbuffer_t::value_type mean = 0.0, typename ringbuffer_t::value_type stddev = 1.0, typename ringbuffer_t::value_type limit = 0.99) {

        std::normal_distribution<typename ringbuffer_t::value_type> dist;

        for (int i=0; i < n; ++i) {
            auto value = dist(gen) * stddev + mean;
            if (value > limit)
                value = limit;
            if (value < -limit)
                value = -limit;
            rb.push_back(value);
        }
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_SIGPROC_SIGPROC_H_
