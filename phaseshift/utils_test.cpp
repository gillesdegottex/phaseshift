// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>

#include <snitch/snitch.hpp>

TEST_CASE("utils", "[compilation]") {
    phaseshift::dev::check_compilation_options();
}

TEST_CASE("utils", "[global_cursor]") {

    long double cursor_max = std::numeric_limits<phaseshift::globalcursor_t>::max();
    cursor_max /= 4*96000; // in seconds with some extreme sampling rate
    cursor_max /= 60;      // in minutes
    cursor_max /= 60;      // in hours
    cursor_max /= 24;      // in days
    cursor_max /= 365;     // in years

    std::cout << "INFO: Global cursor duration: " << cursor_max << " years" << std::endl;

    REQUIRE(cursor_max > 1e5); // be sure it last for a 100'000 years, at least... who knows...
}

TEST_CASE("utils", "[sigproc]") {
    REQUIRE(std::abs(phaseshift::db2lin(phaseshift::lin2db(0.5f))-0.5f) < 1e-7);
    REQUIRE(std::abs(phaseshift::db2lin(phaseshift::lin2db(0.5))-0.5) < 1e-7);

    REQUIRE(std::abs(phaseshift::lin2db(phaseshift::db2lin(-12.34f))+12.34f) < 1e-6);
    REQUIRE(std::abs(phaseshift::lin2db(phaseshift::db2lin(-12.34))+12.34) == 0.0);

    REQUIRE(phaseshift::nextpow2(16) == 16);
    REQUIRE(phaseshift::nextpow2(347) == 512);
    REQUIRE(phaseshift::nextpow2(511) == 512);
    REQUIRE(phaseshift::nextpow2(512) == 512);
    REQUIRE(phaseshift::nextpow2(513) == 1024);
}

TEST_CASE("utils", "[logger]") {
    DLINE
    DOUT << "Logging: " << 3.1415 << std::endl;
}
