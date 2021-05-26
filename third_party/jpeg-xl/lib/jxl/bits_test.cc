// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/base/bits.h"

#include "gtest/gtest.h"

namespace jxl {
namespace {

TEST(BitsTest, TestNumZeroBits) {
  // Zero input is well-defined.
  EXPECT_EQ(32, Num0BitsAboveMS1Bit(0u));
  EXPECT_EQ(64, Num0BitsAboveMS1Bit(0ull));
  EXPECT_EQ(32, Num0BitsBelowLS1Bit(0u));
  EXPECT_EQ(64, Num0BitsBelowLS1Bit(0ull));

  EXPECT_EQ(31, Num0BitsAboveMS1Bit(1u));
  EXPECT_EQ(30, Num0BitsAboveMS1Bit(2u));
  EXPECT_EQ(63, Num0BitsAboveMS1Bit(1ull));
  EXPECT_EQ(62, Num0BitsAboveMS1Bit(2ull));

  EXPECT_EQ(0, Num0BitsBelowLS1Bit(1u));
  EXPECT_EQ(0, Num0BitsBelowLS1Bit(1ull));
  EXPECT_EQ(1, Num0BitsBelowLS1Bit(2u));
  EXPECT_EQ(1, Num0BitsBelowLS1Bit(2ull));

  EXPECT_EQ(0, Num0BitsAboveMS1Bit(0x80000000u));
  EXPECT_EQ(0, Num0BitsAboveMS1Bit(0x8000000000000000ull));
  EXPECT_EQ(31, Num0BitsBelowLS1Bit(0x80000000u));
  EXPECT_EQ(63, Num0BitsBelowLS1Bit(0x8000000000000000ull));
}

TEST(BitsTest, TestFloorLog2) {
  // for input = [1, 7]
  const int expected[7] = {0, 1, 1, 2, 2, 2, 2};
  for (uint32_t i = 1; i <= 7; ++i) {
    EXPECT_EQ(expected[i - 1], FloorLog2Nonzero(i)) << " " << i;
    EXPECT_EQ(expected[i - 1], FloorLog2Nonzero(uint64_t(i))) << " " << i;
  }

  EXPECT_EQ(31, FloorLog2Nonzero(0x80000000u));
  EXPECT_EQ(31, FloorLog2Nonzero(0x80000001u));
  EXPECT_EQ(31, FloorLog2Nonzero(0xFFFFFFFFu));

  EXPECT_EQ(31, FloorLog2Nonzero(0x80000000ull));
  EXPECT_EQ(31, FloorLog2Nonzero(0x80000001ull));
  EXPECT_EQ(31, FloorLog2Nonzero(0xFFFFFFFFull));

  EXPECT_EQ(63, FloorLog2Nonzero(0x8000000000000000ull));
  EXPECT_EQ(63, FloorLog2Nonzero(0x8000000000000001ull));
  EXPECT_EQ(63, FloorLog2Nonzero(0xFFFFFFFFFFFFFFFFull));
}

TEST(BitsTest, TestCeilLog2) {
  // for input = [1, 7]
  const int expected[7] = {0, 1, 2, 2, 3, 3, 3};
  for (uint32_t i = 1; i <= 7; ++i) {
    EXPECT_EQ(expected[i - 1], CeilLog2Nonzero(i)) << " " << i;
    EXPECT_EQ(expected[i - 1], CeilLog2Nonzero(uint64_t(i))) << " " << i;
  }

  EXPECT_EQ(31, CeilLog2Nonzero(0x80000000u));
  EXPECT_EQ(32, CeilLog2Nonzero(0x80000001u));
  EXPECT_EQ(32, CeilLog2Nonzero(0xFFFFFFFFu));

  EXPECT_EQ(31, CeilLog2Nonzero(0x80000000ull));
  EXPECT_EQ(32, CeilLog2Nonzero(0x80000001ull));
  EXPECT_EQ(32, CeilLog2Nonzero(0xFFFFFFFFull));

  EXPECT_EQ(63, CeilLog2Nonzero(0x8000000000000000ull));
  EXPECT_EQ(64, CeilLog2Nonzero(0x8000000000000001ull));
  EXPECT_EQ(64, CeilLog2Nonzero(0xFFFFFFFFFFFFFFFFull));
}

}  // namespace
}  // namespace jxl
