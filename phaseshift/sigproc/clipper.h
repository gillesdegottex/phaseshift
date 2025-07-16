// Copyright (C) 2025 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_SIGPROC_CLIPPER_H_
#define PHASESHIFT_SIGPROC_CLIPPER_H_

#include <cstddef>
#include <cmath>

#include <phaseshift/lookup_table.h>

namespace phaseshift {

    /*
    Lookup table for a clipping function in interval [0, 1].
    In the full clipping function `clipper(.)`, it will be used only between knee and limit.
    */
    class lookup_table_clipper01 : public lookup_table {
     protected:
        friend phaseshift::lookup_table;

     public:
        lookup_table_clipper01();

        template<typename value_type>
        inline value_type evaluate_lookup_table(value_type x) const {
            float nf = x*m_x2i;  // m_xmin==0.0f => (x-m_xmin)*m_x2i;
            if (static_cast<int>(nf) >= m_size-1)  return 1.0f;
            float y = lookup_table::interp_linear_unchecked_boundaries(nf);
            return y;
        }

        template<typename value_type>
        inline value_type evaluate_ground_truth(value_type x) const {
            assert(false && "lookup_table_clipper::evaluate_ground_truth: ground truth of clipper is generated using clipper_table_gen.py");
        }
    };
    static lookup_table_clipper01 g_clipper_lt;

    static const float clipper_knee_def = 0.66f;
    static const float clipper_limit_def = 127.0f/128;  // 8-bit signed upper limit (=0.9921875)
    template<class array_type>
    void clipper(array_type* in, float knee=clipper_knee_def, float limit=clipper_limit_def) {

        assert(limit > knee);

        float transition_band = limit - knee;
        float invtransition_band = 1.0f/transition_band;

        for (int n=0; n<int(in->size()); ++n) {
            float v = (*in)[n];

            if (v > +knee) {
                float c = g_clipper_lt.evaluate_lookup_table((v-knee)*invtransition_band);
                (*in)[n] = knee + transition_band * c;
            } else if (v < -knee) {
                float c = g_clipper_lt.evaluate_lookup_table((-v-knee)*invtransition_band);
                (*in)[n] = -(knee + transition_band * c);
            }
        }
    }
}

#endif  // PHASESHIFT_SIGPROC_CLIPPER_H_
