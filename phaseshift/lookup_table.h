// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_LOOKUP_TABLE_H_
#define PHASESHIFT_LOOKUP_TABLE_H_

#include <cstddef>
#include <cmath>
#include <iomanip>
#include <vector>
#include <utility>
// #include <fstream>

#include <phaseshift/utils.h>
#include <acbench/time_elapsed.h>

namespace phaseshift {

    class lookup_table {
     protected:
        float m_xmin = -1.0;
        float m_xmax = -1.0;
        float m_x2i = -1.0f;
        float m_step = -1.0f;
        int m_size = 0;
        float* m_values = nullptr;

     public:

        //! Generate pairs of ground truth and quantized values to create specific tests for classes inheriting phaseshift::lookup_table
        template<class lookup_table_type>
        static void generate_range(const lookup_table_type& lt, std::vector<std::pair<float,float>>* xys, float stepdiv = 4, float outbound_margin = 0.1f) {
            std::cout << "INFO:     Table params: xmin=" << lt.m_xmin << ", xmax=" << lt.m_xmax << ", step=" << lt.m_step << ", size=" << lt.m_size << std::endl;
            std::cout << "INFO:     Generation params: stepdiv=" << stepdiv << std::endl;

            xys->clear();
            for (float x = lt.m_xmin-std::abs(lt.m_xmin)*outbound_margin; x <= lt.m_xmax+std::abs(lt.m_xmax)*outbound_margin; x+=lt.m_step/stepdiv) {
                assert(!std::isnan(x));
                assert(!std::isinf(x));
                float y = lt.evaluate_lookup_table(x);
                assert(!std::isnan(y));
                assert(!std::isinf(y));
                xys->push_back(std::make_pair(x,y));
            }
        }

        struct validation_stats {
            double abserr_mean = 0.0;
            double abserr_max = 0.0;
            double relerr_mean = 0.0;
            double relerr_max = 0.0;
            double rangerelerr_mean = 0.0;
            double rangerelerr_max = 0.0;
            float ymin = 0.0;
            float ymax = 0.0;
        };

        //! Generic test function for classes inheriting phaseshift::lookup_table
        template<class lookup_table_type>
        static validation_stats test_validation(const lookup_table_type& lt, float stepdiv = 4, float outbound_margin = 0.1f) {

            std::cout << "INFO:     Table params: xmin=" << lt.m_xmin << ", xmax=" << lt.m_xmax << ", step=" << lt.m_step << ", size=" << lt.m_size << std::endl;
            std::cout << "INFO:     Validation params: stepdiv=" << stepdiv << ", outbound_margin=" << outbound_margin << std::endl;

            // std::ofstream validation_file("lt_vf.txt");

            int n = 0;
            validation_stats stats;
            stats.ymin = phaseshift::float32::max();
            stats.ymax = phaseshift::float32::min();
            stats.abserr_max = 0.0;
            stats.abserr_mean = 0.0;
            stats.relerr_max = 0.0;
            stats.relerr_mean = 0.0;
            for (float x = lt.m_xmin-std::abs(lt.m_xmin)*outbound_margin; x <= lt.m_xmax+std::abs(lt.m_xmax)*outbound_margin; x+=lt.m_step/stepdiv, ++n) {
                double estim = lt.evaluate_lookup_table(x);
                double ref = lt.evaluate_ground_truth(x);
                double err = ref-estim;
                stats.ymin = std::min<float>(stats.ymin, ref);
                stats.ymax = std::max<float>(stats.ymax, ref);

                stats.abserr_max = std::max<double>(stats.abserr_max, std::abs(err));
                stats.abserr_mean += std::abs(err);

                double relerr;
                if (std::abs(ref) < 2*phaseshift::float32::eps()) relerr = err;
                else             relerr = err/ref;
                stats.relerr_max = std::max<double>(stats.relerr_max, std::abs(relerr));
                stats.relerr_mean += std::abs(relerr);

                // validation_file << ref << " " << estim << " " << relerr << std::endl;

                bool printout = false;
                if (std::isnan(ref)) printout = true;
                if (std::isinf(ref)) printout = true;
                if (std::isnan(estim)) printout = true;
                if (std::isinf(estim)) printout = true;
                if (std::isnan(relerr)) printout = true;
                if (std::isinf(relerr)) printout = true;
                // float ref_threshold = 10.0f;
                // float relerr_threshold = 0.1f;
                // if ((ref > ref_threshold) || (std::abs(relerr) > relerr_threshold)) printout = true;

                if (printout) {
                    std::cerr << "ERROR: x=" << std::scientific << x << " y'=" << estim << " vs y*=" << ref << " err=" << std::fixed << std::setprecision(3) << std::abs(err) << " relerr=" << std::fixed << std::setprecision(3) << std::abs(relerr) << std::endl;
                }
            }
            stats.abserr_mean /= n;
            stats.relerr_mean /= n;
            stats.rangerelerr_mean = stats.abserr_mean/(stats.ymax-stats.ymin);
            stats.rangerelerr_max = stats.abserr_max/(stats.ymax-stats.ymin);

            // validation_file.close();

            int N = 10000;
            PHASESHIFT_PROF(acbench::time_elapsed te_evaluate_lookup_table(N);)
            PHASESHIFT_PROF(acbench::time_elapsed te_evaluate_ground_truth(N);)
            float sum = 0.0;  // Just do something with the outputs to be sure the loop is not discarded by the compiler

            std::mt19937 gen(0);
            std::uniform_int_distribution<int> rnd_method(1, 2);

            for (int iter=0; iter < N; ++iter) {
                int methodi = rnd_method(gen);
                if (methodi == 1) {
                    PHASESHIFT_PROF(te_evaluate_ground_truth.start();)
                    for (float x = lt.m_xmin-std::abs(lt.m_xmin)*outbound_margin; x <= lt.m_xmax+std::abs(lt.m_xmax)*outbound_margin; x+=lt.m_step/stepdiv, ++n) {
                        auto y = lt.evaluate_ground_truth(x);
                        sum += y;
                    }
                    PHASESHIFT_PROF(te_evaluate_ground_truth.end(0.0f);)
                } else {
                    PHASESHIFT_PROF(te_evaluate_lookup_table.start();)
                    for (float x = lt.m_xmin-std::abs(lt.m_xmin)*outbound_margin; x <= lt.m_xmax+std::abs(lt.m_xmax)*outbound_margin; x+=lt.m_step/stepdiv, ++n) {
                        auto y = lt.evaluate_lookup_table(x);
                        sum += y;
                    }
                    PHASESHIFT_PROF(te_evaluate_lookup_table.end(0.0f);)
                }
            }

            assert(!std::isnan(sum));
            assert(!std::isinf(sum));

            std::cout << "INFO:     Precision: (ignore: " << sum << ")" << std::endl;
            std::cout << "INFO:         abserr_mean=" << stats.abserr_mean << ", abserr_max=" << stats.abserr_max << std::endl;
            std::cout << "INFO:         relerr_mean=" << stats.relerr_mean << ", relerr_max=" << stats.relerr_max << std::endl;
            std::cout << "INFO:         rangerelerr_mean=" << stats.rangerelerr_mean << ", rangerelerr_max=" << stats.rangerelerr_max << std::endl;
            #ifdef PHASESHIFT_DEV_PROFILING
            std::cout << "INFO:     Speed:" << std::endl;
            std::cout << "INFO:         Ground truth times: " << te_evaluate_ground_truth.stats(9) << std::endl;
            std::cout << "INFO:         Lookup table times: " << te_evaluate_lookup_table.stats(9) << std::endl;
            // std::cout << "INFO:         " << te_evaluate_ground_truth.median()/te_evaluate_lookup_table.median() << " faster." << std::endl;
            #endif

            return stats;
        }

        //! Generate a lookup table from a ground truth function
        template<class lookup_table_type>
        static void initialize(lookup_table_type* plt, float xmin, float xmax, int size) {
            lookup_table_type& lt = *plt;
            assert(size > 0);

            lt.m_xmin = xmin;
            lt.m_xmax = xmax;
            lt.m_size = size;
            lt.m_step = (lt.m_xmax-lt.m_xmin)/(lt.m_size-1);

            if (lt.m_values != nullptr) {
                delete[] lt.m_values;
                lt.m_values = nullptr;
            }

            lt.m_values = new float[lt.m_size];

            int n = 0;
            float x = lt.m_xmin;
            float x_last = x;
            for (; n < lt.m_size; x+=lt.m_step, ++n) {
                lt.m_values[n] = lt.evaluate_ground_truth(x);
                x_last = x;
            }
            lt.m_xmax = x_last;  // Correct xmax because it might not have been reached precisely by xmin+N*step

            lt.m_x2i = (lt.m_size-1) / (lt.m_xmax-lt.m_xmin);
        }

        lookup_table();
        ~lookup_table();

        float* values() const {return m_values;}
        int size() const {return m_size;}
        float x2i() const {return m_x2i;}

        //! Interpolation function for lookup tables with no boundary checks
        template<typename value_type>
        inline value_type interp_linear_unchecked_boundaries(value_type nf) const {
            assert(m_values != nullptr);
            // There is no need of boundary checks because its usage is bounded anyway.

            int n = static_cast<int>(nf);
            assert(n >= 0);
            assert(n < m_size);
            value_type prev = m_values[n];
            assert(n+1 >= 0);
            assert(n+1 < m_size);
            value_type next = m_values[n+1];
            value_type g = (nf - n);
            return (static_cast<value_type>(1)-g)*prev + g*next;
        }

        //! Interpolation function for lookup tables with boundary checks
        //! `nf` is the float-typed index (so already normalised by (x-m_xmin)*m_x2i )
        template<typename value_type>
        inline value_type interp_linear_checked_boundaries(value_type nf) const {
            assert(m_values != nullptr);
            // Usage is unbounded, so need to handle boundaries
            if (static_cast<int>(nf)+1 <= 0)        return m_values[0];
            assert(static_cast<int>(nf)+1 >= 0);
            if (static_cast<int>(nf) >= m_size-1)   return m_values[m_size-1];
            assert(static_cast<int>(nf) <= m_size-1);

            return interp_linear_unchecked_boundaries<value_type>(nf);
        }


        /* Inheriting classes should implement the following two functions:

        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {

            // This function returns the result computed using the lookup table,
            // by using either `interp_linear_unchecked_boundaries(.)`
            // or `interp_linear_checked_boundaries(.)`

        }

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {

            // This function returns the the result computed with the CPU heavy computation.

        }
        */
    };

    //! Lookup table for the linear to dB conversion in the range of [0,1]
    class lookup_table_lin012db : public lookup_table {
     protected:
        friend phaseshift::lookup_table;

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {
            return phaseshift::lin2db(x);
        }

     public:
        lookup_table_lin012db();

        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {
            return lookup_table::interp_linear_checked_boundaries((x-m_xmin)*m_x2i);
        }
    };
    static lookup_table_lin012db g_lt_lin012db_float;
    inline float lin012db_ltf(float x) {
        return g_lt_lin012db_float.evaluate_lookup_table(x); // TODO(GD) There is a dereferencing of the object... CPU costly for no reason
    }

    //! Lookup table for the dB to linear conversion in the range of [-300,0.0]
    class lookup_table_db2lin01 : public lookup_table {
     protected:
        friend phaseshift::lookup_table;

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {
            return phaseshift::db2lin(x);
        }

     public:
        lookup_table_db2lin01();

        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {
            return lookup_table::interp_linear_checked_boundaries((x-m_xmin)*m_x2i);
        }
    };
    static lookup_table_db2lin01 g_lt_db2lin01_float;
    inline float db2lin01_ltf(float x) {
        return g_lt_db2lin01_float.evaluate_lookup_table(x); // TODO(GD) There is a dereferencing of the object... CPU costly for no reason
    }

    //! Lookup table for the cosine function in the range of [0,2*pi]
    class lookup_table_cos : public lookup_table {
     protected:
        friend phaseshift::lookup_table;

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {
            return std::cos(x);
        }

     public:
        lookup_table_cos();

        // Can handle (-inf,+inf) bcs values are wrapped inside this function.
        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {
            // Function is symmetrical
            if (x < 0)
                x = -x;

            if (x > phaseshift::twopi)
                x -= int(x*phaseshift::oneover_twopi)*phaseshift::twopi;

            // Bcs the values are wrapped anyway, it is not necessary to check the boundaries.
            // return lookup_table::interp_linear_unchecked_boundaries(x*m_x2i);
            return m_values[static_cast<int>(x*m_x2i+0.5f)];  // TODO(GD) Quite a big diff of speed and not much in differences
        }
    };
    static lookup_table_cos g_lt_cos_float;
    static float* g_lt_cos_values = g_lt_cos_float.values();
    static float g_lt_cos_x2i = g_lt_cos_float.x2i();
    static int g_lt_cos_size = g_lt_cos_float.size();

    inline float cos_ltf(float x) {
        return g_lt_cos_float.evaluate_lookup_table(x);
    }

    //! Lookup table for the sin function in the range of [0,2*pi]
    class lookup_table_sin : public lookup_table {
     protected:
        friend phaseshift::lookup_table;

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {
            return std::sin(x);
        }

     public:
        lookup_table_sin();

        // Can handle (-inf,+inf) bcs values are wrapped inside this function.
        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {
            // Function is antisymmetrical
            bool is_negative = x < 0;
            if (is_negative)
                x = -x;

            if (x > phaseshift::twopi)
                x -= int(x*phaseshift::oneover_twopi)*phaseshift::twopi;

            // Bcs the values are wrapped anyway, it is not necessary to check the boundaries.
            // value_type ret = lookup_table::interp_linear_unchecked_boundaries(x*m_x2i);
            value_type ret = m_values[static_cast<int>(x*m_x2i+0.5f)];  // TODO(GD) Quite a big diff of speed and not much differences

            if (is_negative) {
                return -ret;
            } else {
                return ret;
            }
        }
    };
    static lookup_table_sin g_lt_sin_float;
    static float* g_lt_sin_values = g_lt_sin_float.values();
    static float g_lt_sin_x2i = g_lt_sin_float.x2i();
    static int g_lt_sin_size = g_lt_sin_float.size();

    inline float sin_ltf(float x) {
        return g_lt_sin_float.evaluate_lookup_table(x);
    }

}

#endif  // PHASESHIFT_LOOKUP_TABLE_H_
