// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#ifndef PHASESHIFT_CATCH2_EXTRA_H_
#define PHASESHIFT_CATCH2_EXTRA_H_

#include <mutex>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

// A thread safe version of REQUIRE(.) of Catch2
// See: https://github.com/catchorg/Catch2/issues/2935
extern std::mutex g_catch2_extra_mutex;
#define REQUIRE_TS(expr) {std::lock_guard<std::mutex> guard(g_catch2_extra_mutex); REQUIRE(expr);}

#endif  // PHASESHIFT_UTILS_H_
