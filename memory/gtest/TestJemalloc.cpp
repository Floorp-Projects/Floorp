/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozmemory.h"
#include "Utils.h"

#include "gtest/gtest.h"

#ifdef MOZ_CRASHREPORTER
#include "nsCOMPtr.h"
#include "nsICrashReporter.h"
#include "nsServiceManagerUtils.h"
#endif

#ifdef NIGHTLY_BUILD
#if defined(DEBUG) && !defined(XP_WIN) && !defined(ANDROID)
#define HAS_GDB_SLEEP_DURATION 1
extern unsigned int _gdb_sleep_duration;
#endif

// Death tests are too slow on OSX because of the system crash reporter.
#ifndef XP_DARWIN
static void DisableCrashReporter()
{
#ifdef MOZ_CRASHREPORTER
  nsCOMPtr<nsICrashReporter> crashreporter =
    do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashreporter) {
    crashreporter->SetEnabled(false);
  }
#endif
}

// Wrap ASSERT_DEATH_IF_SUPPORTED to disable the crash reporter
// when entering the subprocess, so that the expected crashes don't
// create a minidump that the gtest harness will interpret as an error.
#define ASSERT_DEATH_WRAP(a, b) \
  ASSERT_DEATH_IF_SUPPORTED({ DisableCrashReporter(); a; }, b)
#else
#define ASSERT_DEATH_WRAP(a, b)
#endif
#endif

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

TEST(Jemalloc, UsableSizeInAdvance)
{
  /*
   * Test every size up to a certain point, then (N-1, N, N+1) triplets for a
   * various sizes beyond that.
   */

  for (size_t n = 0; n < 16_KiB; n++)
    ASSERT_NO_FATAL_FAILURE(TestOne(n));

  for (size_t n = 16_KiB; n < 1_MiB; n += 4_KiB)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));

  for (size_t n = 1_MiB; n < 8_MiB; n += 128_KiB)
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
  for (size_t n = small_max + 1_KiB; n <= stats.large_max; n += 1_KiB) {
    auto p = (char*)malloc(n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(large.append(p));
    for (size_t j = 0; j < usable; j += 347) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveLarge, p, usable));
    }
  }

  // Similar for huge (> 1MiB - 8KiB) allocations.
  for (size_t n = stats.chunksize; n <= 10_MiB; n += 512_KiB) {
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

#ifdef NIGHTLY_BUILD
TEST(Jemalloc, Arenas)
{
  arena_id_t arena = moz_create_arena();
  ASSERT_TRUE(arena != 0);
  void* ptr = moz_arena_malloc(arena, 42);
  ASSERT_TRUE(ptr != nullptr);
  ptr = moz_arena_realloc(arena, ptr, 64);
  ASSERT_TRUE(ptr != nullptr);
  moz_arena_free(arena, ptr);
  ptr = moz_arena_calloc(arena, 24, 2);
  // For convenience, free can be used to free arena pointers.
  free(ptr);
  moz_dispose_arena(arena);

#ifdef HAS_GDB_SLEEP_DURATION
  // Avoid death tests adding some unnecessary (long) delays.
  unsigned int old_gdb_sleep_duration = _gdb_sleep_duration;
  _gdb_sleep_duration = 0;
#endif

  // Can't use an arena after it's disposed.
  ASSERT_DEATH_WRAP(moz_arena_malloc(arena, 80), "");

  // Arena id 0 can't be used to somehow get to the main arena.
  ASSERT_DEATH_WRAP(moz_arena_malloc(0, 80), "");

  arena = moz_create_arena();
  arena_id_t arena2 = moz_create_arena();

  // For convenience, realloc can also be used to reallocate arena pointers.
  // The result should be in the same arena. Test various size class transitions.
  size_t sizes[] = { 1, 42, 80, 1_KiB, 1.5_KiB, 72_KiB, 129_KiB, 2.5_MiB, 5.1_MiB };
  for (size_t from_size : sizes) {
    for (size_t to_size : sizes) {
      ptr = moz_arena_malloc(arena, from_size);
      ptr = realloc(ptr, to_size);
      // Freeing with the wrong arena should crash.
      ASSERT_DEATH_WRAP(moz_arena_free(arena2, ptr), "");
      // Likewise for moz_arena_realloc.
      ASSERT_DEATH_WRAP(moz_arena_realloc(arena2, ptr, from_size), "");
      // The following will crash if it's not in the right arena.
      moz_arena_free(arena, ptr);
    }
  }

  moz_dispose_arena(arena2);
  moz_dispose_arena(arena);

#ifdef HAS_GDB_SLEEP_DURATION
  _gdb_sleep_duration = old_gdb_sleep_duration;
#endif
}
#endif
