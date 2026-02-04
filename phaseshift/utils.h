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

#define DFILE__ std::filesystem::path(__FILE__).filename().string()  // Only filename
// #define DFILE__ __FILE__  // For full path

#ifdef __ANDROID__
    #include <android/log.h>
    #define DLINETAG(CHANNEL_NAME) { __android_log_print(ANDROID_LOG_INFO, CHANNEL_NAME, "%s:%d", std::string(__FILE__).substr(std::string(__FILE__).find_last_of("/\\")+1).c_str(), __LINE__); }

    namespace phaseshift {

        class debug_stream_android_t {
            public:
            std::string m_filename;
            std::string m_channel_name;
            int m_line;
            std::stringstream m_stringstream;
            debug_stream_android_t& set_filename(const std::string& filename) { m_filename = filename; return *this; }
            debug_stream_android_t& set_channel_name(const std::string& channel_name) { m_channel_name = channel_name; return *this; }
            debug_stream_android_t& set_line(int line) { m_line = line; return *this; }
        };

        template<typename value_type>
        debug_stream_android_t& operator<<(debug_stream_android_t& debug_stream, const value_type& v) {
            debug_stream.m_stringstream << v;
            return debug_stream;
        }

        inline debug_stream_android_t& operator<<(debug_stream_android_t& debug_stream, std::ostream& (*fn)(std::ostream&)){
            if (
                fn == static_cast<std::ostream& (*)(std::ostream&)>(std::endl) ||
                fn == static_cast<std::ostream& (*)(std::ostream&)>(std::flush)
            ) {
                __android_log_print(ANDROID_LOG_INFO, debug_stream.m_channel_name.c_str(), "%s:%d: %s", debug_stream.m_filename.c_str(), debug_stream.m_line, debug_stream.m_stringstream.str().c_str());
                // std::cerr << debug_stream.m_filename << ":" << debug_stream.m_line << ": " << debug_stream.m_stringstream.str() << std::endl;

                debug_stream.m_stringstream.str(std::string());
            }

            return debug_stream;
        }

        extern debug_stream_android_t g_debug_stream_android;  // here static will create a copy for each translation unit.
    }

    #define DOUTTAG(CHANNEL_NAME) phaseshift::g_debug_stream_android.set_filename(DFILE__).set_line(__LINE__).set_channel_name(CHANNEL_NAME)

#else
    #define DLINETAG(CHANNEL_NAME) std::cerr << CHANNEL_NAME << ": " << DFILE__ << ":" << __LINE__ << std::endl;
    #define DOUTTAG(CHANNEL_NAME) std::cerr << CHANNEL_NAME << ": " << DFILE__ << ":" << __LINE__ << ": "
#endif

#define DLINE DLINETAG("debug")
#define DOUT DOUTTAG("debug")

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

    inline float coef2st(float coef) {
        return 12.0f * std::log2(coef);
    }
    inline float st2coef(float st) {
        return std::pow(2.0f, st / 12.0f);
    }
    inline float hz2st(float hz, float A4 = 440.0f) {
        return coef2st(hz / A4);
    }
    inline float st2hz(float st, float A4 = 440.0f) {
        return A4 * st2coef(st);
    }

    //! Convert a music note to a semitone number relative to 'C'
    //    Recognizes C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb, B
    int music_note_to_semitone(const std::string& note);

    inline int nextpow2(int winlen) {  // TODO(GD) Move to fftscarf
        assert(winlen > 0);
        int dftlen = std::pow<int>(2, static_cast<int>(std::ceil(std::log2f(winlen))));
        assert(dftlen >= winlen);
        assert(dftlen < 2*winlen);
        return dftlen;
    }

    // // Shortcuts TODO rm
    // namespace int32 {
    //     // inline constexpr int size()  {return sizeof(int32_t);}
    //     inline constexpr int32_t min()   {return (std::numeric_limits<int32_t>::min)();}
    //     inline constexpr int32_t max()   {return (std::numeric_limits<int32_t>::max)();}
    // }
    // namespace int64 {
    //     // inline constexpr int size()  {return sizeof(int64_t);}
    //     inline constexpr int64_t min()   {return (std::numeric_limits<int64_t>::min)();}
    //     inline constexpr int64_t max()   {return (std::numeric_limits<int64_t>::max)();}
    // }
    namespace float32 {
        inline constexpr int size()  {return sizeof(float);}
        inline constexpr double eps()   {return std::numeric_limits<float>::epsilon();}
        inline constexpr double min()   {return std::numeric_limits<float>::min();}
        inline constexpr double max()   {return std::numeric_limits<float>::max();}
    }
    namespace float64 {
        inline constexpr int size()  {return sizeof(double);}
        inline constexpr double eps()   {return std::numeric_limits<double>::epsilon();}
        inline constexpr double min()   {return std::numeric_limits<double>::min();}
        inline constexpr double max()   {return std::numeric_limits<double>::max();}
    }

    static const float twopi = 2*M_PI;
    static const float oneover_twopi = 1.0f/(2*M_PI);
    static const float piovertwo = M_PI/2;

    //! Preferes signed in order to be able to check for overflow or inconsistency
    typedef int64_t globalcursor_t;

    namespace dev {
        int check_compilation_options();

        //! This kind of assert is intended to be used in test functions only, so that technical tests can be run in release mode (handy when using cross compiling, or shipping SDKs).
        inline void test_require(bool condition, const char* message) {
            if (!condition) {
                std::cerr << "ERROR: " << message << std::endl;
                exit(1);
            }
        }

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

        template<class datastruct>
        void signals_check_nan_inf(const datastruct& data) {
            #ifndef NDEBUG
            for (int n=0; n<data.size(); ++n) {
                assert(!std::isnan(data[n]));
                assert(!std::isinf(data[n]));
            }
            #endif
        }

    } // namespace dev

}  // namespace phaseshift

#endif  // PHASESHIFT_UTILS_H_
