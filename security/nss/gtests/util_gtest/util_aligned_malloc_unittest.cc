/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "scoped_ptrs_util.h"

namespace nss_test {

struct SomeContext {
  uint8_t some_buf[13];
  void *mem;
};

template <class T>
struct ScopedDelete {
  void operator()(T *ptr) {
    if (ptr) {
      PORT_Free(ptr->mem);
    }
  }
};
typedef std::unique_ptr<SomeContext, ScopedDelete<SomeContext> >
    ScopedSomeContext;

class AlignedMallocTest : public ::testing::Test,
                          public ::testing::WithParamInterface<size_t> {
 protected:
  ScopedSomeContext test_align_new(size_t alignment) {
    ScopedSomeContext ctx(PORT_ZNewAligned(SomeContext, alignment, mem));
    return ctx;
  };
  ScopedSomeContext test_align_alloc(size_t alignment) {
    void *mem = nullptr;
    ScopedSomeContext ctx((SomeContext *)PORT_ZAllocAligned(sizeof(SomeContext),
                                                            alignment, &mem));
    if (ctx) {
      ctx->mem = mem;
    }
    return ctx;
  }
};

TEST_P(AlignedMallocTest, TestNew) {
  size_t alignment = GetParam();
  ScopedSomeContext ctx = test_align_new(alignment);
  EXPECT_TRUE(ctx.get());
  EXPECT_EQ(0U, (uintptr_t)ctx.get() % alignment);
}

TEST_P(AlignedMallocTest, TestAlloc) {
  size_t alignment = GetParam();
  ScopedSomeContext ctx = test_align_alloc(alignment);
  EXPECT_TRUE(ctx.get());
  EXPECT_EQ(0U, (uintptr_t)ctx.get() % alignment);
}

class AlignedMallocTestBadSize : public AlignedMallocTest {};

TEST_P(AlignedMallocTestBadSize, TestNew) {
  size_t alignment = GetParam();
  ScopedSomeContext ctx = test_align_new(alignment);
  EXPECT_FALSE(ctx.get());
}

TEST_P(AlignedMallocTestBadSize, TestAlloc) {
  size_t alignment = GetParam();
  ScopedSomeContext ctx = test_align_alloc(alignment);
  EXPECT_FALSE(ctx.get());
}

static const size_t kSizes[] = {1, 2, 4, 8, 16, 32, 64};
static const size_t kBadSizes[] = {0, 7, 17, 24, 56};

INSTANTIATE_TEST_SUITE_P(AllAligned, AlignedMallocTest,
                         ::testing::ValuesIn(kSizes));
INSTANTIATE_TEST_SUITE_P(AllAlignedBadSize, AlignedMallocTestBadSize,
                         ::testing::ValuesIn(kBadSizes));

}  // namespace nss_test
