// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_SIGPROC_AUDITORY_H_
#define PHASESHIFT_SIGPROC_AUDITORY_H_

#include <phaseshift/utils.h>
#include <phaseshift/lookup_table.h>

#include <algorithm>
#include <complex>

namespace phaseshift {

// https://en.wikipedia.org/wiki/Mel_scale according to Umesh and Slaney
inline float hz2mel(float freq) {
    constexpr float knee = 1000.0f;
    constexpr float lin = 3.0f / 200.0f;
    if (freq < knee) {
        return lin * freq;
    } else {
        constexpr float log_coef = 27.0f / std::log(6.4f);
        constexpr float start = knee * lin;
        return start + log_coef * std::log(freq/knee);
    }
}
inline float mel2hz(float mel) {
    constexpr float knee = 1000.0f;
    constexpr float lin_inv = 200.0f / 3.0f;
    constexpr float start = knee / lin_inv;
    constexpr float log_coef_inv = std::log(6.4f) / 27.0f;

    // If higher than knee, use log scale, otherwise just keep linear scale
    if (mel > start) {
        return knee * std::exp((mel - start) * log_coef_inv);
    } else {
        return mel * lin_inv;
    }
}

}  // namespace phaseshift

#endif  // PHASESHIFT_SIGPROC_AUDITORY_H_
