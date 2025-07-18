// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_INTERPOLATION_H_
#define PHASESHIFT_INTERPOLATION_H_

#include <cstddef>
#include <cmath>

#include <phaseshift/lookup_table.h>
#include <phaseshift/containers/vector.h>

namespace phaseshift {

    //! Linear interpolation of the closest points around `nf`
    template<typename value_type, class vector_in>
    inline value_type interp_linear(const vector_in& src, value_type nf) {

        if (nf <= 0.0f)
            return src[0];

        if (nf >= src.size()-1)
            return src[src.size()-1];

        int n = int(nf);
        value_type g = (nf-n);

        return (1.0f-g)*src[n] + g*src[n+1];
    }

    //! Linear interpolation by means of a time instant `t`.
    //  Values are indexed by a given series of time instant `pts`.
    //  So the distance between subsequent points can be not constant.
    //  Once reset(.) is called, `t` is assumed to be then monotonically increasing in subsequent calls of operator(.)
    class interp_linear_increasing_t {
        phaseshift::vector<float>* m_pts = nullptr;
        phaseshift::vector<float>* m_pvs = nullptr;
        int m_n = 0;
     public:

        inline void reset() {
            m_n = 0;
        }

        inline void reset(phaseshift::vector<float>* pts, phaseshift::vector<float>* pvs) {
            m_n = 0;
            m_pts = pts;
            m_pvs = pvs;
        }

        bool valid() const {
            return (m_pvs != nullptr) && (m_pvs->size() > 0);
        }

        inline float operator()(double t) {
            assert(m_pts != nullptr);
            assert(m_pvs != nullptr);
            assert(m_pts->size() == m_pvs->size());
            assert(m_pvs->size() > 0);

            if ((m_n == 0) && (t <= (*m_pts)[0]))
                return (*m_pvs)[0];

            if (t >= (*m_pts)[m_pts->size()-1])
                return (*m_pvs)[m_pvs->size()-1];

            while ((m_n+1 < m_pts->size()-1) && (t > (*m_pts)[m_n+1]))
                m_n++;

            if (m_n >= m_pts->size()-1)
                return (*m_pvs)[m_pvs->size()-1];

            assert(m_n < m_pts->size());
            assert(m_n+1 < m_pts->size());

            float g = (t-(*m_pts)[m_n]) / ((*m_pts)[m_n+1]-(*m_pts)[m_n]);

            return (1.0f-g)*(*m_pvs)[m_n] + g*(*m_pvs)[m_n+1];
        }
    };

    inline float sinc(float x) {
        if (x == 0.0f)
            return 1.0f;

        x *= M_PI;

        return std::sin(x)/x;
    }

    /*! Computation of a weight of a raised cosine
        beta=0.25: similar to sinc+hamming
        N=33 seems enough to reach almost perfect interpolation using raised cosine
    */
    template<typename value_type>
    inline value_type raisedcosin_weight(value_type t, value_type beta) {
        value_type beta2 = 2*beta;
        value_type base;
        value_type w;

        if (std::abs(t) == 1/beta2) {
            w = (M_PI/4) * sinc(1.0/beta2);
        } else {
            w = sinc(t) * std::cos(M_PI*beta*t);
            base = beta2*t;
            w /= 1.0 - base*base;
        }

        // (no need to add an extra window function like with the sinc+hamming, there is already one made by the cosine)

        return w;
    }

}  // namespace phaseshift

#endif  // PHASESHIFT_INTERPOLATION_H_
