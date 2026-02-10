// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/lookup_table.h>

phaseshift::lookup_table::lookup_table() {
}

phaseshift::lookup_table::~lookup_table() {
    delete[] m_values;
    m_values = nullptr;
}

// Inheriting classes -----------------------------------------------------------

phaseshift::lookup_table_lin012db::lookup_table_lin012db() {
    phaseshift::lookup_table::initialize(this, phaseshift::float32::eps(), 1.0f, 300*4);
}

phaseshift::lookup_table_db2lin01::lookup_table_db2lin01() {
    phaseshift::lookup_table::initialize(this, -300.0f, 0.0f, 300*4);
}

phaseshift::lookup_table_cos::lookup_table_cos() {
    phaseshift::lookup_table::initialize(this, 0.0f, static_cast<float>(phaseshift::twopi), 1000);  // was 50
}

phaseshift::lookup_table_sin::lookup_table_sin() {
    phaseshift::lookup_table::initialize(this, 0.0f, static_cast<float>(phaseshift::twopi), 1000);  // was 50
}
