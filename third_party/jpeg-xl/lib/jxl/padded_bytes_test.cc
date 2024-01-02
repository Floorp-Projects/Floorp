// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/padded_bytes.h"

#include <numeric>  // iota
#include <vector>

#include "lib/jxl/testing.h"

namespace jxl {
namespace {

TEST(PaddedBytesTest, TestNonEmptyFirstByteZero) {
  PaddedBytes pb(1);
  EXPECT_EQ(0, pb[0]);
  // Even after resizing..
  pb.resize(20);
  EXPECT_EQ(0, pb[0]);
  // And reserving.
  pb.reserve(200);
  EXPECT_EQ(0, pb[0]);
}

TEST(PaddedBytesTest, TestEmptyFirstByteZero) {
  PaddedBytes pb(0);
  // After resizing - new zero is written despite there being nothing to copy.
  pb.resize(20);
  EXPECT_EQ(0, pb[0]);
}

TEST(PaddedBytesTest, TestFillWithoutReserve) {
  PaddedBytes pb;
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_GE(pb.capacity(), 170u);
}

TEST(PaddedBytesTest, TestFillWithExactReserve) {
  PaddedBytes pb;
  pb.reserve(170);
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_EQ(pb.capacity(), 170u);
}

TEST(PaddedBytesTest, TestFillWithMoreReserve) {
  PaddedBytes pb;
  pb.reserve(171);
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_GT(pb.capacity(), 170u);
}

}  // namespace
}  // namespace jxl
