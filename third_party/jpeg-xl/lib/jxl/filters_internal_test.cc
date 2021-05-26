// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/filters_internal.h"

#include "gtest/gtest.h"

namespace jxl {

class FiltersInternalTest : public ::testing::Test {};

// Test the mping of rows using RowMapMod.
TEST(FiltersInternalTest, RowMapModTest) {
  RowMapMod<5> m;
  // Identity part:
  EXPECT_EQ(0, m(0));
  EXPECT_EQ(4, m(4));

  // Larger than the module work.
  EXPECT_EQ(0, m(5));
  EXPECT_EQ(1, m(11));

  // Smaller than 0 up to a block.
  EXPECT_EQ(4, m(-1));
  EXPECT_EQ(2, m(-8));
}

// Test the implementation for mirroring of rows.
TEST(FiltersInternalTest, RowMapMirrorTest) {
  RowMapMirror m(0, 10);  // Image size of 10 rows.

  EXPECT_EQ(2, m(-3));
  EXPECT_EQ(1, m(-2));
  EXPECT_EQ(0, m(-1));

  EXPECT_EQ(0, m(0));
  EXPECT_EQ(9, m(9));

  EXPECT_EQ(9, m(10));
  EXPECT_EQ(8, m(11));
  EXPECT_EQ(7, m(12));

  // It mirrors the rows to infinity.
  EXPECT_EQ(1, m(21));
  EXPECT_EQ(1, m(41));
}

}  // namespace jxl
