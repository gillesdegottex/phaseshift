// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>

#include <iostream>

int phaseshift::dev::check_compilation_options() {
    int ret = 0;

    #ifndef NDEBUG
    ret++;
    std::cerr << "WARNING: phaseshift library: C asserts are enabled. Maximum speed is not expected. (PHASESHIFT_DEV_ASSERT=ON)" << std::endl;
    #endif

    #ifdef PHASESHIFT_DEV_ASSERT
    ret++;
    std::cerr << "WARNING: phaseshift library: Removed -DNDEBUG from compilation flags in order to enable C asserts. Should be used for testing only. (PHASESHIFT_DEV_ASSERT=ON)" << std::endl;
    #endif

    #ifdef PHASESHIFT_DEV_PROFILING
    ret++;
    std::cerr << "WARNING: phaseshift library: Profiling is enabled. Extra time might be spent in measuring some function calls (ex. audio_block::proc(.)). (PHASESHIFT_DEV_PROFILING=ON)" << std::endl;
    #endif

    // Supress warnings
    (void)phaseshift::twopi;
    (void)phaseshift::oneover_twopi;
    (void)phaseshift::piovertwo;

    return ret;
}
