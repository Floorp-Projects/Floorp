// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JPEGLI_TESTING_H_
#define LIB_JPEGLI_TESTING_H_

// GTest specific macros / wrappers.

#include "gtest/gtest.h"

// googletest before 1.10 didn't define INSTANTIATE_TEST_SUITE_P() but instead
// used INSTANTIATE_TEST_CASE_P which is now deprecated.
#ifdef INSTANTIATE_TEST_SUITE_P
#define JPEGLI_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_SUITE_P
#else
#define JPEGLI_INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

// Ensures that we don't make our test bounds too lax, effectively disabling the
// tests.
#define EXPECT_SLIGHTLY_BELOW(A, E)       \
  {                                       \
    double _actual = (A);                 \
    double _expected = (E);               \
    EXPECT_LE(_actual, _expected);        \
    EXPECT_GE(_actual, 0.75 * _expected); \
  }

#endif  // LIB_JPEGLI_TESTING_H_
