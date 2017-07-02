// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "core/fxcrt/fx_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kMaxByteAlloc = std::numeric_limits<size_t>::max();
const size_t kMaxIntAlloc = kMaxByteAlloc / sizeof(int);
const size_t kOverflowIntAlloc = kMaxIntAlloc + 100;
const size_t kWidth = 640;
const size_t kOverflowIntAlloc2D = kMaxIntAlloc / kWidth + 10;

}  // namespace

// TODO(tsepez): re-enable OOM tests if we can find a way to
// prevent it from hosing the bots.
TEST(fxcrt, DISABLED_FX_AllocOOM) {
  EXPECT_DEATH_IF_SUPPORTED((void)FX_Alloc(int, kMaxIntAlloc), "");

  int* ptr = FX_Alloc(int, 1);
  EXPECT_TRUE(ptr);
  EXPECT_DEATH_IF_SUPPORTED((void)FX_Realloc(int, ptr, kMaxIntAlloc), "");
  FX_Free(ptr);
}

TEST(fxcrt, FX_AllocOverflow) {
  // |ptr| needs to be defined and used to avoid Clang optimizes away the
  // FX_Alloc() statement overzealously for optimized builds.
  int* ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(ptr = FX_Alloc(int, kOverflowIntAlloc), "") << ptr;

  ptr = FX_Alloc(int, 1);
  EXPECT_TRUE(ptr);
  EXPECT_DEATH_IF_SUPPORTED((void)FX_Realloc(int, ptr, kOverflowIntAlloc), "");
  FX_Free(ptr);
}

TEST(fxcrt, FX_AllocOverflow2D) {
  // |ptr| needs to be defined and used to avoid Clang optimizes away the
  // FX_Alloc() statement overzealously for optimized builds.
  int* ptr = nullptr;
  EXPECT_DEATH_IF_SUPPORTED(ptr = FX_Alloc2D(int, kWidth, kOverflowIntAlloc2D),
                            "")
      << ptr;
}

TEST(fxcrt, DISABLED_FX_TryAllocOOM) {
  EXPECT_FALSE(FX_TryAlloc(int, kMaxIntAlloc));

  int* ptr = FX_Alloc(int, 1);
  EXPECT_TRUE(ptr);
  EXPECT_FALSE(FX_TryRealloc(int, ptr, kMaxIntAlloc));
  FX_Free(ptr);
}

TEST(fxcrt, FX_TryAllocOverflow) {
  // |ptr| needs to be defined and used to avoid Clang optimizes away the
  // calloc() statement overzealously for optimized builds.
  int* ptr = (int*)calloc(sizeof(int), kOverflowIntAlloc);
  EXPECT_FALSE(ptr) << ptr;

  ptr = FX_Alloc(int, 1);
  EXPECT_TRUE(ptr);
  EXPECT_FALSE(FX_TryRealloc(int, ptr, kOverflowIntAlloc));
  FX_Free(ptr);
}

TEST(fxcrt, DISABLED_FXMEM_DefaultOOM) {
  EXPECT_FALSE(FXMEM_DefaultAlloc(kMaxByteAlloc, 0));

  void* ptr = FXMEM_DefaultAlloc(1, 0);
  EXPECT_TRUE(ptr);
  EXPECT_FALSE(FXMEM_DefaultRealloc(ptr, kMaxByteAlloc, 0));
  FXMEM_DefaultFree(ptr, 0);
}
