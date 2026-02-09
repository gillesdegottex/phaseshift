// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_CONTAINERS_UTILS_H_
#define PHASESHIFT_CONTAINERS_UTILS_H_

#include <cstdint>
#include <phaseshift/utils.h>

#include <cstring>
#include <string>
#include <vector>
#include <deque>

#define assert_nan_inf_array(array) {                   \
        for (int n=0; n < int(array.size()); ++n) {     \
            assert(!std::isnan(array[n]));              \
            assert(!std::isinf(array[n]));              \
        }                                               \
    }

#define assert_nan_inf_array_complex(array) {           \
        for (int n=0; n < int(array.size()); ++n) {     \
            assert(!std::isnan(array[n].real()));       \
            assert(!std::isnan(array[n].imag()));       \
            assert(!std::isinf(array[n].real()));       \
            assert(!std::isinf(array[n].imag()));       \
        }                                               \
    }

#define assert_nan_inf_array(array) {                   \
        for (int n=0; n < int(array.size()); ++n) {     \
            assert(!std::isnan(array[n]));              \
            assert(!std::isinf(array[n]));              \
        }                                               \
    }

#define assert_nozero_array_complex(array) {            \
        for (int n=0; n < int(array.size()); ++n) {     \
            assert(array[n].real() != 0);               \
            assert(array[n].imag() != 0);               \
        }                                               \
    }

#define assert_nozero_array(array) {                    \
        for (int n=0; n < int(array.size()); ++n) {     \
            assert(array[n] != 0);                      \
        }                                               \
    }

namespace phaseshift {

    namespace dev {

        // must mirror python numpy functions: np.fromfile(filepath, dtype=dtype)
        // TODO(GD) Move to acbench?

        template<typename array_type>
        inline void binaryfile_write_generic_int32(const std::string& filepath, const array_type& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            assert(filepath.size() > 0);

            std::ofstream outfile(filepath, mode);
            assert(outfile.is_open());

            for (int n = 0; n < int(array.size()); ++n) {
                int32_t value = static_cast<int32_t>(array[n]);
                outfile.write(reinterpret_cast<char*>(&value), sizeof(int32_t));
            }
            outfile.close();
        }

        template<typename array_type>
        inline void binaryfile_write_generic_float32(const std::string& filepath, const array_type& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            assert(filepath.size() > 0);

            std::ofstream outfile(filepath, mode);
            assert(outfile.is_open());

            for (int n = 0; n < int(array.size()); ++n) {
                float value = static_cast<float>(array[n]);
                outfile.write(reinterpret_cast<char*>(&value), sizeof(float));
            }
            outfile.close();
        }

        template<typename array_type>
        inline void binaryfile_write_generic_complex64(const std::string& filepath, const array_type& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            assert(filepath.size() > 0);

            std::ofstream outfile(filepath, mode);
            assert(outfile.is_open());

            for (int n = 0; n < int(array.size()); ++n) {
                float value_real = static_cast<float>(array[n].real());
                outfile.write(reinterpret_cast<char*>(&value_real), sizeof(float));
                float value_imag = static_cast<float>(array[n].imag());
                outfile.write(reinterpret_cast<char*>(&value_imag), sizeof(float));
            }
            outfile.close();
        }

        // std::deque
        inline void binaryfile_write(const std::string& filepath, const std::deque<float>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_float32(filepath, array, mode);
        }

        // std::vector
        inline void binaryfile_write(const std::string& filepath, const std::vector<int>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_int32(filepath, array, mode);
        }
        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const std::vector<std::complex<value_type>>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_complex64(filepath, array, mode);
        }
        template<typename value_type>
        inline void binaryfile_write(const std::string& filepath, const std::vector<value_type>& array, std::ios_base::openmode mode = std::ios::out | std::ios::binary) {
            phaseshift::dev::binaryfile_write_generic_float32(filepath, array, mode);
        }

    } // namespace dev

}  // namespace phaseshift

#endif  // PHASESHIFT_CONTAINERS_UTILS_H_
