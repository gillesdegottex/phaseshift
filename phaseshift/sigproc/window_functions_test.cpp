// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/sigproc/window_functions.h>
#include <phaseshift/containers/vector.h>

#include <fftscarf.h>

#include <snitch/snitch.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
    float sum_values(const phaseshift::vector<float>& values) {
        float sum = 0.0f;
        for (int n = 0; n < values.size(); ++n) {
            sum += values[n];
        }
        return sum;
    }

    void require_symmetric(const phaseshift::vector<float>& values, float tol = 1e-6f) {
        for (int n = 0; n < values.size(); ++n) {
            REQUIRE(std::abs(values[n] - values[values.size() - 1 - n]) < tol);
        }
    }
}

TEST_CASE("window_hann_properties", "[window_functions]") {
    phaseshift::vector<float> win;
    win.resize_allocation(64);
    phaseshift::win_hann(&win, 64, true);

    REQUIRE(std::abs(sum_values(win) - 1.0f) < 1e-5f);
    require_symmetric(win);
    REQUIRE(std::abs(win[0]) < 1e-6f);
    REQUIRE(win[win.size() / 2] > win[1]);
}

TEST_CASE("window_hamming_properties", "[window_functions]") {
    phaseshift::vector<float> win;
    win.resize_allocation(64);
    phaseshift::win_hamming(&win, 64, true);

    REQUIRE(std::abs(sum_values(win) - 1.0f) < 1e-5f);
    require_symmetric(win);
    REQUIRE(win[0] > 0.0f);
    REQUIRE(win[win.size() / 2] > win[0]);
}

TEST_CASE("window_blackman_properties", "[window_functions]") {
    phaseshift::vector<float> win;
    win.resize_allocation(64);
    phaseshift::win_blackman(&win, 64, true);

    REQUIRE(std::abs(sum_values(win) - 1.0f) < 1e-5f);
    require_symmetric(win);
    REQUIRE(std::abs(win[0]) < 1e-6f);
    REQUIRE(win[win.size() / 2] > win[1]);
}

TEST_CASE("window_gaussian_properties", "[window_functions]") {
    phaseshift::vector<float> win;
    win.resize_allocation(63);
    phaseshift::win_gaussian(&win, 63, true, 0.5f);

    REQUIRE(std::abs(sum_values(win) - 1.0f) < 1e-5f);
    require_symmetric(win);

    float max_value = 0.0f;
    int max_index = 0;
    for (int n = 0; n < win.size(); ++n) {
        if (win[n] > max_value) {
            max_value = win[n];
            max_index = n;
        }
    }
    REQUIRE(max_index == win.size() / 2);
}

TEST_CASE("window_bandwidth_6db_monotonic", "[window_functions]") {
    const float fs = 48000.0f;
    phaseshift::vector<float> win_small;
    phaseshift::vector<float> win_large;
    phaseshift::vector<std::complex<float>> win_rfft;

    win_small.resize_allocation(32);
    win_small.resize(32);
    win_large.resize_allocation(128);
    win_large.resize(128);
    for (int n = 0; n < win_small.size(); ++n) {
        win_small[n] = 1.0f;
    }
    for (int n = 0; n < win_large.size(); ++n) {
        win_large[n] = 1.0f;
    }

    int dftlen_small = phaseshift::nextpow2(win_small.size()) * 16;
    int dftlen_large = phaseshift::nextpow2(win_large.size()) * 16;
    fftscarf::planmanagerf().prepare(std::max(dftlen_small, dftlen_large));
    win_rfft.resize_allocation(std::max(dftlen_small, dftlen_large) / 2 + 1);

    float bw_small = phaseshift::window_bandwidth_6db(win_small, fs, &win_rfft, 16);
    float bw_large = phaseshift::window_bandwidth_6db(win_large, fs, &win_rfft, 16);

    REQUIRE(bw_small > 0.0f);
    REQUIRE(bw_large > 0.0f);
    REQUIRE(bw_small > bw_large);
}
