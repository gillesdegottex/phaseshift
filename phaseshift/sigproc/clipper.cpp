// Copyright (C) 2025 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include "./clipper.h"
#include "./clipper_table.h"  // Include only once, here, and no where else.

phaseshift::lookup_table_clipper01::lookup_table_clipper01() {

    // Don't use the lookup_table::initialize(.)
    // Load the values from clipper_table.h file instead.

    m_xmin = g_clipper_table_xmin;
    m_xmax = g_clipper_table_xmax;
    m_step = g_clipper_table_step;
    m_size = g_clipper_table_size;

    m_x2i = (m_size-1) / (m_xmax - m_xmin);

    m_values = new float[m_size];
    std::memcpy(m_values, g_clipper_table, m_size*sizeof(float));
}
