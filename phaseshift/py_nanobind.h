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

#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/complex.h>
#include <nanobind/ndarray.h>
namespace nb = nanobind;

#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/containers/vector.h>

inline void ndarray2ringbuffer(const nb::ndarray<>& _in, phaseshift::ringbuffer<float>* in) {
    // TODO(GD) Remove extra copy by providing the buffer to the phaseshift::ringbuffer ctor
    in->resize_allocation(_in.size());
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 32) {
        in->push_back(static_cast<const float*>(_in.data()), _in.size());
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 64) {
        in->push_back(static_cast<const double*>(_in.data()), _in.size());
    } else {
        assert(_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float  && "Only float32 or float64 types supported.");
        assert(((_in.dtype().bits == 32) || (_in.dtype().bits == 64)) && "Only float32 or float64 types supported.");
        throw std::runtime_error("Only float32 or float64 types supported.");  // TCE_ALLOW throw TCE_ALLOW_ANY_STRING
    }
}

inline nb::ndarray<nb::numpy, float> ringbuffer2ndarray(const phaseshift::ringbuffer<float>& rb) {
    // TODO(GD) Well... I hope there is something simpler... ask on github
    // TODO(GD) 1) Steal the buffer of the output ringbuffer?
    float* data = new float[rb.size()];
    memcpy(data, rb.data(), sizeof(float)*rb.size());
    // Delete 'data' when the 'owner' capsule expires
    // https://nanobind.readthedocs.io/en/latest/ndarray.html
    nb::capsule numpy_array_owner(data, [](void *p) noexcept {delete[] (float *) p;});
    size_t shape[1] = { static_cast<size_t>(rb.size()) };
    return nb::ndarray<nb::numpy, float>(data, 1, shape, numpy_array_owner);
}

inline void ndarray2vector(const nb::ndarray<>& _in, phaseshift::vector<std::complex<float>>* in) {
    // TODO(GD) Remove extra copy by providing the buffer to the phaseshift::vector ctor
    in->resize_allocation(_in.size());
    in->clear();
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Complex && _in.dtype().bits == 64) {
        // TODO(GD) SPEEDUP We can use memcpy
        for (int k=0; k < int(_in.size()); ++k) {
            float real = ((float*)(_in.data()))[2*k];
            float imag = ((float*)(_in.data()))[2*k+1];
            std::complex<float> c(real, imag);
            in->push_back(c);
        }
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Complex && _in.dtype().bits == 128) {
        for (int k=0; k < int(_in.size()); ++k) {
            float real = ((double*)(_in.data()))[2*k];
            float imag = ((double*)(_in.data()))[2*k+1];
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
    in->resize_allocation(_in.size());
    in->clear();
    if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 32) {
        in->push_back(static_cast<const float*>(_in.data()), _in.size());
    } else if (_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float && _in.dtype().bits == 64) {
        in->resize(_in.size());
        for (int k=0; k < int(_in.size()); ++k) {
            float real = ((double*)(_in.data()))[k];
            in->data()[k] = real;
        }
    } else {
        assert(_in.dtype().code == (uint8_t)nb::dlpack::dtype_code::Float  && "Only float32 or float64 types supported.");
        assert(((_in.dtype().bits == 32) || (_in.dtype().bits == 64)) && "Only float32 or float64 types supported.");
        throw std::runtime_error("Only float32 or float64 or complex64 types supported.");  // TCE_ALLOW throw TCE_ALLOW_ANY_STRING
    }
}

inline nb::ndarray<nb::numpy, const float> vector2ndarray(const phaseshift::vector<float>& rb) {
    // TODO(GD) Well... I hope there is something simpler... ask on github
    // TODO(GD) 1) Steal the buffer of the output ringbuffer
    float* data = new float[rb.size()];
    memcpy(data, rb.data(), sizeof(float)*rb.size());
    // Delete 'data' when the 'owner' capsule expires
    // https://nanobind.readthedocs.io/en/latest/ndarray.html
    nb::capsule numpy_array_owner(data, [](void *p) noexcept {delete[] (float *) p;});
    size_t shape[1] = { static_cast<size_t>(rb.size()) };
    return nb::ndarray<nb::numpy, const float>(data, 1, shape, numpy_array_owner);
}


inline std::map<std::string, std::string> kwargs2options(const nb::kwargs& kwargs) {
    std::map<std::string, std::string> options;
    for (auto kv: kwargs) {
        options.insert_or_assign(nb::str(kv.first).c_str(), nb::str(kv.second).c_str());
    }
    return options;
}

#endif  // PHASESHIFT_PY_NANOBIND_H_
