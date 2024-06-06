// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/padded_bytes.h"

#include <jxl/memory_manager.h>

#include <cstddef>

#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

TEST(PaddedBytesTest, TestNonEmptyFirstByteZero) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  PaddedBytes pb(memory_manager, 1);
  EXPECT_EQ(0, pb[0]);
  // Even after resizing..
  pb.resize(20);
  EXPECT_EQ(0, pb[0]);
  // And reserving.
  pb.reserve(200);
  EXPECT_EQ(0, pb[0]);
}

TEST(PaddedBytesTest, TestEmptyFirstByteZero) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  PaddedBytes pb(memory_manager, 0);
  // After resizing - new zero is written despite there being nothing to copy.
  pb.resize(20);
  EXPECT_EQ(0, pb[0]);
}

TEST(PaddedBytesTest, TestFillWithoutReserve) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  PaddedBytes pb{memory_manager};
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_GE(pb.capacity(), 170u);
}

TEST(PaddedBytesTest, TestFillWithExactReserve) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  PaddedBytes pb{memory_manager};
  pb.reserve(170);
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_EQ(pb.capacity(), 170u);
}

TEST(PaddedBytesTest, TestFillWithMoreReserve) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  PaddedBytes pb{memory_manager};
  pb.reserve(171);
  for (size_t i = 0; i < 170u; ++i) {
    pb.push_back(i);
  }
  EXPECT_EQ(170u, pb.size());
  EXPECT_GT(pb.capacity(), 170u);
}

}  // namespace
}  // namespace jxl
