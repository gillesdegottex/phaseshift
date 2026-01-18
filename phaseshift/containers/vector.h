// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_CONTAINERS_VECTOR_H_
#define PHASESHIFT_CONTAINERS_VECTOR_H_

#include <phaseshift/utils.h>

#include <Eigen/Dense>

#include <fftscarf.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <phaseshift/containers/utils.h>

// Cross-platform aligned memory allocation for SIMD optimization
// Default to 32-byte alignment supports AVX/AVX2
// For AVX-512, 64-byte alignment: Define PHASESHIFT_SIMD_ALIGNMENT before including the header or via compiler flags: -DPHASESHIFT_SIMD_ALIGNMENT=64
// Disable alignment: If you don't need SIMD optimization: -DPHASESHIFT_SIMD_ALIGNMENT=1
#ifndef PHASESHIFT_SIMD_ALIGNMENT
#define PHASESHIFT_SIMD_ALIGNMENT 32
#endif

namespace phaseshift {
    namespace allocation {

        inline void* aligned_malloc(size_t size, size_t alignment) {
            if (size == 0) return nullptr;
            #if defined(_MSC_VER) || defined(__MINGW32__)
                return _aligned_malloc(size, alignment);
            #elif defined(__ANDROID__)
                #if defined(ANDROID_API) && (ANDROID_API >= 28)
                    // std::aligned_alloc requires size to be a multiple of alignment
                    size_t aligned_size = ((size + alignment - 1) / alignment) * alignment;
                    return std::aligned_alloc(alignment, aligned_size);
                #else
                    size_t aligned_size = ((size + alignment - 1) / alignment) * alignment;
                    void* ptr = nullptr;
                    if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
                        return nullptr;
                    }
                    return ptr;
                #endif
            #else
                // std::aligned_alloc requires size to be a multiple of alignment
                size_t aligned_size = ((size + alignment - 1) / alignment) * alignment;
                return std::aligned_alloc(alignment, aligned_size);
            #endif
            return nullptr;
        }

        inline void aligned_free(void* ptr) {
            if (ptr == nullptr) return;
            #if defined(_MSC_VER) || defined(__MINGW32__)
                _aligned_free(ptr);
            #else
                std::free(ptr);
            #endif
        }

    }  // namespace allocation
}  // namespace phaseshift

namespace phaseshift {

    template<typename T> class ringbuffer;

    // TODO(GD) Try inheritance from Eigen, or:
    //  typedef Eigen::ArrayXf vector;
    //  template<class T> using vector = Eigen::Array<T, Eigen::Dynamic, 1>;
    // template<typename T>
    // class vector : public Eigen::Array<T, Eigen::Dynamic, 1> {
    // };

    template<typename T>
    class vector {
     public:
        typedef T value_type;

     protected:
        int m_size_max = 0;

     public:
        inline int size_max() const {
            return m_size_max;
        }

     private:
        int m_size = 0;
        T* m_data = nullptr;

        inline void destroy() {
            if (m_data != nullptr) {
                phaseshift::allocation::aligned_free(m_data);
                m_data = nullptr;
            }
            m_size = 0;
            m_size_max = 0;
        }

        // Copy constructor is forbidden to avoid implicit calls.
        // Do it manually if necessary (using `.resize_allocation(.)` and assignment operation or `.push_back(.)`)
        explicit vector(const vector& vec) {
            (void)vec;
        }

     protected:
        inline void memory_copy(value_type* pdest, const value_type* psrc, int size) {
            if (size==0) return;
            assert(size > 0);
            std::memcpy(reinterpret_cast<void*>(pdest), reinterpret_cast<const void*>(psrc), sizeof(value_type)*size);
        }

     public:
        vector() {
        }
        explicit vector(int size_max_and_size) {
            resize_allocation(size_max_and_size);
            resize(size_max_and_size);
        }
        void resize_allocation(int size_max) {
            if (size_max == m_size_max) {
                clear();
                return;
            }
            destroy();

            // Aligned allocation for SIMD optimization (AVX/AVX2 requires 32-byte alignment)
            m_data = static_cast<value_type*>(
                phaseshift::allocation::aligned_malloc(sizeof(value_type) * size_max, PHASESHIFT_SIMD_ALIGNMENT));
            m_size_max = size_max;

            m_size = 0;
        }
        // Release ownership of the internal buffer.
        // Returns {pointer, size}. Caller takes ownership and must call aligned_free().
        // WARNING: After this call, the vector is invalid, an empty shell, without allocation.
        inline std::pair<value_type*, int> release_allocation() {
            value_type* ptr = m_data;
            int sz = m_size;
            m_data = nullptr;
            m_size = 0;
            m_size_max = 0;
            return {ptr, sz};
        }
        inline bool is_valid() const {
            return m_data != nullptr;
        }
        ~vector() {
            destroy();
        }

        inline void clear() {
            m_size = 0;
        }

        value_type* data() const {
            return m_data;
        }

        int size() const {
            return m_size;
        }
        bool empty() const {
            return m_size == 0;
        }

        typedef value_type* iterator;
        iterator begin() {return m_data;}
        iterator end() {return m_data+m_size;}

        value_type operator[](int n) const {
            assert(n < m_size);
            return m_data[n];
        }
        value_type& operator[](int n) {
            assert(n < m_size);
            return m_data[n];
        }

        inline void resize(int size) {
            assert(size <= m_size_max);

            if (size == m_size)
                return;

            m_size = size;
        }

        inline void push_back(const value_type v) {
            assert(m_size+1 <= m_size_max);

            m_data[m_size] = v;

            ++m_size;
        }
        inline void push_back(const value_type* array, int array_size) {
            assert(m_size+array_size <= m_size_max);

            // No need to slice it
            memory_copy(m_data+m_size, array, array_size);

            m_size += array_size;
        }
        inline void push_back(const phaseshift::vector<value_type>& array) {
            push_back(array.data(), array.size());
        }

        inline value_type front() const {
            assert(m_size > 0);
            return m_data[0];
        }
        inline value_type back() const {
            assert(m_size > 0);
            return m_data[m_size-1];
        }
        inline void pop_back() {
            if (m_size == 0)                  // Ignore pops on empty vectors
                return;

            --m_size;
        }

        inline void pop_back(int n) {
            if (m_size == 0)                  // Ignore pops on empty vectors
                return;

            if (n >= m_size) {                // Just clears all if not enough to be poped
                clear();
                return;
            }

            m_size -= n;
        }

        inline void operator=(const phaseshift::vector<value_type>& vec) {
            resize(vec.size());
            memory_copy(m_data, vec.m_data, vec.m_size);
        }
        inline void operator=(const phaseshift::ringbuffer<value_type>& rb) {
            resize(rb.size());

            if (rb.m_front+rb.size() <= rb.size_max()) {
                // The whole content is within monotonically increasing indices in [0, size_max)

                memory_copy(m_data, rb.m_data+rb.m_front, rb.size());

            } else {
                // The content is split between a first segment that is in [m_front, size_max-1]
                // and a second segment that is in [0, m_back-1]

                // 1st segment
                int seg1size = rb.size_max() - rb.m_front;
                memory_copy(m_data, rb.m_data+rb.m_front, seg1size);

                // 2nd segment
                int seg2size = rb.size() - seg1size;
                memory_copy(m_data+seg1size, rb.m_data, seg2size);
            }
        }
        inline void operator=(const value_type& value) {
            value_type* pdata = m_data;
            for (int n = 0; n < size(); ++n)
                *pdata++ = value;
        }

        template<typename T2>
        inline vector& operator+=(const phaseshift::vector<T2>& arr) {
            assert(size() == arr.size());

            value_type* pdata = m_data;
            T2* pdata2 = arr.m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ += *pdata2++;

            return *this;
        }
        template<typename T2>
        inline vector& operator-=(const phaseshift::vector<T2>& arr) {
            assert(size() == arr.size());

            value_type* pdata = m_data;
            T2* pdata2 = arr.m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ -= *pdata2++;

            return *this;
        }
        template<typename T2>
        inline vector& operator*=(const phaseshift::vector<T2>& arr) {
            assert(size() == arr.size());

            value_type* pdata = m_data;
            T2* pdata2 = arr.m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ *= *pdata2++;

            return *this;
        }
        template<typename T2>
        inline vector& operator/=(const phaseshift::vector<T2>& arr) {
            assert(size() == arr.size());

            value_type* pdata = m_data;
            T2* pdata2 = arr.m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ /= *pdata2++;

            return *this;
        }

        inline vector& operator+=(value_type v) {

            value_type* pdata = m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ += v;

            return *this;
        }
        inline vector& operator-=(value_type v) {

            value_type* pdata = m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ -= v;

            return *this;
        }
        inline vector& operator*=(value_type v) {

            value_type* pdata = m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ *= v;

            return *this;
        }
        inline vector& operator/=(value_type v) {

            value_type* pdata = m_data;
            for (int n=0; n < m_size; ++n)
                *pdata++ /= v;

            return *this;
        }

        template<typename _T> friend class ringbuffer;
    };

    namespace dev {

        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const phaseshift::vector<std::complex<value_type>>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_complex64(filepath, array, mode);
        }

        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const phaseshift::vector<value_type>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_float32(filepath, array, mode);
        }

    } // namespace dev

}  // namespace phaseshift


namespace fftscarf{
    template<typename T>struct is_container_complex<phaseshift::vector<std::complex<T>>> : public std::true_type {};
}

#endif  // PHASESHIFT_CONTAINERS_VECTOR_H_
