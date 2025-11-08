// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/ola.h>
#include <phaseshift/dev/catch2_extra.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("audio_block_ola_builder_test", "[audio_block_ola_builder_test]") {
    phaseshift::dev::check_compilation_options();

    phaseshift::dev::audio_block_ola_builder_test();
}
