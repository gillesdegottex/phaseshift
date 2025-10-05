// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_UTILS_H_
#define PHASESHIFT_UTILS_H_

#include <random>
#include <limits>
#include <complex>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#define _USE_MATH_DEFINES  // For win32
#include <cmath>
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>

#include "./filesystem.h"

#define DFILE__ std::filesystem::path(__FILE__).filename().string()
// #define DFILE__ __FILE__

// #define DOUT std::cerr << DFILE__ << ":" << __LINE__ << ": "
// #define DLINE std::cerr << DFILE__ << ":" << __LINE__ << std::endl;
#define DOUT std::cerr << DFILE__ << ":" << std::to_string(__LINE__) << ": "
#define DLINE std::cerr << DFILE__ << ":" << std::to_string(__LINE__) << std::endl;

// A macro to disable lines related to profiling only.
#ifdef PHASESHIFT_DEV_PROFILING
    #define PHASESHIFT_PROF(X)    X
#else
    #define PHASESHIFT_PROF(X)
#endif

#define assert_nan_inf(value) {assert(!std::isnan(value) && "value is nan"); assert(!std::isinf(value) && "value is inf");}

namespace phaseshift {

    inline float lin2db(float value) {
        return 20.0f*log10f(fabsf(value));
    }
    inline float db2lin(float value) {
        return powf(10.0f, value*0.05f);  // 0.05=1.0/20.0
    }
    inline float lin2db(std::complex<float> value) {
        return 10.0f*log10f(std::norm(value));
    }

    template<typename T>
    T lin2db(T value) {
        return static_cast<T>(20.0)*std::log10(std::abs(value));
    }
    template<typename T>
    T db2lin(T value) {
        return std::pow(static_cast<T>(10.0), value*static_cast<T>(0.05));  // 0.05=1.0/20.0
    }

    inline float hz2st(float hz) {
        return 12.0f * std::log2(hz/440.0f);
    }
    inline float st2hz(float st) {
        return 440.0f * std::pow(2.0f, st/12.0f);
    }

    inline int nextpow2(int winlen) {  // TODO(GD) Move to fftscarf
        assert(winlen > 0);
        int dftlen = std::pow<int>(2, static_cast<int>(std::ceil(std::log2f(winlen))));
        assert(dftlen >= winlen);
        assert(dftlen < 2*winlen);
        return dftlen;
    }

    // Shortcuts
    namespace int32 {
        inline constexpr int size()  {return sizeof(int32_t);}
        inline constexpr double eps()   {return std::numeric_limits<int32_t>::epsilon();}
        inline constexpr double min()   {return (std::numeric_limits<int32_t>::min)();}
        inline constexpr double max()   {return (std::numeric_limits<int32_t>::max)();}
    }
    namespace float32 {
        inline constexpr int size()  {return sizeof(float);}
        inline constexpr double eps()   {return std::numeric_limits<float>::epsilon();}
        inline constexpr double min()   {return (std::numeric_limits<float>::min)();}
        inline constexpr double max()   {return (std::numeric_limits<float>::max)();}
    }
    namespace float64 {
        inline constexpr int size()  {return sizeof(double);}
        inline constexpr double eps()   {return std::numeric_limits<double>::epsilon();}
        inline constexpr double min()   {return (std::numeric_limits<double>::min)();}
        inline constexpr double max()   {return (std::numeric_limits<double>::max)();}
    }

    static const float twopi = 2*M_PI;
    static const float oneover_twopi = 1.0f/(2*M_PI);
    static const float piovertwo = M_PI/2;

    //! Preferes signed in order to be able to check for overflow or inconsistency
    typedef int64_t globalcursor_t;

    namespace dev {
        int check_compilation_options();

        template<class datastruct_ref, class datastruct_test>
        bool signals_equal_strictly(const datastruct_ref& ref, const datastruct_test& test, double threshold = phaseshift::float32::eps()) {
            if (int(ref.size()) != int(test.size())) {
                std::cerr << "ERROR: signals_equal_strictly: Signals have different length: " << ref.size() << " vs. " << test.size() << std::endl;
                return false;
            }

            for(int n=0; n<int(ref.size()); ++n) {
                // DOUT << phaseshift::lin2db((ref[n] - test[n])) << "<" << phaseshift::lin2db(threshold) << std::endl;
                if ((ref[n] - test[n]) > threshold) {
                    std::cerr << "ref[" << n << "]=" << ref[n] << " test[" << n << "]=" << test[n] << " err=" << (ref[n]-test[n]) << "(" << phaseshift::lin2db(ref[n]-test[n]) << "dB) > " << threshold << "(" << phaseshift::lin2db(threshold) << "dB)" << std::endl;
                    return false;
                }
            }

            return true;
        }

        template<class datastruct_ref, class datastruct_test>
        double signals_diff_ener(const datastruct_ref& ref, const datastruct_test& test) {
            if (ref.size() != test.size()) {
                std::cerr << "ERROR: signals_diff_ener: Signals have different length: " << ref.size() << " vs. " << test.size() << std::endl;
                return -1.0;
            }

            if (ref.size() == 0) {
                std::cerr << "ERROR: signals_diff_ener: Signals have zero length" << std::endl;
                return -1.0;
            }

            double ener = 0.0;
            for (int n = 0; n < ref.size(); ++n) {
                ener += (ref[n] - test[n]) * (ref[n] - test[n]);
            }
            ener /= ref.size();
            ener = std::sqrt(ener);

            return ener;
        }
        template<class datastruct_ref, class datastruct_test>
        double signals_diff_max(const datastruct_ref& ref, const datastruct_test& test) {
            if (ref.size() != test.size()) {
                std::cerr << "ERROR: signals_diff_max: Signals have different length: " << ref.size() << " vs. " << test.size() << std::endl;
                return -1.0;
            }

            float maxv = 0.0f;
            for (int n = 0; n < ref.size(); ++n) {
                maxv = std::max(maxv, std::abs(ref[n] - test[n]));
            }

            return maxv;
        }
    } // namespace dev

}  // namespace phaseshift

#endif  // PHASESHIFT_UTILS_H_
