/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozmemory.h"

#include "gtest/gtest.h"

using namespace mozilla;

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

#define K   * 1024
#define M   * 1024 * 1024

TEST(Jemalloc, UsableSizeInAdvance)
{
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

static int gStaticVar;

bool InfoEq(jemalloc_ptr_info_t& aInfo, PtrInfoTag aTag, void* aAddr,
            size_t aSize)
{
  return aInfo.tag == aTag && aInfo.addr == aAddr && aInfo.size == aSize;
}

bool InfoEqFreedPage(jemalloc_ptr_info_t& aInfo, void* aAddr, size_t aPageSize)
{
  size_t pageSizeMask = aPageSize - 1;

  return jemalloc_ptr_is_freed_page(&aInfo) &&
         aInfo.addr == (void*)(uintptr_t(aAddr) & ~pageSizeMask) &&
         aInfo.size == aPageSize;
}

TEST(Jemalloc, PtrInfo)
{
  // Some things might be running in other threads, so ensure our assumptions
  // (e.g. about isFreedSmall and isFreedPage ratios below) are not altered by
  // other threads.
  jemalloc_thread_local_arena(true);

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  jemalloc_ptr_info_t info;
  Vector<char*> small, large, huge;

  // For small (<= 2KiB) allocations, test every position within many possible
  // sizes.
  size_t small_max = stats.page_size / 2;
  for (size_t n = 0; n <= small_max; n += 8) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(small.append(p));
    for (size_t j = 0; j < usable; j++) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveSmall, p, usable));
    }
  }

  // Similar for large (2KiB + 1 KiB .. 1MiB - 8KiB) allocations.
  for (size_t n = small_max + 1 K; n <= stats.large_max; n += 1 K) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(large.append(p));
    for (size_t j = 0; j < usable; j += 347) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveLarge, p, usable));
    }
  }

  // Similar for huge (> 1MiB - 8KiB) allocations.
  for (size_t n = stats.chunksize; n <= 10 M; n += 512 K) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(huge.append(p));
    for (size_t j = 0; j < usable; j += 567) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveHuge, p, usable));
    }
  }

  // The following loops check freed allocations. We step through the vectors
  // using prime-sized steps, which gives full coverage of the arrays while
  // avoiding deallocating in the same order we allocated.
  size_t len;

  // Free the small allocations and recheck them.
  int isFreedSmall = 0, isFreedPage = 0;
  len = small.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 19) % len) {
    char* p = small[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k++) {
      jemalloc_ptr_info(&p[k], &info);
      // There are two valid outcomes here.
      if (InfoEq(info, TagFreedSmall, p, usable)) {
        isFreedSmall++;
      } else if (InfoEqFreedPage(info, &p[k], stats.page_size)) {
        isFreedPage++;
      } else {
        ASSERT_TRUE(false);
      }
    }
  }
  // There should be both FreedSmall and FreedPage results, but a lot more of
  // the former.
  ASSERT_TRUE(isFreedSmall != 0);
  ASSERT_TRUE(isFreedPage != 0);
  ASSERT_TRUE(isFreedSmall / isFreedPage > 10);

  // Free the large allocations and recheck them.
  len = large.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 31) % len) {
    char* p = large[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k += 357) {
      jemalloc_ptr_info(&p[k], &info);
      ASSERT_TRUE(InfoEqFreedPage(info, &p[k], stats.page_size));
    }
  }

  // Free the huge allocations and recheck them.
  len = huge.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 7) % len) {
    char* p = huge[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k += 587) {
      jemalloc_ptr_info(&p[k], &info);
      ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
    }
  }

  // Null ptr.
  jemalloc_ptr_info(nullptr, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Near-null ptr.
  jemalloc_ptr_info((void*)0x123, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Maximum address.
  jemalloc_ptr_info((void*)uintptr_t(-1), &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Stack memory.
  int stackVar;
  jemalloc_ptr_info(&stackVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Code memory.
  jemalloc_ptr_info((const void*)&jemalloc_ptr_info, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Static memory.
  jemalloc_ptr_info(&gStaticVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));

  // Chunk header.
  UniquePtr<int> p = MakeUnique<int>();
  size_t chunksizeMask = stats.chunksize - 1;
  char* chunk = (char*)(uintptr_t(p.get()) & ~chunksizeMask);
  size_t chunkHeaderSize = stats.chunksize - stats.large_max;
  for (size_t i = 0; i < chunkHeaderSize; i += 64) {
    jemalloc_ptr_info(&chunk[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
  }

  // Run header.
  size_t page_sizeMask = stats.page_size - 1;
  char* run = (char*)(uintptr_t(p.get()) & ~page_sizeMask);
  for (size_t i = 0; i < 4 * sizeof(void*); i++) {
    jemalloc_ptr_info(&run[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U));
  }

  // Entire chunk. It's impossible to check what is put into |info| for all of
  // these addresses; this is more about checking that we don't crash.
  for (size_t i = 0; i < stats.chunksize; i += 256) {
    jemalloc_ptr_info(&chunk[i], &info);
  }

  jemalloc_thread_local_arena(false);
}

#undef K
#undef M
