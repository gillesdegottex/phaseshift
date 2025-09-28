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
        constexpr float log_coef = 27.0f / 1.8562979903656263f;  // std::log(6.4f)=1.8562979903656263
        constexpr float start = knee * lin;
        return start + log_coef * std::log(freq/knee);
    }
}
inline float mel2hz(float mel) {
    constexpr float knee = 1000.0f;
    constexpr float lin_inv = 200.0f / 3.0f;
    constexpr float start = knee / lin_inv;
    constexpr float log_coef_inv = 1.8562979903656263f / 27.0f;  // std::log(6.4f)=1.8562979903656263

    // If higher than knee, use log scale, otherwise just keep linear scale
    if (mel > start) {
        return knee * std::exp((mel - start) * log_coef_inv);
    } else {
        return mel * lin_inv;
    }
}


// https://en.wikipedia.org/wiki/A-weighting
inline float a_weighting(float f) {
    return (12194.0f*12194.0f) * (f*f*f*f) / ( (f*f + 20.6f*20.6f) * std::sqrt( (f*f + 107.7f*107.7f) * (f*f + 737.9f*737.9f) ) * (f*f + 12194.0f*12194.0f) );
}
inline float b_weighting(float f) {
    return (12194.0f*12194.0f) * (f*f*f) / ( (f*f + 20.6f*20.6f) * std::sqrt( (f*f + 158.5f*158.5f) ) * (f*f + 12194.0*12194.0f) );
}
inline float c_weighting(float f) {
    return (12194.0f*12194.0f) * (f*f) / ( (f*f + 20.6f*20.6f) * (f*f + 12194.0f*12194.0f) );
}
inline float d_weighting(float f) {
    float v1 = (1037918.48f - f*f);
    float v2 = (9837328.0f - f*f);
    float h_f = (v1*v1 + 1080768.16f*f*f) / ( v2*v2 + 11723776.0f*f*f );
    return (f/6.8966888496476f*10e-5) * std::sqrt(h_f / ((f*f + 79919.29f)*(f*f + 1345600.0f)));
}

}  // namespace phaseshift

#endif  // PHASESHIFT_SIGPROC_AUDITORY_H_
