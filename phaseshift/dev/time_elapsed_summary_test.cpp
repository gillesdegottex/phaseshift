// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/dev/time_elapsed_summary.h>

#include <snitch/snitch.hpp>

#include <sstream>

#ifdef PHASESHIFT_DEV_PROFILING
TEST_CASE("time_elapsed_summary_print", "[time_elapsed_summary]") {
    phaseshift::dev::time_elapsed_summary summary;
    acbench::time_elapsed step(8);

    summary.initialize.start();
    summary.initialize.end(0.0f);

    summary.loop.start();
    step.start();
    step.end(1.0f);
    summary.loop.end(1.0f);

    summary.finalize.start();
    summary.finalize.end(0.0f);

    summary.loop_add("step", &step);

    std::ostringstream out;
    summary.print(out);
    REQUIRE(!out.str().empty());
    REQUIRE(out.str().find("Initialize") != std::string::npos);
    REQUIRE(out.str().find("Finalize") != std::string::npos);
}
#else
TEST_CASE("time_elapsed_summary_skipped", "[time_elapsed_summary]") {
    SKIP("PHASESHIFT_DEV_PROFILING is disabled.");
}
#endif
