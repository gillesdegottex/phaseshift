// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/sigproc/sigproc.h>
#include <phaseshift/containers/vector.h>

#include <snitch/snitch.hpp>

#include <cmath>
#include <complex>
#include <deque>
#include <vector>

TEST_CASE("sigproc_basic_stats", "[sigproc]") {
    std::vector<float> values = {3.0f, 1.0f, 4.0f};
    REQUIRE(phaseshift::argmin<float>(values) == 1);
    REQUIRE(phaseshift::argmax<float>(values) == 2);
    REQUIRE(phaseshift::min<float>(values) == 1.0f);
    REQUIRE(phaseshift::max<float>(values) == 4.0f);

    std::vector<float> sum_values = {0.0f, 2.0f, 3.0f};
    REQUIRE(phaseshift::sum<float>(sum_values) == 5.0f);

    std::vector<float> prod_values = {1.0f, 2.0f, 3.0f};
    REQUIRE(phaseshift::prod<float>(prod_values) == 6.0f);

    std::vector<float> mean_values = {1.0f, 2.0f, 3.0f};
    REQUIRE(std::abs(phaseshift::mean<float>(mean_values) - 2.0f) < 1e-6f);

    std::deque<float> mean_deque = {1.0f, 3.0f};
    REQUIRE(std::abs(phaseshift::mean<float>(mean_deque) - 2.0f) < 1e-6f);

    std::vector<float> std_values = {2.0f, 2.0f, 2.0f};
    REQUIRE(std::abs(phaseshift::std<float>(std_values)) < 1e-6f);

    phaseshift::vector<float> norm_values;
    norm_values.resize_allocation(2);
    norm_values.resize(2);
    norm_values[0] = 3.0f;
    norm_values[1] = 4.0f;
    REQUIRE(std::abs(phaseshift::norm<float>(norm_values) - 25.0f) < 1e-6f);
}

TEST_CASE("sigproc_sigmoid", "[sigproc]") {
    phaseshift::vector<float> values;
    values.resize_allocation(9);
    values.resize(9);
    phaseshift::sigmoid(&values, 4.0f, 1.0f);

    REQUIRE(std::abs(values[4] - 0.5f) < 1e-6f);
    for (int k = 0; k < static_cast<int>(values.size()) - 1; ++k) {
        REQUIRE(values[k] <= values[k + 1]);
    }
}

TEST_CASE("sigproc_shift_half_size", "[sigproc]") {
    std::vector<int> values = {0, 1, 2, 3};
    phaseshift::shift_half_size(&values);
    REQUIRE(values[0] == 2);
    REQUIRE(values[1] == 3);
    REQUIRE(values[2] == 0);
    REQUIRE(values[3] == 1);
}

TEST_CASE("sigproc_timeshift_sig", "[sigproc]") {
    phaseshift::vector<float> values;
    values.resize_allocation(6);
    values.resize(6);
    for (int n = 0; n < values.size(); ++n) {
        values[n] = static_cast<float>(n + 1);
    }

    phaseshift::timeshift_sig(&values, 2);
    REQUIRE(values[0] == 0.0f);
    REQUIRE(values[1] == 0.0f);
    REQUIRE(values[2] == 1.0f);
    REQUIRE(values[3] == 2.0f);
    REQUIRE(values[4] == 3.0f);
    REQUIRE(values[5] == 4.0f);

    phaseshift::vector<float> values_neg;
    values_neg.resize_allocation(6);
    values_neg.resize(6);
    for (int n = 0; n < values_neg.size(); ++n) {
        values_neg[n] = static_cast<float>(n + 1);
    }
    phaseshift::timeshift_sig(&values_neg, -2);
    REQUIRE(values_neg[0] == 3.0f);
    REQUIRE(values_neg[1] == 4.0f);
    REQUIRE(values_neg[2] == 5.0f);
    REQUIRE(values_neg[3] == 6.0f);
    REQUIRE(values_neg[4] == 0.0f);
    REQUIRE(values_neg[5] == 0.0f);
}

TEST_CASE("sigproc_timeshift_hspec", "[sigproc]") {
    const int dftlen = 8;
    const int hsize = dftlen / 2 + 1;
    phaseshift::vector<std::complex<float>> spec;
    spec.resize_allocation(hsize);
    spec.resize(hsize);
    for (int k = 0; k < hsize; ++k) {
        spec[k] = std::complex<float>(0.0f, 0.0f);
    }
    spec[0] = std::complex<float>(1.0f, 0.0f);
    spec[1] = std::complex<float>(1.0f, 0.0f);

    phaseshift::timeshift_hspec(&spec, 0.0f);
    REQUIRE(std::abs(spec[0].real() - 1.0f) < 1e-6f);
    REQUIRE(std::abs(spec[0].imag()) < 1e-6f);
    REQUIRE(std::abs(std::abs(spec[1]) - 1.0f) < 1e-6f);

    spec[0] = std::complex<float>(1.0f, 0.0f);
    spec[1] = std::complex<float>(1.0f, 0.0f);
    phaseshift::timeshift_hspec(&spec, 1.0f);
    REQUIRE(std::abs(spec[0].real() - 1.0f) < 1e-4f);
    REQUIRE(std::abs(spec[0].imag()) < 1e-4f);
    REQUIRE(std::abs(std::abs(spec[1]) - 1.0f) < 1e-3f);
}

TEST_CASE("sigproc_lowpass_hspec", "[sigproc]") {
    const int dftlen = 16;
    const int hsize = dftlen / 2 + 1;
    phaseshift::vector<std::complex<float>> spec;
    spec.resize_allocation(hsize);
    spec.resize(hsize);
    for (int k = 0; k < hsize; ++k) {
        spec[k] = std::complex<float>(1.0f, 0.0f);
    }

    phaseshift::lowpass_hspec(&spec, 48000.0f, 2, -1.0f);
    REQUIRE(std::abs(std::abs(spec[2]) - 1.0f) < 1e-5f);
    REQUIRE(std::abs(spec[3]) < std::abs(spec[2]));
}
