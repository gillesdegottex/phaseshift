// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_PY_NANOBIND_H_
#define PHASESHIFT_PY_NANOBIND_H_

#include <cstddef>
#include <cstring>
#include <map>
#include <algorithm>
#include <limits>

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/complex.h>
#include <nanobind/ndarray.h>
namespace nb = nanobind;

#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/containers/vector.h>

// Note: ndarray to something should use intermediate convertions to const types (see ndarray2ringbuffer())
// Note: something to ndarray should use intermediate convertions to non-const types (see ringbuffer2ndarray())

inline void ndarray2ringbuffer(const nb::ndarray<>& _in, phaseshift::ringbuffer<float>* in) {
    // TODO(GD) Remove extra copy by providing the buffer to the phaseshift::ringbuffer ctor
    const int in_size = static_cast<int>(_in.size());
    in->resize_allocation(in_size);
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 32) {
        in->push_back(static_cast<const float*>(_in.data()), in_size);
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 64) {
        in->push_back(static_cast<const double*>(_in.data()), in_size);
    } else {
        assert(_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float  && "Only float32 or float64 types supported.");
        assert(((_in.dtype().bits == 32) || (_in.dtype().bits == 64)) && "Only float32 or float64 types supported.");
        throw std::runtime_error("Only float32 or float64 types supported.");  // TCE_ALLOW throw TCE_ALLOW_ANY_STRING
    }
}

inline nb::ndarray<nb::numpy, float> ringbuffer2ndarray(const phaseshift::ringbuffer<float>& rb) {
    if (rb.size() == 0) {
        return nb::ndarray<nb::numpy, float>(nullptr, { 0 });
    }
    float* data = new float[rb.size()];
    rb.copy_to_contiguous(data);
    nb::capsule owner(data, [](void* p) noexcept { delete[] (float*)p; });
    return nb::ndarray<nb::numpy, float>(data, { static_cast<size_t>(rb.size()) }, owner);
}

inline void ndarray2vector(const nb::ndarray<>& _in, phaseshift::vector<std::complex<float>>* in) {
    // TODO(GD) Remove extra copy by providing the buffer to the phaseshift::vector ctor
    const int in_size = static_cast<int>(_in.size());
    in->resize_allocation(in_size);
    in->clear();
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Complex && _in.dtype().bits == 64) {
        // TODO(GD) SPEEDUP We can use memcpy
        for (int k=0; k < in_size; ++k) {
            float real = ((float*)(_in.data()))[2*k];
            float imag = ((float*)(_in.data()))[2*k+1];
            std::complex<float> c(real, imag);
            in->push_back(c);
        }
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Complex && _in.dtype().bits == 128) {
        for (int k=0; k < in_size; ++k) {
            float real = static_cast<float>(((double*)(_in.data()))[2*k]);
            float imag = static_cast<float>(((double*)(_in.data()))[2*k+1]);
            in->push_back(std::complex<float>(real, imag));
        }
    } else {
        assert(_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Complex  && "Only complex64 and complex128 types supported.");
        assert(((_in.dtype().bits == 64) || (_in.dtype().bits == 128)) && "Only complex64 and complex128 types supported.");
        throw std::runtime_error("Only complex64 and complex128 types supported.");  // TCE_ALLOW throw TCE_ALLOW_ANY_STRING
    }
}

inline void ndarray2vector(const nb::ndarray<>& _in, phaseshift::vector<float>* in) {
    // TODO(GD) SPEEDUP Remove extra copy by providing the buffer to the phaseshift::vector ctor
    const int in_size = static_cast<int>(_in.size());
    in->resize_allocation(in_size);
    in->clear();
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 32) {
        in->push_back(static_cast<const float*>(_in.data()), in_size);
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 64) {
        in->resize(in_size);
        for (int k=0; k < in_size; ++k) {
            float real = static_cast<float>(((const double*)(_in.data()))[k]);
            in->data()[k] = real;
        }
    } else {
        assert(_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float  && "Only float32 or float64 types supported.");
        assert(((_in.dtype().bits == 32) || (_in.dtype().bits == 64)) && "Only float32 or float64 types supported.");
        throw std::runtime_error("Only float32 or float64 or complex64 types supported.");  // TCE_ALLOW throw TCE_ALLOW_ANY_STRING
    }
}

inline nb::ndarray<nb::numpy, float> vector2ndarray(const phaseshift::vector<float>& vec) {
    if (vec.size() == 0) {
        return nb::ndarray<nb::numpy, float>(nullptr, { 0 });
    }
    float* data = new float[vec.size()];
    std::memcpy(data, vec.data(), sizeof(float) * vec.size());
    nb::capsule owner(data, [](void* p) noexcept { delete[] (float*)p; });
    return nb::ndarray<nb::numpy, float>(data, { static_cast<size_t>(vec.size()) }, owner);
}

// Zero-copy version: transfers ownership of the vector's buffer to numpy.
// WARNING: The vector is left empty after this call.
inline nb::ndarray<nb::numpy, float> vector2ndarray_zerocopy(phaseshift::vector<float>* pvec) {
    phaseshift::vector<float>& vec = *pvec;
    auto [data, size] = vec.release_allocation();
    if (data == nullptr) {
        return nb::ndarray<nb::numpy, float>(nullptr, { 0 });
    }
    // Custom deleter for aligned memory allocated by phaseshift::vector
    nb::capsule owner(data, [](void* p) noexcept {
        phaseshift::allocation::aligned_free(p);
    });
    return nb::ndarray<nb::numpy, float>(data, { static_cast<size_t>(size) }, owner);
}


inline std::map<std::string, std::string> kwargs2options(const nb::kwargs& kwargs) {
    std::map<std::string, std::string> options;
    for (auto kv: kwargs) {
        options.insert_or_assign(nb::str(kv.first).c_str(), nb::str(kv.second).c_str());
    }
    return options;
}

#endif  // PHASESHIFT_PY_NANOBIND_H_
