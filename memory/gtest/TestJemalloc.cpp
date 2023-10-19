/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Vector.h"
#include "mozilla/gtest/MozHelpers.h"
#include "mozmemory.h"
#include "nsCOMPtr.h"
#include "Utils.h"

#include "gtest/gtest.h"

#ifdef MOZ_PHC
#  include "PHC.h"
#endif

using namespace mozilla;

class AutoDisablePHCOnCurrentThread {
 public:
  AutoDisablePHCOnCurrentThread() {
#ifdef MOZ_PHC
    mozilla::phc::DisablePHCOnCurrentThread();
#endif
  }

  ~AutoDisablePHCOnCurrentThread() {
#ifdef MOZ_PHC
    mozilla::phc::ReenablePHCOnCurrentThread();
#endif
  }
};

static inline void TestOne(size_t size) {
  size_t req = size;
  size_t adv = malloc_good_size(req);
  char* p = (char*)malloc(req);
  size_t usable = moz_malloc_usable_size(p);
  // NB: Using EXPECT here so that we still free the memory on failure.
  EXPECT_EQ(adv, usable) << "malloc_good_size(" << req << ") --> " << adv
                         << "; "
                            "malloc_usable_size("
                         << req << ") --> " << usable;
  free(p);
}

static inline void TestThree(size_t size) {
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

  for (size_t n = 0; n < 16_KiB; n++) ASSERT_NO_FATAL_FAILURE(TestOne(n));

  for (size_t n = 16_KiB; n < 1_MiB; n += 4_KiB)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));

  for (size_t n = 1_MiB; n < 8_MiB; n += 128_KiB)
    ASSERT_NO_FATAL_FAILURE(TestThree(n));
}

static int gStaticVar;

bool InfoEq(jemalloc_ptr_info_t& aInfo, PtrInfoTag aTag, void* aAddr,
            size_t aSize, arena_id_t arenaId) {
  return aInfo.tag == aTag && aInfo.addr == aAddr && aInfo.size == aSize
#ifdef MOZ_DEBUG
         && aInfo.arenaId == arenaId
#endif
      ;
}

bool InfoEqFreedPage(jemalloc_ptr_info_t& aInfo, void* aAddr, size_t aPageSize,
                     arena_id_t arenaId) {
  size_t pageSizeMask = aPageSize - 1;

  return jemalloc_ptr_is_freed_page(&aInfo) &&
         aInfo.addr == (void*)(uintptr_t(aAddr) & ~pageSizeMask) &&
         aInfo.size == aPageSize
#ifdef MOZ_DEBUG
         && aInfo.arenaId == arenaId
#endif
      ;
}

TEST(Jemalloc, PtrInfo)
{
  arena_id_t arenaId = moz_create_arena();
  ASSERT_TRUE(arenaId != 0);

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  jemalloc_ptr_info_t info;
  Vector<char*> small, large, huge;

  // For small (less than half the page size) allocations, test every position
  // within many possible sizes.
  size_t small_max =
      stats.subpage_max ? stats.subpage_max : stats.quantum_wide_max;
  for (size_t n = 0; n <= small_max; n += 8) {
    auto p = (char*)moz_arena_malloc(arenaId, n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(small.append(p));
    for (size_t j = 0; j < usable; j++) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveAlloc, p, usable, arenaId));
    }
  }

  // Similar for large (small_max + 1 KiB .. 1MiB - 8KiB) allocations.
  for (size_t n = small_max + 1_KiB; n <= stats.large_max; n += 1_KiB) {
    auto p = (char*)moz_arena_malloc(arenaId, n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(large.append(p));
    for (size_t j = 0; j < usable; j += 347) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveAlloc, p, usable, arenaId));
    }
  }

  // Similar for huge (> 1MiB - 8KiB) allocations.
  for (size_t n = stats.chunksize; n <= 10_MiB; n += 512_KiB) {
    auto p = (char*)moz_arena_malloc(arenaId, n);
    size_t usable = moz_malloc_size_of(p);
    ASSERT_TRUE(huge.append(p));
    for (size_t j = 0; j < usable; j += 567) {
      jemalloc_ptr_info(&p[j], &info);
      ASSERT_TRUE(InfoEq(info, TagLiveAlloc, p, usable, arenaId));
    }
  }

  // The following loops check freed allocations. We step through the vectors
  // using prime-sized steps, which gives full coverage of the arrays while
  // avoiding deallocating in the same order we allocated.
  size_t len;

  // Free the small allocations and recheck them.
  int isFreedAlloc = 0, isFreedPage = 0;
  len = small.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 19) % len) {
    char* p = small[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k++) {
      jemalloc_ptr_info(&p[k], &info);
      // There are two valid outcomes here.
      if (InfoEq(info, TagFreedAlloc, p, usable, arenaId)) {
        isFreedAlloc++;
      } else if (InfoEqFreedPage(info, &p[k], stats.page_size, arenaId)) {
        isFreedPage++;
      } else {
        ASSERT_TRUE(false);
      }
    }
  }
  // There should be both FreedAlloc and FreedPage results, but a lot more of
  // the former.
  ASSERT_TRUE(isFreedAlloc != 0);
  ASSERT_TRUE(isFreedPage != 0);
  ASSERT_TRUE(isFreedAlloc / isFreedPage > 8);

  // Free the large allocations and recheck them.
  len = large.length();
  for (size_t i = 0, j = 0; i < len; i++, j = (j + 31) % len) {
    char* p = large[j];
    size_t usable = moz_malloc_size_of(p);
    free(p);
    for (size_t k = 0; k < usable; k += 357) {
      jemalloc_ptr_info(&p[k], &info);
      ASSERT_TRUE(InfoEqFreedPage(info, &p[k], stats.page_size, arenaId));
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
      ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));
    }
  }

  // Null ptr.
  jemalloc_ptr_info(nullptr, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Near-null ptr.
  jemalloc_ptr_info((void*)0x123, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Maximum address.
  jemalloc_ptr_info((void*)uintptr_t(-1), &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Stack memory.
  int stackVar;
  jemalloc_ptr_info(&stackVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Code memory.
  jemalloc_ptr_info((const void*)&jemalloc_ptr_info, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Static memory.
  jemalloc_ptr_info(&gStaticVar, &info);
  ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));

  // Chunk header.
  UniquePtr<int> p = MakeUnique<int>();
  size_t chunksizeMask = stats.chunksize - 1;
  char* chunk = (char*)(uintptr_t(p.get()) & ~chunksizeMask);
  size_t chunkHeaderSize = stats.chunksize - stats.large_max - stats.page_size;
  for (size_t i = 0; i < chunkHeaderSize; i += 64) {
    jemalloc_ptr_info(&chunk[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));
  }

  // Run header.
  size_t page_sizeMask = stats.page_size - 1;
  char* run = (char*)(uintptr_t(p.get()) & ~page_sizeMask);
  for (size_t i = 0; i < 4 * sizeof(void*); i++) {
    jemalloc_ptr_info(&run[i], &info);
    ASSERT_TRUE(InfoEq(info, TagUnknown, nullptr, 0U, 0U));
  }

  // Entire chunk. It's impossible to check what is put into |info| for all of
  // these addresses; this is more about checking that we don't crash.
  for (size_t i = 0; i < stats.chunksize; i += 256) {
    jemalloc_ptr_info(&chunk[i], &info);
  }

  moz_dispose_arena(arenaId);
}

size_t sSizes[] = {1,      42,      79,      918,     1.4_KiB,
                   73_KiB, 129_KiB, 1.1_MiB, 2.6_MiB, 5.1_MiB};

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

  // Avoid death tests adding some unnecessary (long) delays.
  SAVE_GDB_SLEEP_LOCAL();

  // Can't use an arena after it's disposed.
  // ASSERT_DEATH_WRAP(moz_arena_malloc(arena, 80), "");

  // Arena id 0 can't be used to somehow get to the main arena.
  ASSERT_DEATH_WRAP(moz_arena_malloc(0, 80), "");

  arena = moz_create_arena();
  arena_id_t arena2 = moz_create_arena();
  // Ensure arena2 is used to prevent OSX errors:
  (void)arena2;

  // For convenience, realloc can also be used to reallocate arena pointers.
  // The result should be in the same arena. Test various size class
  // transitions.
  for (size_t from_size : sSizes) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
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

  RESTORE_GDB_SLEEP_LOCAL();
}

// Check that a buffer aPtr is entirely filled with a given character from
// aOffset to aSize. For faster comparison, the caller is required to fill a
// reference buffer with the wanted character, and give the size of that
// reference buffer.
static void bulk_compare(char* aPtr, size_t aOffset, size_t aSize,
                         char* aReference, size_t aReferenceSize) {
  for (size_t i = aOffset; i < aSize; i += aReferenceSize) {
    size_t length = std::min(aSize - i, aReferenceSize);
    if (memcmp(aPtr + i, aReference, length)) {
      // We got a mismatch, we now want to report more precisely where.
      for (size_t j = i; j < i + length; j++) {
        ASSERT_EQ(aPtr[j], *aReference);
      }
    }
  }
}

// A range iterator for size classes between two given values.
class SizeClassesBetween {
 public:
  SizeClassesBetween(size_t aStart, size_t aEnd) : mStart(aStart), mEnd(aEnd) {}

  class Iterator {
   public:
    explicit Iterator(size_t aValue) : mValue(malloc_good_size(aValue)) {}

    operator size_t() const { return mValue; }
    size_t operator*() const { return mValue; }
    Iterator& operator++() {
      mValue = malloc_good_size(mValue + 1);
      return *this;
    }

   private:
    size_t mValue;
  };

  Iterator begin() { return Iterator(mStart); }
  Iterator end() { return Iterator(mEnd); }

 private:
  size_t mStart, mEnd;
};

#define ALIGNMENT_CEILING(s, alignment) \
  (((s) + ((alignment)-1)) & (~((alignment)-1)))

#define ALIGNMENT_FLOOR(s, alignment) ((s) & (~((alignment)-1)))

static bool IsSameRoundedHugeClass(size_t aSize1, size_t aSize2,
                                   jemalloc_stats_t& aStats) {
  return (aSize1 > aStats.large_max && aSize2 > aStats.large_max &&
          ALIGNMENT_CEILING(aSize1 + aStats.page_size, aStats.chunksize) ==
              ALIGNMENT_CEILING(aSize2 + aStats.page_size, aStats.chunksize));
}

static bool CanReallocInPlace(size_t aFromSize, size_t aToSize,
                              jemalloc_stats_t& aStats) {
  // PHC allocations must be disabled because PHC reallocs differently to
  // mozjemalloc.
#ifdef MOZ_PHC
  MOZ_RELEASE_ASSERT(!mozilla::phc::IsPHCEnabledOnCurrentThread());
#endif

  if (aFromSize == malloc_good_size(aToSize)) {
    // Same size class: in-place.
    return true;
  }
  if (aFromSize >= aStats.page_size && aFromSize <= aStats.large_max &&
      aToSize >= aStats.page_size && aToSize <= aStats.large_max) {
    // Any large class to any large class: in-place when there is space to.
    return true;
  }
  if (IsSameRoundedHugeClass(aFromSize, aToSize, aStats)) {
    // Huge sizes that round up to the same multiple of the chunk size:
    // in-place.
    return true;
  }
  return false;
}

TEST(Jemalloc, InPlace)
{
  // Disable PHC allocations for this test, because CanReallocInPlace() isn't
  // valid for PHC allocations.
  AutoDisablePHCOnCurrentThread disable;

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Using a separate arena, which is always emptied after an iteration, ensures
  // that in-place reallocation happens in all cases it can happen. This test is
  // intended for developers to notice they may have to adapt other tests if
  // they change the conditions for in-place reallocation.
  arena_id_t arena = moz_create_arena();

  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
      char* ptr = (char*)moz_arena_malloc(arena, from_size);
      char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
      if (CanReallocInPlace(from_size, to_size, stats)) {
        EXPECT_EQ(ptr, ptr2);
      } else {
        EXPECT_NE(ptr, ptr2);
      }
      moz_arena_free(arena, ptr2);
    }
  }

  moz_dispose_arena(arena);
}

// Bug 1474254: disable this test for windows ccov builds because it leads to
// timeout.
#if !defined(XP_WIN) || !defined(MOZ_CODE_COVERAGE)
TEST(Jemalloc, JunkPoison)
{
  // Disable PHC allocations for this test, because CanReallocInPlace() isn't
  // valid for PHC allocations, and the testing UAFs aren't valid.
  AutoDisablePHCOnCurrentThread disable;

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Avoid death tests adding some unnecessary (long) delays.
  SAVE_GDB_SLEEP_LOCAL();

  // Create buffers in a separate arena, for faster comparisons with
  // bulk_compare.
  arena_id_t buf_arena = moz_create_arena();
  char* junk_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  // Depending on its configuration, the allocator will either fill the
  // requested allocation with the junk byte (0xe4) or with zeroes, or do
  // nothing, in which case, since we're allocating in a fresh arena,
  // we'll be getting zeroes.
  char junk = stats.opt_junk ? '\xe4' : '\0';
  for (size_t i = 0; i < stats.page_size; i++) {
    ASSERT_EQ(junk_buf[i], junk);
  }

  char* poison_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  memset(poison_buf, 0xe5, stats.page_size);

  static const char fill = 0x42;
  char* fill_buf = (char*)moz_arena_malloc(buf_arena, stats.page_size);
  memset(fill_buf, fill, stats.page_size);

  arena_params_t params;
  // Allow as many dirty pages in the arena as possible, so that purge never
  // happens in it. Purge breaks some of the tests below randomly depending on
  // what other things happen on other threads.
  params.mMaxDirty = size_t(-1);
  arena_id_t arena = moz_create_arena_with_params(&params);

  // Mozjemalloc is configured to only poison the first four cache lines.
  const size_t poison_check_len = 256;

  // Allocating should junk the buffer, and freeing should poison the buffer.
  for (size_t size : sSizes) {
    if (size <= stats.large_max) {
      SCOPED_TRACE(testing::Message() << "size = " << size);
      char* buf = (char*)moz_arena_malloc(arena, size);
      size_t allocated = moz_malloc_usable_size(buf);
      if (stats.opt_junk || stats.opt_zero) {
        ASSERT_NO_FATAL_FAILURE(
            bulk_compare(buf, 0, allocated, junk_buf, stats.page_size));
      }
      moz_arena_free(arena, buf);
      // We purposefully do a use-after-free here, to check that the data was
      // poisoned.
      ASSERT_NO_FATAL_FAILURE(
          bulk_compare(buf, 0, std::min(allocated, poison_check_len),
                       poison_buf, stats.page_size));
    }
  }

  // Shrinking in the same size class should be in place and poison between the
  // new allocation size and the old one.
  size_t prev = 0;
  for (size_t size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "size = " << size);
    SCOPED_TRACE(testing::Message() << "prev = " << prev);
    char* ptr = (char*)moz_arena_malloc(arena, size);
    memset(ptr, fill, moz_malloc_usable_size(ptr));
    char* ptr2 = (char*)moz_arena_realloc(arena, ptr, prev + 1);
    ASSERT_EQ(ptr, ptr2);
    ASSERT_NO_FATAL_FAILURE(
        bulk_compare(ptr, 0, prev + 1, fill_buf, stats.page_size));
    ASSERT_NO_FATAL_FAILURE(bulk_compare(ptr, prev + 1,
                                         std::min(size, poison_check_len),
                                         poison_buf, stats.page_size));
    moz_arena_free(arena, ptr);
    prev = size;
  }

  // In-place realloc should junk the new bytes when growing and poison the old
  // bytes when shrinking.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
      if (CanReallocInPlace(from_size, to_size, stats)) {
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, moz_malloc_usable_size(ptr));
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_EQ(ptr, ptr2);
        // Shrinking allocation
        if (from_size >= to_size) {
          ASSERT_NO_FATAL_FAILURE(
              bulk_compare(ptr, 0, to_size, fill_buf, stats.page_size));
          // Huge allocations have guards and will crash when accessing
          // beyond the valid range.
          if (to_size > stats.large_max) {
            size_t page_limit = ALIGNMENT_CEILING(to_size, stats.page_size);
            ASSERT_NO_FATAL_FAILURE(bulk_compare(
                ptr, to_size, std::min(page_limit, poison_check_len),
                poison_buf, stats.page_size));
            ASSERT_DEATH_WRAP(ptr[page_limit] = 0, "");
          } else {
            ASSERT_NO_FATAL_FAILURE(bulk_compare(
                ptr, to_size, std::min(from_size, poison_check_len), poison_buf,
                stats.page_size));
          }
        } else {
          // Enlarging allocation
          ASSERT_NO_FATAL_FAILURE(
              bulk_compare(ptr, 0, from_size, fill_buf, stats.page_size));
          if (stats.opt_junk || stats.opt_zero) {
            ASSERT_NO_FATAL_FAILURE(bulk_compare(ptr, from_size, to_size,
                                                 junk_buf, stats.page_size));
          }
          // Huge allocation, so should have a guard page following
          if (to_size > stats.large_max) {
            ASSERT_DEATH_WRAP(
                ptr[ALIGNMENT_CEILING(to_size, stats.page_size)] = 0, "");
          }
        }
        moz_arena_free(arena, ptr2);
      }
    }
  }

  // Growing to a different size class should poison the old allocation,
  // preserve the original bytes, and junk the new bytes in the new allocation.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      if (from_size < to_size && malloc_good_size(to_size) != from_size &&
          !IsSameRoundedHugeClass(from_size, to_size, stats)) {
        SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, moz_malloc_usable_size(ptr));
        // Avoid in-place realloc by allocating a buffer, expecting it to be
        // right after the buffer we just received. Buffers smaller than the
        // page size and exactly or larger than the size of the largest large
        // size class can't be reallocated in-place.
        char* avoid_inplace = nullptr;
        if (from_size >= stats.page_size && from_size < stats.large_max) {
          avoid_inplace = (char*)moz_arena_malloc(arena, stats.page_size);
          ASSERT_EQ(ptr + from_size, avoid_inplace);
        }
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_NE(ptr, ptr2);
        if (from_size <= stats.large_max) {
          ASSERT_NO_FATAL_FAILURE(
              bulk_compare(ptr, 0, std::min(from_size, poison_check_len),
                           poison_buf, stats.page_size));
        }
        ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr2, 0, from_size, fill_buf, stats.page_size));
        if (stats.opt_junk || stats.opt_zero) {
          size_t rounded_to_size = malloc_good_size(to_size);
          ASSERT_NE(to_size, rounded_to_size);
          ASSERT_NO_FATAL_FAILURE(bulk_compare(ptr2, from_size, rounded_to_size,
                                               junk_buf, stats.page_size));
        }
        moz_arena_free(arena, ptr2);
        moz_arena_free(arena, avoid_inplace);
      }
    }
  }

  // Shrinking to a different size class should poison the old allocation,
  // preserve the original bytes, and junk the extra bytes in the new
  // allocation.
  for (size_t from_size : SizeClassesBetween(1, 2 * stats.chunksize)) {
    SCOPED_TRACE(testing::Message() << "from_size = " << from_size);
    for (size_t to_size : sSizes) {
      if (from_size > to_size &&
          !CanReallocInPlace(from_size, to_size, stats)) {
        SCOPED_TRACE(testing::Message() << "to_size = " << to_size);
        char* ptr = (char*)moz_arena_malloc(arena, from_size);
        memset(ptr, fill, from_size);
        char* ptr2 = (char*)moz_arena_realloc(arena, ptr, to_size);
        ASSERT_NE(ptr, ptr2);
        if (from_size <= stats.large_max) {
          ASSERT_NO_FATAL_FAILURE(
              bulk_compare(ptr, 0, std::min(from_size, poison_check_len),
                           poison_buf, stats.page_size));
        }
        ASSERT_NO_FATAL_FAILURE(
            bulk_compare(ptr2, 0, to_size, fill_buf, stats.page_size));
        if (stats.opt_junk || stats.opt_zero) {
          size_t rounded_to_size = malloc_good_size(to_size);
          ASSERT_NE(to_size, rounded_to_size);
          ASSERT_NO_FATAL_FAILURE(bulk_compare(ptr2, from_size, rounded_to_size,
                                               junk_buf, stats.page_size));
        }
        moz_arena_free(arena, ptr2);
      }
    }
  }

  moz_dispose_arena(arena);

  moz_arena_free(buf_arena, poison_buf);
  moz_arena_free(buf_arena, junk_buf);
  moz_arena_free(buf_arena, fill_buf);
  moz_dispose_arena(buf_arena);

  RESTORE_GDB_SLEEP_LOCAL();
}
#endif  // !defined(XP_WIN) || !defined(MOZ_CODE_COVERAGE)

TEST(Jemalloc, TrailingGuard)
{
  // Disable PHC allocations for this test, because even a single PHC
  // allocation occurring can throw it off.
  AutoDisablePHCOnCurrentThread disable;

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Avoid death tests adding some unnecessary (long) delays.
  SAVE_GDB_SLEEP_LOCAL();

  arena_id_t arena = moz_create_arena();
  ASSERT_TRUE(arena != 0);

  // Do enough large allocations to fill a chunk, and then one additional one,
  // and check that the guard page is still present after the one-but-last
  // allocation, i.e. that we didn't allocate the guard.
  Vector<void*> ptr_list;
  for (size_t cnt = 0; cnt < stats.large_max / stats.page_size; cnt++) {
    void* ptr = moz_arena_malloc(arena, stats.page_size);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_TRUE(ptr_list.append(ptr));
  }

  void* last_ptr_in_chunk = ptr_list[ptr_list.length() - 1];
  void* extra_ptr = moz_arena_malloc(arena, stats.page_size);
  void* guard_page = (void*)ALIGNMENT_CEILING(
      (uintptr_t)last_ptr_in_chunk + stats.page_size, stats.page_size);
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info(guard_page, &info);
  ASSERT_TRUE(jemalloc_ptr_is_freed_page(&info));

  ASSERT_DEATH_WRAP(*(char*)guard_page = 0, "");

  for (void* ptr : ptr_list) {
    moz_arena_free(arena, ptr);
  }
  moz_arena_free(arena, extra_ptr);

  moz_dispose_arena(arena);

  RESTORE_GDB_SLEEP_LOCAL();
}

TEST(Jemalloc, LeadingGuard)
{
  // Disable PHC allocations for this test, because even a single PHC
  // allocation occurring can throw it off.
  AutoDisablePHCOnCurrentThread disable;

  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Avoid death tests adding some unnecessary (long) delays.
  SAVE_GDB_SLEEP_LOCAL();

  arena_id_t arena = moz_create_arena();
  ASSERT_TRUE(arena != 0);

  // Do a simple normal allocation, but force all the allocation space
  // in the chunk to be used up. This allows us to check that we get
  // the safe area right in the logic that follows (all memory will be
  // committed and initialized), and it forces this pointer to the start
  // of the zone to sit at the very start of the usable chunk area.
  void* ptr = moz_arena_malloc(arena, stats.large_max);
  ASSERT_TRUE(ptr != nullptr);
  // If ptr is chunk-aligned, the above allocation went wrong.
  void* chunk_start = (void*)ALIGNMENT_FLOOR((uintptr_t)ptr, stats.chunksize);
  ASSERT_NE((uintptr_t)ptr, (uintptr_t)chunk_start);
  // If ptr is 1 page after the chunk start (so right after the header),
  // we must have missed adding the guard page.
  ASSERT_NE((uintptr_t)ptr, (uintptr_t)chunk_start + stats.page_size);
  // The actual start depends on the amount of metadata versus the page
  // size, so we can't check equality without pulling in too many
  // implementation details.

  // Guard page should be right before data area
  void* guard_page = (void*)(((uintptr_t)ptr) - sizeof(void*));
  jemalloc_ptr_info_t info;
  jemalloc_ptr_info(guard_page, &info);
  ASSERT_TRUE(info.tag == TagUnknown);
  ASSERT_DEATH_WRAP(*(char*)guard_page = 0, "");

  moz_arena_free(arena, ptr);
  moz_dispose_arena(arena);

  RESTORE_GDB_SLEEP_LOCAL();
}

TEST(Jemalloc, DisposeArena)
{
  jemalloc_stats_t stats;
  jemalloc_stats(&stats);

  // Avoid death tests adding some unnecessary (long) delays.
  SAVE_GDB_SLEEP_LOCAL();

  arena_id_t arena = moz_create_arena();
  void* ptr = moz_arena_malloc(arena, 42);
  // Disposing of the arena when it's not empty is a MOZ_CRASH-worthy error.
  ASSERT_DEATH_WRAP(moz_dispose_arena(arena), "");
  moz_arena_free(arena, ptr);
  moz_dispose_arena(arena);

  arena = moz_create_arena();
  ptr = moz_arena_malloc(arena, stats.page_size * 2);
  // Disposing of the arena when it's not empty is a MOZ_CRASH-worthy error.
  ASSERT_DEATH_WRAP(moz_dispose_arena(arena), "");
  moz_arena_free(arena, ptr);
  moz_dispose_arena(arena);

  arena = moz_create_arena();
  ptr = moz_arena_malloc(arena, stats.chunksize * 2);
#ifdef MOZ_DEBUG
  // On debug builds, we do the expensive check that arenas are empty.
  ASSERT_DEATH_WRAP(moz_dispose_arena(arena), "");
  moz_arena_free(arena, ptr);
  moz_dispose_arena(arena);
#else
  // Currently, the allocator can't trivially check whether the arena is empty
  // of huge allocations, so disposing of it works.
  moz_dispose_arena(arena);
  // But trying to free a pointer that belongs to it will MOZ_CRASH.
  ASSERT_DEATH_WRAP(free(ptr), "");
  // Likewise for realloc
  ASSERT_DEATH_WRAP(ptr = realloc(ptr, stats.chunksize * 3), "");
#endif

  // Using the arena after it's been disposed of is MOZ_CRASH-worthy.
  ASSERT_DEATH_WRAP(moz_arena_malloc(arena, 42), "");

  RESTORE_GDB_SLEEP_LOCAL();
}
