// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_CONTAINERS_RINGBUFFER_H_
#define PHASESHIFT_CONTAINERS_RINGBUFFER_H_

#include <phaseshift/utils.h>
#include <acbench/ringbuffer.h>

#include <cstring>
#include <complex>

#include <phaseshift/containers/utils.h>

namespace phaseshift {
    template<typename T> class vector;

    // Inherit from acbench::ringbuffer<T> and add some convenience functions for phaseshift scenarios.
    template<typename T>
    class ringbuffer : public acbench::ringbuffer<T> {
        template<typename _T> friend class vector;

    public:
        typedef T value_type;
        typedef acbench::ringbuffer<value_type> acbr;

        using acbench::ringbuffer<value_type>::size;
        using acbench::ringbuffer<value_type>::push_back;   // Brings all push_back member functions from acbench::ringbuffer

        //! Convenience function
        inline void push_back(const double* array, int array_size) {
            for (int n=0; n < array_size; ++n)
                push_back(array[n]);
        }

        inline void push_back(const phaseshift::vector<value_type>& v) {
            push_back(v.data(), v.size());
        }

        inline void push_back(const phaseshift::vector<value_type>& v, int start, int size) {
            assert(start >= 0);
            // assert(size <= v.size() - start);
            size = std::min(size, v.size() - start);
            push_back(v.data() + start, size);
        }

        // This case is commented out for being too generic
        // It might be used by mistake instead of the specific and efficient ones here after.
        // template<typename different_value_type>
        // inline void push_back(const different_value_type* array, int array_size) {
        //     for (int n=0; n < array_size; ++n)
        //         push_back(array[n]);
        // }

        //! Push back only a segment of the ringbuffer given as argument.
        inline void push_back(const ringbuffer<value_type>& rb, int start, int size) {
            if (rb.size() == 0)     return;  // Ignore push of empty ringbuffers
            if (size == 0)          return;  // Ignore push of empty data
            if (start >= rb.size()) return;  // Ignore push of empty data

            if (start+size > rb.size())
                size = rb.size() - start;
            int rb_size = size;

            int rb_front = rb.m_front + start;
            if (rb_front >= rb.m_size_max)
                rb_front -= rb.m_size_max;

            acbr::memory_check_size_nolock(rb_size);

            if (acbr::m_end+rb_size <= acbr::m_size_max) {
                // The destination segment is continuous

                // Now let's see the source segment(s)
                if (rb_front+rb_size <= rb.m_size_max) {
                    // The source segment is continuous...
                    // ... easiest game of my life
                    acbr::memory_copy_nolock(acbr::m_data+acbr::m_end, rb.m_data+rb_front, rb_size);

                } else {
                    // The source segment is made of two continuous segments

                    // 1st segment
                    int seg1size = rb.m_size_max - rb_front;
                    acbr::memory_copy_nolock(acbr::m_data+acbr::m_end, rb.m_data+rb_front, seg1size);

                    // 2nd segment
                    int seg2size = rb_size - seg1size;
                    acbr::memory_copy_nolock(acbr::m_data+acbr::m_end+seg1size, rb.m_data, seg2size);
                }

                acbr::m_end += rb_size;

            } else {
                // The destination segment is made of two continuous segments

                if (rb_front+rb_size <= rb.m_size_max) {
                    // The source segment is continuous...

                    // 1st segment
                    int seg1size = acbr::m_size_max - acbr::m_end;
                    acbr::memory_copy_nolock(acbr::m_data+acbr::m_end, rb.m_data+rb_front, seg1size);

                    // 2nd segment
                    int seg2size = rb_size - seg1size;
                    acbr::memory_copy_nolock(acbr::m_data, rb.m_data+rb_front+seg1size, seg2size);

                } else {
                    // The source segment is also made of two continuous segments...
                    // ... worst game of my life.

                    // Let's check if the source's break point comes before or after the destination's max size.
                    if ((rb.size_max()-rb_front) < (acbr::m_size_max-acbr::m_end)) {
                        // the source's break point comes before the destination's max size...
                        // .. handle the 3 resulting segments

                        // 1st segment
                        int seg1size = rb.m_size_max - rb_front;
                        acbr::memory_copy_nolock(acbr::m_data+acbr::m_end, rb.m_data+rb_front, seg1size);

                        // 2nd segment
                        int seg2size = (acbr::m_size_max-acbr::m_end) - seg1size;
                        acbr::memory_copy_nolock(acbr::m_data+acbr::m_end+seg1size, rb.m_data, seg2size);

                        // 3rd segment
                        int seg3size = rb_size - seg1size - seg2size;
                        acbr::memory_copy_nolock(acbr::m_data, rb.m_data+seg2size, seg3size);

                    } else {
                        // the source's break point comes after or on the destination's max size...
                        // .. handle the 3 resulting segments

                        // 1st segment
                        int seg1size = acbr::m_size_max - acbr::m_end;
                        acbr::memory_copy_nolock(acbr::m_data+acbr::m_end, rb.m_data+rb_front, seg1size);

                        // 2nd segment
                        int seg2size = (rb.m_size_max-rb_front) - seg1size;
                        acbr::memory_copy_nolock(acbr::m_data, rb.m_data+rb_front+seg1size, seg2size);

                        // 3rd segment
                        int seg3size = rb_size - seg1size - seg2size;
                        acbr::memory_copy_nolock(acbr::m_data+seg2size, rb.m_data, seg3size);
                    }
                }

                // m_end = seg2size; // TODO Isn't it equal to this?
                acbr::m_end += rb_size;
                if (acbr::m_end >= acbr::m_size_max)
                    acbr::m_end -= acbr::m_size_max;
            }

            acbr::m_size += rb_size;
        }

        ringbuffer& operator+=(float v) {

            for (int n=0; n < size(); ++n)
                (*this)[n] += v;

            return *this;
        }
        ringbuffer& operator-=(float v) {

            for (int n=0; n < size(); ++n)
                (*this)[n] -= v;

            return *this;
        }
        ringbuffer& operator*=(float v) {

            for (int n=0; n < size(); ++n)
                (*this)[n] *= v;

            return *this;
        }
        ringbuffer& operator/=(float v) {

            for (int n=0; n < size(); ++n)
                (*this)[n] /= v;

            return *this;
        }

        ringbuffer& operator+=(const phaseshift::vector<value_type>& v) {
            assert(size() == v.size());

            if (v.size() == 0)
                return *this;

            if (acbr::m_front+v.size() <= acbr::m_size_max) {
                // No need to slice it

                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < v.size(); ++n)
                    *pdata++ += *pvdata++;

            } else {
                // Need to slice the array into two segments

                // 1st segment: m_end:m_size_max-1
                int seg1size = acbr::m_size_max - acbr::m_end;
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < seg1size; ++n)
                    *pdata++ += *pvdata++;

                // 2nd segment: 0:array_size-seg1size
                int seg2size = v.size() - seg1size;
                pdata = acbr::m_data;
                pvdata = v.m_data + seg1size;
                for (int n = 0; n < seg2size; ++n)
                    *pdata++ += *pvdata++;

            }
            return *this;
        }
        // TODO(GD) SPEEDUP: like += operator
        ringbuffer& operator-=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            for (int n=0; n < rb.size(); ++n)
                (*this)[n] -= rb[n];

            return *this;
        }
        // TODO(GD) SPEEDUP: like += operator
        ringbuffer& operator*=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            for (int n=0; n < rb.size(); ++n)
                (*this)[n] *= rb[n];

            return *this;
        }
        // TODO(GD) SPEEDUP: like += operator
        ringbuffer& operator/=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            for (int n=0; n < rb.size(); ++n)
                (*this)[n] /= rb[n];

            return *this;
        }
        //! *this /= rb
        void divide_equal_range(const phaseshift::ringbuffer<value_type>& rb, int size) {
            assert(size <= acbr::m_size);
            assert(size <= rb.m_size);

            if (acbr::m_front+size <= acbr::m_size_max) {
                // The destination segment is continuous

                // Now let's see the source segment(s)
                if (rb.m_front+size <= rb.size_max()) {
                    // The source segment is continuous...
                    // ... easiest game of my life
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n=0; n < size; ++n)
                        *pdst++ /= *psrc++;

                } else {
                    // The source segment is made of two continuous segments

                    // Case never met yet:
                    // Because the scenario that motivated writting ` divide_equal_range(.)`  was based on synchronous ringbuffers, which falls on cases (i) both segment are continuous, or (ii) both segments are non-continuous (with rb.size_max()-rb.m_front) == (size_max()-m_front) )

                    // TODO(GD) SPEEDUP
                    for (int n=0; n < size; ++n)
                        (*this)[n] /= rb[n];
                }

            } else {
                // The destination segment is made of two continuous segments

                if (rb.m_front+size <= rb.size_max()) {
                    // The source segment is continuous...

                    // Case never met yet:
                    // Because the scenario that motivated writting ` divide_equal_range(.)`  was based on synchronous ringbuffers, which falls on cases (i) both segment are continuous, or (ii) both segments are non-continuous (with rb.size_max()-rb.m_front) == (size_max()-m_front) )

                    // TODO(GD) SPEEDUP
                    for (int n=0; n < size; ++n)
                        (*this)[n] /= rb[n];

                } else {
                    // The source segment is also made of two continuous segments...
                    // ... worst game of my life.

                    // Let's check if the source's break point comes before or after the destination's max size.
                    int diff = (rb.size_max()-rb.m_front) - (acbr::m_size_max-acbr::m_front);
                    if (diff < 0) {

                        // TODO(GD) SPEEDUP
                        for (int n=0; n < size; ++n)
                            (*this)[n] /= rb[n];

                    } else if (diff > 0) {

                        // TODO(GD) SPEEDUP
                        for (int n=0; n < size; ++n)
                            (*this)[n] /= rb[n];

                    } else {  //  -> diff == 0

                        // the source's break point comes on the destination's max size...
                        // .. handle the 2 resulting segments

                        // 1st segment
                        int seg1size = acbr::m_size_max - acbr::m_front;
                        value_type* pdst = acbr::m_data + acbr::m_front;
                        value_type* psrc = rb.m_data+rb.m_front;
                        for (int n=0; n < seg1size; ++n)
                            *pdst++ /= *psrc++;

                        // 2rd segment
                        int seg2size = size - seg1size;
                        pdst = acbr::m_data;
                        psrc = rb.m_data;
                        for (int n=0; n < seg2size; ++n)
                            *pdst++ /= *psrc++;
                    }
                }
            }
        }
    };

    namespace dev {

        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const phaseshift::ringbuffer<std::complex<value_type>>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_complex64(filepath, array, mode);
        }
        
        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const phaseshift::ringbuffer<value_type>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_float32(filepath, array, mode);
        }

    } // namespace dev

}  // namespace phaseshift

#endif  // PHASESHIFT_CONTAINERS_RINGBUFFER_H_
