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
        using acbench::ringbuffer<value_type>::push_front;  // Brings all push_front member functions from acbench::ringbuffer

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
        inline void push_front(const phaseshift::vector<value_type>& v) {
            acbench::ringbuffer<T>::push_front(v.data(), v.size());
        }

        // This case is commented out for being too generic
        // It might be used by mistake instead of the specific and efficient ones here after.
        // template<typename different_value_type>
        // inline void push_back(const different_value_type* array, int array_size) {
        //     for (int n=0; n < array_size; ++n)
        //         push_back(array[n]);
        // }

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
        ringbuffer& operator-=(const phaseshift::vector<value_type>& v) {
            assert(size() == v.size());

            if (v.size() == 0)
                return *this;

            if (acbr::m_front+v.size() <= acbr::m_size_max) {
                // No need to slice it
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < v.size(); ++n)
                    *pdata++ -= *pvdata++;

            } else {
                // Need to slice the array into two segments
                int seg1size = acbr::m_size_max - acbr::m_front;
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < seg1size; ++n)
                    *pdata++ -= *pvdata++;

                int seg2size = v.size() - seg1size;
                pdata = acbr::m_data;
                pvdata = v.m_data + seg1size;
                for (int n = 0; n < seg2size; ++n)
                    *pdata++ -= *pvdata++;
            }
            return *this;
        }
        ringbuffer& operator-=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            if (rb.size() == 0)
                return *this;

            if (acbr::m_front+rb.size() <= acbr::m_size_max) {
                // Destination is continuous
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is also continuous
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < rb.size(); ++n)
                        *pdst++ -= *psrc++;
                } else {
                    // Source wraps around
                    int seg1size = rb.m_size_max - rb.m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ -= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    psrc = rb.m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ -= *psrc++;
                }
            } else {
                // Destination wraps around
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is continuous
                    int seg1size = acbr::m_size_max - acbr::m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ -= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    pdst = acbr::m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ -= *psrc++;
                } else {
                    // Both wrap - check alignment
                    int dst_seg1 = acbr::m_size_max - acbr::m_front;
                    int src_seg1 = rb.m_size_max - rb.m_front;
                    if (dst_seg1 == src_seg1) {
                        // Aligned wrap points
                        value_type* pdst = acbr::m_data+acbr::m_front;
                        value_type* psrc = rb.m_data+rb.m_front;
                        for (int n = 0; n < dst_seg1; ++n)
                            *pdst++ -= *psrc++;

                        int seg2size = rb.size() - dst_seg1;
                        pdst = acbr::m_data;
                        psrc = rb.m_data;
                        for (int n = 0; n < seg2size; ++n)
                            *pdst++ -= *psrc++;
                    } else {
                        // Misaligned - fallback to indexed access
                        for (int n = 0; n < rb.size(); ++n)
                            (*this)[n] -= rb[n];
                    }
                }
            }
            return *this;
        }
        ringbuffer& operator*=(const phaseshift::vector<value_type>& v) {
            assert(size() == v.size());

            if (v.size() == 0)
                return *this;

            if (acbr::m_front+v.size() <= acbr::m_size_max) {
                // No need to slice it
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < v.size(); ++n)
                    *pdata++ *= *pvdata++;

            } else {
                // Need to slice the array into two segments
                int seg1size = acbr::m_size_max - acbr::m_front;
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < seg1size; ++n)
                    *pdata++ *= *pvdata++;

                int seg2size = v.size() - seg1size;
                pdata = acbr::m_data;
                pvdata = v.m_data + seg1size;
                for (int n = 0; n < seg2size; ++n)
                    *pdata++ *= *pvdata++;
            }
            return *this;
        }
        ringbuffer& operator*=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            if (rb.size() == 0)
                return *this;

            if (acbr::m_front+rb.size() <= acbr::m_size_max) {
                // Destination is continuous
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is also continuous
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < rb.size(); ++n)
                        *pdst++ *= *psrc++;
                } else {
                    // Source wraps around
                    int seg1size = rb.m_size_max - rb.m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ *= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    psrc = rb.m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ *= *psrc++;
                }
            } else {
                // Destination wraps around
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is continuous
                    int seg1size = acbr::m_size_max - acbr::m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ *= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    pdst = acbr::m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ *= *psrc++;
                } else {
                    // Both wrap - check alignment
                    int dst_seg1 = acbr::m_size_max - acbr::m_front;
                    int src_seg1 = rb.m_size_max - rb.m_front;
                    if (dst_seg1 == src_seg1) {
                        // Aligned wrap points
                        value_type* pdst = acbr::m_data+acbr::m_front;
                        value_type* psrc = rb.m_data+rb.m_front;
                        for (int n = 0; n < dst_seg1; ++n)
                            *pdst++ *= *psrc++;

                        int seg2size = rb.size() - dst_seg1;
                        pdst = acbr::m_data;
                        psrc = rb.m_data;
                        for (int n = 0; n < seg2size; ++n)
                            *pdst++ *= *psrc++;
                    } else {
                        // Misaligned - fallback to indexed access
                        for (int n = 0; n < rb.size(); ++n)
                            (*this)[n] *= rb[n];
                    }
                }
            }
            return *this;
        }
        ringbuffer& operator/=(const phaseshift::vector<value_type>& v) {
            assert(size() == v.size());

            if (v.size() == 0)
                return *this;

            if (acbr::m_front+v.size() <= acbr::m_size_max) {
                // No need to slice it
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < v.size(); ++n)
                    *pdata++ /= *pvdata++;

            } else {
                // Need to slice the array into two segments
                int seg1size = acbr::m_size_max - acbr::m_front;
                value_type* pdata = acbr::m_data+acbr::m_front;
                value_type* pvdata = v.m_data;
                for (int n = 0; n < seg1size; ++n)
                    *pdata++ /= *pvdata++;

                int seg2size = v.size() - seg1size;
                pdata = acbr::m_data;
                pvdata = v.m_data + seg1size;
                for (int n = 0; n < seg2size; ++n)
                    *pdata++ /= *pvdata++;
            }
            return *this;
        }
        ringbuffer& operator/=(const phaseshift::ringbuffer<value_type>& rb) {
            assert(size() == rb.size());

            if (rb.size() == 0)
                return *this;

            if (acbr::m_front+rb.size() <= acbr::m_size_max) {
                // Destination is continuous
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is also continuous
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < rb.size(); ++n)
                        *pdst++ /= *psrc++;
                } else {
                    // Source wraps around
                    int seg1size = rb.m_size_max - rb.m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ /= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    psrc = rb.m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ /= *psrc++;
                }
            } else {
                // Destination wraps around
                if (rb.m_front+rb.size() <= rb.m_size_max) {
                    // Source is continuous
                    int seg1size = acbr::m_size_max - acbr::m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n = 0; n < seg1size; ++n)
                        *pdst++ /= *psrc++;

                    int seg2size = rb.size() - seg1size;
                    pdst = acbr::m_data;
                    for (int n = 0; n < seg2size; ++n)
                        *pdst++ /= *psrc++;
                } else {
                    // Both wrap - check alignment
                    int dst_seg1 = acbr::m_size_max - acbr::m_front;
                    int src_seg1 = rb.m_size_max - rb.m_front;
                    if (dst_seg1 == src_seg1) {
                        // Aligned wrap points
                        value_type* pdst = acbr::m_data+acbr::m_front;
                        value_type* psrc = rb.m_data+rb.m_front;
                        for (int n = 0; n < dst_seg1; ++n)
                            *pdst++ /= *psrc++;

                        int seg2size = rb.size() - dst_seg1;
                        pdst = acbr::m_data;
                        psrc = rb.m_data;
                        for (int n = 0; n < seg2size; ++n)
                            *pdst++ /= *psrc++;
                    } else {
                        // Misaligned - fallback to indexed access
                        for (int n = 0; n < rb.size(); ++n)
                            (*this)[n] /= rb[n];
                    }
                }
            }
            return *this;
        }
        //! *this /= rb (only first 'size' elements)
        void divide_equal_range(const phaseshift::ringbuffer<value_type>& rb, int size) {
            assert(size <= acbr::m_size);
            assert(size <= rb.m_size);

            if (size == 0)
                return;

            if (acbr::m_front+size <= acbr::m_size_max) {
                // The destination segment is continuous

                if (rb.m_front+size <= rb.size_max()) {
                    // The source segment is also continuous
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n=0; n < size; ++n)
                        *pdst++ /= *psrc++;

                } else {
                    // Destination continuous, source wraps around
                    int src_seg1 = rb.size_max() - rb.m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n=0; n < src_seg1; ++n)
                        *pdst++ /= *psrc++;

                    int src_seg2 = size - src_seg1;
                    psrc = rb.m_data;
                    for (int n=0; n < src_seg2; ++n)
                        *pdst++ /= *psrc++;
                }

            } else {
                // The destination segment wraps around

                if (rb.m_front+size <= rb.size_max()) {
                    // Destination wraps, source is continuous
                    int dst_seg1 = acbr::m_size_max - acbr::m_front;
                    value_type* pdst = acbr::m_data+acbr::m_front;
                    value_type* psrc = rb.m_data+rb.m_front;
                    for (int n=0; n < dst_seg1; ++n)
                        *pdst++ /= *psrc++;

                    int dst_seg2 = size - dst_seg1;
                    pdst = acbr::m_data;
                    for (int n=0; n < dst_seg2; ++n)
                        *pdst++ /= *psrc++;

                } else {
                    // Both wrap around
                    int dst_seg1 = acbr::m_size_max - acbr::m_front;
                    int src_seg1 = rb.size_max() - rb.m_front;

                    if (dst_seg1 == src_seg1) {
                        // Aligned wrap points - handle 2 segments
                        value_type* pdst = acbr::m_data + acbr::m_front;
                        value_type* psrc = rb.m_data + rb.m_front;
                        for (int n=0; n < dst_seg1; ++n)
                            *pdst++ /= *psrc++;

                        int seg2size = size - dst_seg1;
                        pdst = acbr::m_data;
                        psrc = rb.m_data;
                        for (int n=0; n < seg2size; ++n)
                            *pdst++ /= *psrc++;

                    } else if (src_seg1 < dst_seg1) {
                        // Source breaks first - handle 3 segments
                        value_type* pdst = acbr::m_data + acbr::m_front;
                        value_type* psrc = rb.m_data + rb.m_front;

                        // Segment 1: both at start positions, up to source wrap
                        for (int n=0; n < src_seg1; ++n)
                            *pdst++ /= *psrc++;

                        // Segment 2: destination continues, source wraps
                        psrc = rb.m_data;
                        int seg2size = dst_seg1 - src_seg1;
                        for (int n=0; n < seg2size; ++n)
                            *pdst++ /= *psrc++;

                        // Segment 3: destination wraps
                        pdst = acbr::m_data;
                        int seg3size = size - dst_seg1;
                        for (int n=0; n < seg3size; ++n)
                            *pdst++ /= *psrc++;

                    } else {
                        // Destination breaks first - handle 3 segments
                        value_type* pdst = acbr::m_data + acbr::m_front;
                        value_type* psrc = rb.m_data + rb.m_front;

                        // Segment 1: both at start positions, up to dest wrap
                        for (int n=0; n < dst_seg1; ++n)
                            *pdst++ /= *psrc++;

                        // Segment 2: source continues, destination wraps
                        pdst = acbr::m_data;
                        int seg2size = src_seg1 - dst_seg1;
                        for (int n=0; n < seg2size; ++n)
                            *pdst++ /= *psrc++;

                        // Segment 3: source wraps
                        psrc = rb.m_data;
                        int seg3size = size - src_seg1;
                        for (int n=0; n < seg3size; ++n)
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
