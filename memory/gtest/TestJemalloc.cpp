/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozmemory.h"

#include "gtest/gtest.h"

static inline void
TestOne(size_t size)
{
    size_t req = size;
    size_t adv = malloc_good_size(req);
    char* p = (char*)malloc(req);
    size_t usable = moz_malloc_usable_size(p);
    // NB: Using EXPECT here so that we still free the memory on failure.
    EXPECT_EQ(adv, usable) <<
           "malloc_good_size(" << req << ") --> " << adv << "; "
           "malloc_usable_size(" << req << ") --> " << usable;
    free(p);
}

static inline void
TestThree(size_t size)
{
    ASSERT_NO_FATAL_FAILURE(TestOne(size - 1));
    ASSERT_NO_FATAL_FAILURE(TestOne(size));
    ASSERT_NO_FATAL_FAILURE(TestOne(size + 1));
}

TEST(Jemalloc, UsableSizeInAdvance)
{
  #define K   * 1024
  #define M   * 1024 * 1024

  /*
   * Test every size up to a certain point, then (N-1, N, N+1) triplets for a
   * various sizes beyond that.
   */

  for (size_t n = 0; n < 16 K; n++)
    ASSERT_NO_FATAL_FAILURE(TestOne(n));

  for (size_t n = 16 K; n < 1 M; n += 4 K)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));

  for (size_t n = 1 M; n < 8 M; n += 128 K)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));
}
