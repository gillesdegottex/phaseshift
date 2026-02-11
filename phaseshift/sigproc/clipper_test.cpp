// Copyright (C) 2025 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/sigproc/clipper.h>

#include <string>

#include <snitch/snitch.hpp>

TEST_CASE("clipper", "[clipper]") {
    phaseshift::dev::check_compilation_options();

    std::cout << std::endl << "INFO: phaseshift::clipper" << std::endl;
    phaseshift::lookup_table_clipper01 lt;
    std::vector<std::pair<float,float>> xys;
    phaseshift::lookup_table::generate_range(lt, &xys);

    REQUIRE( xys.size() > 0 );
    for (int n=0; n < xys.size(); ++n) {
        auto xy = xys[n];

        if (n > 0) {
            REQUIRE( xys[n].first > xys[n-1].first );
            REQUIRE( xys[n].second >= xys[n-1].second );
        }
    }
}
