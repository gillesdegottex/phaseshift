// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/containers/ringbuffer.h>
#include <phaseshift/containers/vector.h>

#include <snitch/snitch.hpp>

#include <cmath>

static bool approx_equal(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

namespace Catch {
    struct Approx {
        float value;
        float epsilon;

        explicit Approx(float v) : value(v), epsilon(1e-5f) {
        }
    };

    inline bool operator==(float lhs, const Approx& rhs) {
        return approx_equal(lhs, rhs.value, rhs.epsilon);
    }

    inline bool operator==(const Approx& lhs, float rhs) {
        return approx_equal(rhs, lhs.value, lhs.epsilon);
    }
}

// Helper to create a ringbuffer in a wrapped state
// Fills rb so m_front is at wrap_offset from the start
static void create_wrapped_ringbuffer(phaseshift::ringbuffer<float>& rb, int capacity, int size, int wrap_offset, float start_value = 0.0f) {
    rb.resize_allocation(capacity);
    rb.clear();

    // Push and pop to move the front pointer
    for (int i = 0; i < wrap_offset; ++i) {
        rb.push_back(0.0f);
    }
    rb.pop_front(wrap_offset);

    // Now push the actual data
    for (int i = 0; i < size; ++i) {
        rb.push_back(start_value + static_cast<float>(i));
    }
}

// Helper to create a vector with sequential values
static void create_vector(phaseshift::vector<float>& v, int size, float start_value = 0.0f) {
    v.resize_allocation(size);
    v.clear();
    for (int i = 0; i < size; ++i) {
        v.push_back(start_value + static_cast<float>(i));
    }
}

// =============================================================================
// Basic Ringbuffer Operations
// =============================================================================

TEST_CASE("ringbuffer_basic_operations", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    SECTION("empty ringbuffer") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);

        REQUIRE(rb.size() == 0);
        REQUIRE(rb.size_max() == 10);
    }

    SECTION("push_back and size") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);

        rb.push_back(1.0f);
        REQUIRE(rb.size() == 1);
        REQUIRE(rb[0] == 1.0f);

        rb.push_back(2.0f);
        rb.push_back(3.0f);
        REQUIRE(rb.size() == 3);
        REQUIRE(rb[0] == 1.0f);
        REQUIRE(rb[1] == 2.0f);
        REQUIRE(rb[2] == 3.0f);
    }

    SECTION("pop_front") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);

        rb.push_back(1.0f);
        rb.push_back(2.0f);
        rb.push_back(3.0f);

        rb.pop_front(1);
        REQUIRE(rb.size() == 2);
        REQUIRE(rb[0] == 2.0f);
        REQUIRE(rb[1] == 3.0f);
    }

    SECTION("clear") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);

        rb.push_back(1.0f);
        rb.push_back(2.0f);
        rb.clear();

        REQUIRE(rb.size() == 0);
        REQUIRE(rb.size_max() == 10);
    }

    SECTION("wrap around") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(5);

        // Fill partially
        rb.push_back(1.0f);
        rb.push_back(2.0f);
        rb.push_back(3.0f);

        // Pop some to move front pointer
        rb.pop_front(2);

        // Push more to wrap around
        rb.push_back(4.0f);
        rb.push_back(5.0f);
        rb.push_back(6.0f);
        rb.push_back(7.0f);

        REQUIRE(rb.size() == 5);
        REQUIRE(rb[0] == 3.0f);
        REQUIRE(rb[1] == 4.0f);
        REQUIRE(rb[2] == 5.0f);
        REQUIRE(rb[3] == 6.0f);
        REQUIRE(rb[4] == 7.0f);
    }
}

// =============================================================================
// Scalar Operators
// =============================================================================

TEST_CASE("ringbuffer_scalar_operators", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    SECTION("operator+= scalar") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(5);
        rb.push_back(1.0f);
        rb.push_back(2.0f);
        rb.push_back(3.0f);

        rb += 10.0f;

        REQUIRE(rb[0] == Catch::Approx(11.0f));
        REQUIRE(rb[1] == Catch::Approx(12.0f));
        REQUIRE(rb[2] == Catch::Approx(13.0f));
    }

    SECTION("operator-= scalar") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(5);
        rb.push_back(10.0f);
        rb.push_back(20.0f);
        rb.push_back(30.0f);

        rb -= 5.0f;

        REQUIRE(rb[0] == Catch::Approx(5.0f));
        REQUIRE(rb[1] == Catch::Approx(15.0f));
        REQUIRE(rb[2] == Catch::Approx(25.0f));
    }

    SECTION("operator*= scalar") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(5);
        rb.push_back(1.0f);
        rb.push_back(2.0f);
        rb.push_back(3.0f);

        rb *= 2.0f;

        REQUIRE(rb[0] == Catch::Approx(2.0f));
        REQUIRE(rb[1] == Catch::Approx(4.0f));
        REQUIRE(rb[2] == Catch::Approx(6.0f));
    }

    SECTION("operator/= scalar") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(5);
        rb.push_back(10.0f);
        rb.push_back(20.0f);
        rb.push_back(30.0f);

        rb /= 2.0f;

        REQUIRE(rb[0] == Catch::Approx(5.0f));
        REQUIRE(rb[1] == Catch::Approx(10.0f));
        REQUIRE(rb[2] == Catch::Approx(15.0f));
    }
}

// =============================================================================
// Vector Operators - Continuous (no wrap)
// =============================================================================

TEST_CASE("ringbuffer_vector_operators_continuous", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int size = 5;

    SECTION("operator+= vector - continuous") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);
        for (int i = 0; i < size; ++i) rb.push_back(static_cast<float>(i));  // 0,1,2,3,4

        phaseshift::vector<float> v;
        create_vector(v, size, 10.0f);  // 10,11,12,13,14

        rb += v;

        REQUIRE(rb[0] == Catch::Approx(10.0f));
        REQUIRE(rb[1] == Catch::Approx(12.0f));
        REQUIRE(rb[2] == Catch::Approx(14.0f));
        REQUIRE(rb[3] == Catch::Approx(16.0f));
        REQUIRE(rb[4] == Catch::Approx(18.0f));
    }

    SECTION("operator-= vector - continuous") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);
        for (int i = 0; i < size; ++i) rb.push_back(static_cast<float>(i) + 20.0f);  // 20,21,22,23,24

        phaseshift::vector<float> v;
        create_vector(v, size, 10.0f);  // 10,11,12,13,14

        rb -= v;

        REQUIRE(rb[0] == Catch::Approx(10.0f));
        REQUIRE(rb[1] == Catch::Approx(10.0f));
        REQUIRE(rb[2] == Catch::Approx(10.0f));
        REQUIRE(rb[3] == Catch::Approx(10.0f));
        REQUIRE(rb[4] == Catch::Approx(10.0f));
    }

    SECTION("operator*= vector - continuous") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);
        for (int i = 0; i < size; ++i) rb.push_back(static_cast<float>(i) + 1.0f);  // 1,2,3,4,5

        phaseshift::vector<float> v;
        create_vector(v, size, 2.0f);  // 2,3,4,5,6

        rb *= v;

        REQUIRE(rb[0] == Catch::Approx(2.0f));
        REQUIRE(rb[1] == Catch::Approx(6.0f));
        REQUIRE(rb[2] == Catch::Approx(12.0f));
        REQUIRE(rb[3] == Catch::Approx(20.0f));
        REQUIRE(rb[4] == Catch::Approx(30.0f));
    }

    SECTION("operator/= vector - continuous") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);
        for (int i = 0; i < size; ++i) rb.push_back(static_cast<float>((i+1) * 10));  // 10,20,30,40,50

        phaseshift::vector<float> v;
        v.resize_allocation(size);
        for (int i = 0; i < size; ++i) v.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5

        rb /= v;

        REQUIRE(rb[0] == Catch::Approx(10.0f));
        REQUIRE(rb[1] == Catch::Approx(10.0f));
        REQUIRE(rb[2] == Catch::Approx(10.0f));
        REQUIRE(rb[3] == Catch::Approx(10.0f));
        REQUIRE(rb[4] == Catch::Approx(10.0f));
    }
}

// =============================================================================
// Vector Operators - Wrapped
// =============================================================================

TEST_CASE("ringbuffer_vector_operators_wrapped", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;
    const int size = 6;
    const int wrap_offset = 7;  // Forces wrap-around

    SECTION("operator+= vector - wrapped") {
        phaseshift::ringbuffer<float> rb;
        create_wrapped_ringbuffer(rb, capacity, size, wrap_offset, 0.0f);  // 0,1,2,3,4,5
        phaseshift::vector<float> v;
        create_vector(v, size, 10.0f);  // 10,11,12,13,14,15

        rb += v;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb[i] == Catch::Approx(static_cast<float>(i) + 10.0f + static_cast<float>(i)));
        }
    }

    SECTION("operator-= vector - wrapped") {
        phaseshift::ringbuffer<float> rb;
        create_wrapped_ringbuffer(rb, capacity, size, wrap_offset, 20.0f);  // 20,21,22,23,24,25
        phaseshift::vector<float> v;
        create_vector(v, size, 10.0f);  // 10,11,12,13,14,15

        rb -= v;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= vector - wrapped") {
        phaseshift::ringbuffer<float> rb;
        create_wrapped_ringbuffer(rb, capacity, size, wrap_offset, 1.0f);  // 1,2,3,4,5,6
        phaseshift::vector<float> v;
        v.resize_allocation(size);
        for (int i = 0; i < size; ++i) v.push_back(2.0f);  // all 2s

        rb *= v;

        REQUIRE(rb[0] == Catch::Approx(2.0f));
        REQUIRE(rb[1] == Catch::Approx(4.0f));
        REQUIRE(rb[2] == Catch::Approx(6.0f));
        REQUIRE(rb[3] == Catch::Approx(8.0f));
        REQUIRE(rb[4] == Catch::Approx(10.0f));
        REQUIRE(rb[5] == Catch::Approx(12.0f));
    }

    SECTION("operator/= vector - wrapped") {
        phaseshift::ringbuffer<float> rb;
        create_wrapped_ringbuffer(rb, capacity, size, wrap_offset, 10.0f);  // 10,11,12,13,14,15 (but we want multiples)
        rb.clear();
        // Recreate with wrap and specific values
        for (int i = 0; i < wrap_offset; ++i) rb.push_back(0.0f);
        rb.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::vector<float> v;
        v.resize_allocation(size);
        for (int i = 0; i < size; ++i) v.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb /= v;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb[i] == Catch::Approx(6.0f));
        }
    }
}

// =============================================================================
// Ringbuffer-to-Ringbuffer Operators - Both Continuous
// =============================================================================

TEST_CASE("ringbuffer_ringbuffer_operators_both_continuous", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int size = 5;

    SECTION("operator-= ringbuffer - both continuous") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(10);
        rb2.resize_allocation(10);

        for (int i = 0; i < size; ++i) {
            rb1.push_back(static_cast<float>(i) + 20.0f);  // 20,21,22,23,24
            rb2.push_back(static_cast<float>(i) + 10.0f);  // 10,11,12,13,14
        }

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= ringbuffer - both continuous") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(10);
        rb2.resize_allocation(10);

        for (int i = 0; i < size; ++i) {
            rb1.push_back(static_cast<float>(i) + 1.0f);  // 1,2,3,4,5
            rb2.push_back(2.0f);  // 2,2,2,2,2
        }

        rb1 *= rb2;

        REQUIRE(rb1[0] == Catch::Approx(2.0f));
        REQUIRE(rb1[1] == Catch::Approx(4.0f));
        REQUIRE(rb1[2] == Catch::Approx(6.0f));
        REQUIRE(rb1[3] == Catch::Approx(8.0f));
        REQUIRE(rb1[4] == Catch::Approx(10.0f));
    }

    SECTION("operator/= ringbuffer - both continuous") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(10);
        rb2.resize_allocation(10);

        for (int i = 0; i < size; ++i) {
            rb1.push_back(static_cast<float>((i+1) * 10));  // 10,20,30,40,50
            rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5
        }

        rb1 /= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }
}

// =============================================================================
// Ringbuffer-to-Ringbuffer Operators - Destination Wrapped, Source Continuous
// =============================================================================

TEST_CASE("ringbuffer_ringbuffer_operators_dest_wrapped_src_continuous", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;
    const int size = 6;
    const int wrap_offset = 7;

    SECTION("operator-= - dest wrapped, src continuous") {
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, wrap_offset, 20.0f);  // 20,21,22,23,24,25 (wrapped)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb2.push_back(static_cast<float>(i) + 10.0f);  // 10,11,12,13,14,15 (continuous)

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= - dest wrapped, src continuous") {
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, wrap_offset, 1.0f);  // 1,2,3,4,5,6 (wrapped)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb2.push_back(3.0f);  // all 3s (continuous)

        rb1 *= rb2;

        REQUIRE(rb1[0] == Catch::Approx(3.0f));
        REQUIRE(rb1[1] == Catch::Approx(6.0f));
        REQUIRE(rb1[2] == Catch::Approx(9.0f));
        REQUIRE(rb1[3] == Catch::Approx(12.0f));
        REQUIRE(rb1[4] == Catch::Approx(15.0f));
        REQUIRE(rb1[5] == Catch::Approx(18.0f));
    }

    SECTION("operator/= - dest wrapped, src continuous") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb1.push_back(0.0f);
        rb1.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>((i+1) * 12));  // 12,24,36,48,60,72 (wrapped)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6 (continuous)

        rb1 /= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(12.0f));
        }
    }
}

// =============================================================================
// Ringbuffer-to-Ringbuffer Operators - Destination Continuous, Source Wrapped
// =============================================================================

TEST_CASE("ringbuffer_ringbuffer_operators_dest_continuous_src_wrapped", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;
    const int size = 6;
    const int wrap_offset = 7;

    SECTION("operator-= - dest continuous, src wrapped") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>(i) + 20.0f);  // 20,21,22,23,24,25 (continuous)

        phaseshift::ringbuffer<float> rb2;
        create_wrapped_ringbuffer(rb2, capacity, size, wrap_offset, 10.0f);  // 10,11,12,13,14,15 (wrapped)

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= - dest continuous, src wrapped") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>(i) + 1.0f);  // 1,2,3,4,5,6 (continuous)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb2.push_back(0.0f);
        rb2.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb2.push_back(2.0f);  // all 2s (wrapped)

        rb1 *= rb2;

        REQUIRE(rb1[0] == Catch::Approx(2.0f));
        REQUIRE(rb1[1] == Catch::Approx(4.0f));
        REQUIRE(rb1[2] == Catch::Approx(6.0f));
        REQUIRE(rb1[3] == Catch::Approx(8.0f));
        REQUIRE(rb1[4] == Catch::Approx(10.0f));
        REQUIRE(rb1[5] == Catch::Approx(12.0f));
    }

    SECTION("operator/= - dest continuous, src wrapped") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36 (continuous)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb2.push_back(0.0f);
        rb2.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6 (wrapped)

        rb1 /= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }
}

// =============================================================================
// Ringbuffer-to-Ringbuffer Operators - Both Wrapped, Aligned
// =============================================================================

TEST_CASE("ringbuffer_ringbuffer_operators_both_wrapped_aligned", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;
    const int size = 6;
    const int wrap_offset = 7;  // Same offset for both = aligned wrap points

    SECTION("operator-= - both wrapped, aligned") {
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, wrap_offset, 20.0f);  // 20,21,22,23,24,25
        phaseshift::ringbuffer<float> rb2;
        create_wrapped_ringbuffer(rb2, capacity, size, wrap_offset, 10.0f);  // 10,11,12,13,14,15

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= - both wrapped, aligned") {
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, wrap_offset, 1.0f);  // 1,2,3,4,5,6

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb2.push_back(0.0f);
        rb2.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb2.push_back(2.0f);  // all 2s

        rb1 *= rb2;

        REQUIRE(rb1[0] == Catch::Approx(2.0f));
        REQUIRE(rb1[1] == Catch::Approx(4.0f));
        REQUIRE(rb1[2] == Catch::Approx(6.0f));
        REQUIRE(rb1[3] == Catch::Approx(8.0f));
        REQUIRE(rb1[4] == Catch::Approx(10.0f));
        REQUIRE(rb1[5] == Catch::Approx(12.0f));
    }

    SECTION("operator/= - both wrapped, aligned") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb1.push_back(0.0f);
        rb1.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < wrap_offset; ++i) rb2.push_back(0.0f);
        rb2.pop_front(wrap_offset);
        for (int i = 0; i < size; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb1 /= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }
}

// =============================================================================
// Ringbuffer-to-Ringbuffer Operators - Both Wrapped, Misaligned
// =============================================================================

TEST_CASE("ringbuffer_ringbuffer_operators_both_wrapped_misaligned", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;
    const int size = 6;

    SECTION("operator-= - both wrapped, src breaks first") {
        // Dest wraps at offset 7 (3 elements before wrap, 3 after)
        // Src wraps at offset 8 (2 elements before wrap, 4 after)
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, 7, 20.0f);  // 20,21,22,23,24,25
        phaseshift::ringbuffer<float> rb2;
        create_wrapped_ringbuffer(rb2, capacity, size, 8, 10.0f);  // 10,11,12,13,14,15

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator-= - both wrapped, dest breaks first") {
        // Dest wraps at offset 8 (2 elements before wrap, 4 after)
        // Src wraps at offset 7 (3 elements before wrap, 3 after)
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, 8, 20.0f);  // 20,21,22,23,24,25
        phaseshift::ringbuffer<float> rb2;
        create_wrapped_ringbuffer(rb2, capacity, size, 7, 10.0f);  // 10,11,12,13,14,15

        rb1 -= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(10.0f));
        }
    }

    SECTION("operator*= - both wrapped, misaligned") {
        phaseshift::ringbuffer<float> rb1;
        create_wrapped_ringbuffer(rb1, capacity, size, 7, 1.0f);  // 1,2,3,4,5,6

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 8; ++i) rb2.push_back(0.0f);
        rb2.pop_front(8);
        for (int i = 0; i < size; ++i) rb2.push_back(2.0f);  // all 2s

        rb1 *= rb2;

        REQUIRE(rb1[0] == Catch::Approx(2.0f));
        REQUIRE(rb1[1] == Catch::Approx(4.0f));
        REQUIRE(rb1[2] == Catch::Approx(6.0f));
        REQUIRE(rb1[3] == Catch::Approx(8.0f));
        REQUIRE(rb1[4] == Catch::Approx(10.0f));
        REQUIRE(rb1[5] == Catch::Approx(12.0f));
    }

    SECTION("operator/= - both wrapped, misaligned") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 7; ++i) rb1.push_back(0.0f);
        rb1.pop_front(7);
        for (int i = 0; i < size; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 8; ++i) rb2.push_back(0.0f);
        rb2.pop_front(8);
        for (int i = 0; i < size; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb1 /= rb2;

        for (int i = 0; i < size; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }
}

// =============================================================================
// divide_equal_range - All Cases
// =============================================================================

TEST_CASE("ringbuffer_divide_equal_range", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    const int capacity = 10;

    SECTION("both continuous") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(capacity);
        rb2.resize_allocation(capacity);

        for (int i = 0; i < 6; ++i) {
            rb1.push_back(static_cast<float>((i+1) * 10));  // 10,20,30,40,50,60
            rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6
        }

        rb1.divide_equal_range(rb2, 4);  // Only divide first 4 elements

        REQUIRE(rb1[0] == Catch::Approx(10.0f));
        REQUIRE(rb1[1] == Catch::Approx(10.0f));
        REQUIRE(rb1[2] == Catch::Approx(10.0f));
        REQUIRE(rb1[3] == Catch::Approx(10.0f));
        REQUIRE(rb1[4] == Catch::Approx(50.0f));  // Unchanged
        REQUIRE(rb1[5] == Catch::Approx(60.0f));  // Unchanged
    }

    SECTION("dest continuous, src wrapped") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 6; ++i) rb1.push_back(static_cast<float>((i+1) * 12));  // 12,24,36,48,60,72

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 7; ++i) rb2.push_back(0.0f);
        rb2.pop_front(7);
        for (int i = 0; i < 6; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6 (wrapped)

        rb1.divide_equal_range(rb2, 6);

        for (int i = 0; i < 6; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(12.0f));
        }
    }

    SECTION("dest wrapped, src continuous") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 7; ++i) rb1.push_back(0.0f);
        rb1.pop_front(7);
        for (int i = 0; i < 6; ++i) rb1.push_back(static_cast<float>((i+1) * 12));  // 12,24,36,48,60,72 (wrapped)

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 6; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6 (continuous)

        rb1.divide_equal_range(rb2, 6);

        for (int i = 0; i < 6; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(12.0f));
        }
    }

    SECTION("both wrapped, aligned") {
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 7; ++i) rb1.push_back(0.0f);
        rb1.pop_front(7);
        for (int i = 0; i < 6; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 7; ++i) rb2.push_back(0.0f);
        rb2.pop_front(7);
        for (int i = 0; i < 6; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb1.divide_equal_range(rb2, 6);

        for (int i = 0; i < 6; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }

    SECTION("both wrapped, src breaks first (3 segments)") {
        // Dest at offset 6, src at offset 8
        // For size=6, capacity=10:
        // Dest: elements at indices 6,7,8,9,0,1 (wraps after 4 elements)
        // Src: elements at indices 8,9,0,1,2,3 (wraps after 2 elements)
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 6; ++i) rb1.push_back(0.0f);
        rb1.pop_front(6);
        for (int i = 0; i < 6; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 8; ++i) rb2.push_back(0.0f);
        rb2.pop_front(8);
        for (int i = 0; i < 6; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb1.divide_equal_range(rb2, 6);

        for (int i = 0; i < 6; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }

    SECTION("both wrapped, dest breaks first (3 segments)") {
        // Dest at offset 8, src at offset 6
        phaseshift::ringbuffer<float> rb1;
        rb1.resize_allocation(capacity);
        for (int i = 0; i < 8; ++i) rb1.push_back(0.0f);
        rb1.pop_front(8);
        for (int i = 0; i < 6; ++i) rb1.push_back(static_cast<float>((i+1) * 6));  // 6,12,18,24,30,36

        phaseshift::ringbuffer<float> rb2;
        rb2.resize_allocation(capacity);
        for (int i = 0; i < 6; ++i) rb2.push_back(0.0f);
        rb2.pop_front(6);
        for (int i = 0; i < 6; ++i) rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6

        rb1.divide_equal_range(rb2, 6);

        for (int i = 0; i < 6; ++i) {
            REQUIRE(rb1[i] == Catch::Approx(6.0f));
        }
    }

    SECTION("partial range") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(capacity);
        rb2.resize_allocation(capacity);

        for (int i = 0; i < 8; ++i) {
            rb1.push_back(static_cast<float>((i+1) * 10));  // 10,20,30,40,50,60,70,80
            rb2.push_back(static_cast<float>(i + 1));  // 1,2,3,4,5,6,7,8
        }

        rb1.divide_equal_range(rb2, 3);  // Only divide first 3

        REQUIRE(rb1[0] == Catch::Approx(10.0f));
        REQUIRE(rb1[1] == Catch::Approx(10.0f));
        REQUIRE(rb1[2] == Catch::Approx(10.0f));
        REQUIRE(rb1[3] == Catch::Approx(40.0f));  // Unchanged
        REQUIRE(rb1[4] == Catch::Approx(50.0f));  // Unchanged
    }

    SECTION("zero size does nothing") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(capacity);
        rb2.resize_allocation(capacity);

        rb1.push_back(10.0f);
        rb1.push_back(20.0f);
        rb2.push_back(1.0f);
        rb2.push_back(2.0f);

        rb1.divide_equal_range(rb2, 0);

        REQUIRE(rb1[0] == Catch::Approx(10.0f));
        REQUIRE(rb1[1] == Catch::Approx(20.0f));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("ringbuffer_edge_cases", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    SECTION("empty vector operations do nothing") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);

        phaseshift::vector<float> v;
        v.resize_allocation(0);

        // These should not crash
        rb += v;
        rb -= v;
        rb *= v;
        rb /= v;

        REQUIRE(rb.size() == 0);
    }

    SECTION("empty ringbuffer operations do nothing") {
        phaseshift::ringbuffer<float> rb1, rb2;
        rb1.resize_allocation(10);
        rb2.resize_allocation(10);

        // These should not crash
        rb1 -= rb2;
        rb1 *= rb2;
        rb1 /= rb2;

        REQUIRE(rb1.size() == 0);
    }

    SECTION("single element operations") {
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(10);
        rb.push_back(10.0f);

        phaseshift::vector<float> v;
        v.resize_allocation(1);
        v.push_back(2.0f);

        rb *= v;
        REQUIRE(rb[0] == Catch::Approx(20.0f));

        rb /= v;
        REQUIRE(rb[0] == Catch::Approx(10.0f));
    }

    SECTION("full capacity ringbuffer") {
        const int cap = 8;
        phaseshift::ringbuffer<float> rb;
        rb.resize_allocation(cap);

        // Fill to capacity
        for (int i = 0; i < cap; ++i) {
            rb.push_back(static_cast<float>(i + 1));
        }

        phaseshift::vector<float> v;
        v.resize_allocation(cap);
        for (int i = 0; i < cap; ++i) {
            v.push_back(2.0f);
        }

        rb *= v;

        for (int i = 0; i < cap; ++i) {
            REQUIRE(rb[i] == Catch::Approx(static_cast<float>((i + 1) * 2)));
        }
    }
}

// =============================================================================
// Push Back Ringbuffer (existing test case, now with content)
// =============================================================================

TEST_CASE("ringbuffer_push_back_ringbuffer", "[ringbuffer]") {
    phaseshift::dev::check_compilation_options();

    SECTION("push_back from continuous to continuous") {
        phaseshift::ringbuffer<float> src, dst;
        src.resize_allocation(10);
        dst.resize_allocation(20);

        for (int i = 0; i < 5; ++i) src.push_back(static_cast<float>(i));

        dst.push_back(src, 1, 3);  // Push elements 1,2,3

        REQUIRE(dst.size() == 3);
        REQUIRE(dst[0] == Catch::Approx(1.0f));
        REQUIRE(dst[1] == Catch::Approx(2.0f));
        REQUIRE(dst[2] == Catch::Approx(3.0f));
    }

    SECTION("push_back from wrapped source") {
        phaseshift::ringbuffer<float> src, dst;
        src.resize_allocation(10);
        dst.resize_allocation(20);

        // Create wrapped source
        for (int i = 0; i < 7; ++i) src.push_back(0.0f);
        src.pop_front(7);
        for (int i = 0; i < 5; ++i) src.push_back(static_cast<float>(i + 10));  // 10,11,12,13,14

        dst.push_back(src, 0, 5);

        REQUIRE(dst.size() == 5);
        for (int i = 0; i < 5; ++i) {
            REQUIRE(dst[i] == Catch::Approx(static_cast<float>(i + 10)));
        }
    }

    SECTION("push_back to wrapped destination") {
        phaseshift::ringbuffer<float> src, dst;
        src.resize_allocation(10);
        dst.resize_allocation(10);

        // Create wrapped destination
        for (int i = 0; i < 7; ++i) dst.push_back(0.0f);
        dst.pop_front(7);
        dst.push_back(100.0f);  // One element at position 7

        for (int i = 0; i < 5; ++i) src.push_back(static_cast<float>(i));

        dst.push_back(src, 0, 5);

        REQUIRE(dst.size() == 6);
        REQUIRE(dst[0] == Catch::Approx(100.0f));
        for (int i = 0; i < 5; ++i) {
            REQUIRE(dst[i + 1] == Catch::Approx(static_cast<float>(i)));
        }
    }
}
