// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/sigproc/auditory.h>

#include <snitch/snitch.hpp>

#include <cmath>

TEST_CASE("auditory_mel_roundtrip", "[auditory]") {
    float mel_1k = phaseshift::hz2mel(1000.0f);
    REQUIRE(std::abs(mel_1k - 15.0f) < 1e-6f);
    REQUIRE(std::abs(phaseshift::mel2hz(mel_1k) - 1000.0f) < 1e-3f);

    float mel_2k = phaseshift::hz2mel(2000.0f);
    float hz_2k = phaseshift::mel2hz(mel_2k);
    REQUIRE(std::abs(hz_2k - 2000.0f) < 1e-3f);

    REQUIRE(phaseshift::hz2mel(500.0f) < phaseshift::hz2mel(1000.0f));
    REQUIRE(phaseshift::hz2mel(1000.0f) < phaseshift::hz2mel(2000.0f));
}

TEST_CASE("auditory_weighting_finite", "[auditory]") {
    float freqs[] = {1000.0f, 10000.0f};
    for (float f : freqs) {
        float aw = phaseshift::a_weighting(f);
        float bw = phaseshift::b_weighting(f);
        float cw = phaseshift::c_weighting(f);
        float dw = phaseshift::d_weighting(f);
        REQUIRE(std::isfinite(aw));
        REQUIRE(std::isfinite(bw));
        REQUIRE(std::isfinite(cw));
        REQUIRE(std::isfinite(dw));
        REQUIRE(aw > 0.0f);
        REQUIRE(bw > 0.0f);
        REQUIRE(cw > 0.0f);
        REQUIRE(dw > 0.0f);
    }
}
