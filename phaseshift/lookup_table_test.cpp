// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/sigproc/interpolation.h>

#include <string>

#include <snitch/snitch.hpp>

void require_common(const phaseshift::lookup_table::validation_stats& stats) {
    REQUIRE(!std::isnan(stats.abserr_mean));
    REQUIRE(!std::isinf(stats.abserr_mean));
    REQUIRE(!std::isnan(stats.abserr_max));
    REQUIRE(!std::isinf(stats.abserr_max));

    REQUIRE(!std::isnan(stats.relerr_mean));
    REQUIRE(!std::isinf(stats.relerr_mean));
    REQUIRE(!std::isnan(stats.relerr_max));
    REQUIRE(!std::isinf(stats.relerr_max));

    REQUIRE(!std::isnan(stats.rangerelerr_mean));
    REQUIRE(!std::isinf(stats.rangerelerr_mean));
    REQUIRE(!std::isnan(stats.rangerelerr_max));
    REQUIRE(!std::isinf(stats.rangerelerr_max));
}

TEST_CASE("cos", "[lookup_table]") {
    phaseshift::dev::check_compilation_options();

    std::cout << std::endl << "INFO: phaseshift::cos_ltf" << std::endl;
    phaseshift::lookup_table_cos cos_lt;
    phaseshift::lookup_table::validation_stats stats = phaseshift::lookup_table::test_validation(cos_lt);
    require_common(stats);
    REQUIRE(stats.abserr_mean < 0.002);
    REQUIRE(stats.abserr_max < 0.005);
    REQUIRE(stats.rangerelerr_mean < 0.001);
    REQUIRE(stats.rangerelerr_max < 0.002);
}
TEST_CASE("sin", "[lookup_table]") {
    phaseshift::dev::check_compilation_options();

    std::cout << std::endl << "INFO: phaseshift::sin_ltf" << std::endl;
    phaseshift::lookup_table_sin sin_lt;
    phaseshift::lookup_table::validation_stats stats = phaseshift::lookup_table::test_validation(sin_lt);
    require_common(stats);
    REQUIRE(stats.abserr_mean < 0.002);
    REQUIRE(stats.abserr_max < 0.005);
    REQUIRE(stats.rangerelerr_mean < 0.001);
    REQUIRE(stats.rangerelerr_max < 0.002);
}

TEST_CASE("lin012db", "[lookup_table]") {
    phaseshift::dev::check_compilation_options();

    std::cout << std::endl << "INFO: phaseshift::lin012db_ltf" << std::endl;
    phaseshift::lookup_table_lin012db lin012db_lt;
    phaseshift::lookup_table::validation_stats stats = phaseshift::lookup_table::test_validation(lin012db_lt, 4, 0.0);
    require_common(stats);
    REQUIRE(stats.abserr_mean < 0.03);
    // This is bad precision, but a linear mapping can't catchup with an exponential increase anyway. As long as high dB values are precise enough, it is not a huge problem.
    REQUIRE(stats.abserr_max < 50.0f);
    REQUIRE(stats.rangerelerr_mean < 2e-4); 
    REQUIRE(stats.rangerelerr_max < 0.5);
}

TEST_CASE("db2lin01", "[lookup_table]") {
    phaseshift::dev::check_compilation_options();

    std::cout << std::endl << "INFO: phaseshift::db2lin01_ltf" << std::endl;
    phaseshift::lookup_table_db2lin01 db2lin01_lt;
    phaseshift::lookup_table::validation_stats stats = phaseshift::lookup_table::test_validation(db2lin01_lt);
    require_common(stats);
    REQUIRE(stats.abserr_mean < 1e-5);
    REQUIRE(stats.abserr_max < 1e-4);
    REQUIRE(stats.rangerelerr_mean < 1e-5);
    REQUIRE(stats.rangerelerr_max < 1e-4);
}
