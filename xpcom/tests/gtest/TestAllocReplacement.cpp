/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/mozalloc.h"
#include "mozilla/ScopeExit.h"
#include "nsCOMPtr.h"
#include "nsIMemoryReporter.h"
#include "nsServiceManagerUtils.h"
#include "gtest/gtest.h"

// We want to ensure that various functions are hooked properly and that
// allocations are getting routed through jemalloc.  The strategy
// pursued below relies on jemalloc's statistics tracking: we measure
// the size of the jemalloc heap using nsIMemoryReporterManager,
// allocate a chunk of memory with whatever function is supposed to be
// hooked, and then ask for the size of the jemalloc heap again.  If the
// function has been hooked correctly, then the heap size should be
// different between the two measurements.  We can also check the
// hooking of |free| and similar functions: once we free() the returned
// pointer, we can measure the jemalloc heap size again, expecting it to
// be identical to the size prior to the allocation.
//
// If we're not using jemalloc, then nsIMemoryReporterManager will
// simply report an error, and we will ignore the entire test.
//
// This strategy is not perfect: it relies on GTests being
// single-threaded, which they are, and no other threads doing
// allocation during the test, which is uncertain, as XPCOM has started
// up during gtests, and who knows what might be going on behind the
// scenes.  This latter assumption, however, does not seem to be a
// problem in practice.
#if defined(MOZ_MEMORY)
#  define ALLOCATION_ASSERT(b) ASSERT_TRUE((b))
#else
#  define ALLOCATION_ASSERT(b) (void)(b)
#endif

#define ASSERT_ALLOCATION_HAPPENED(lambda) \
  ALLOCATION_ASSERT(ValidateHookedAllocation(lambda, free));

// We do run the risk of OOM'ing when we allocate something...all we can
// do is try to allocate something so small that OOM'ing is unlikely.
const size_t kAllocAmount = 16;

// We declare this function MOZ_NEVER_INLINE to work around optimizing
// compilers.  If we permitted inlining here, then the compiler might
// inline both this function and the calls to the function pointers we
// pass in, giving something like:
//
//   void* p = malloc(...);
//   ...do nothing with p except check nullptr-ness...
//   free(p);
//
// and the optimizer can delete the calls to malloc and free entirely,
// which would make checking that the jemalloc heap had never changed
// difficult.
static MOZ_NEVER_INLINE bool ValidateHookedAllocation(
    void* (*aAllocator)(void), void (*aFreeFunction)(void*)) {
  nsCOMPtr<nsIMemoryReporterManager> manager =
      do_GetService("@mozilla.org/memory-reporter-manager;1");

  int64_t before = 0;
  nsresult rv = manager->GetHeapAllocated(&before);
  if (NS_FAILED(rv)) {
    return false;
  }

  {
    void* p = aAllocator();

    if (!p) {
      return false;
    }

    int64_t after = 0;
    rv = manager->GetHeapAllocated(&after);

    // Regardless of whether that call succeeded or failed, we are done with
    // the allocated buffer now.
    aFreeFunction(p);

    if (NS_FAILED(rv)) {
      return false;
    }

    // Verify that our heap stats have changed.
    if ((before + int64_t(kAllocAmount)) != after) {
      return false;
    }
  }

  // Verify that freeing the allocated pointer resets our heap to what it
  // was before.
  int64_t after = 0;
  rv = manager->GetHeapAllocated(&after);
  if (NS_FAILED(rv)) {
    return false;
  }

  return before == after;
}

// We use the "*DeathTest" suffix for all tests in this file to ensure they
// run before other GTests. As noted at the top, this is important because
// other tests might spawn threads that interfere with heap memory
// measurements.
//
// See
// <https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#death-tests>
// for more information about death tests in the GTest framework.
TEST(AllocReplacementDeathTest, malloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return malloc(kAllocAmount); });
}

// See above for an explanation of the "*DeathTest" suffix used here.
TEST(AllocReplacementDeathTest, calloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return calloc(1, kAllocAmount); });
}

// See above for an explanation of the "*DeathTest" suffix used here.
TEST(AllocReplacementDeathTest, realloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return realloc(nullptr, kAllocAmount); });
}

#if defined(HAVE_POSIX_MEMALIGN)
// See above for an explanation of the "*DeathTest" suffix used here.
TEST(AllocReplacementDeathTest, posix_memalign_check)
{
  ASSERT_ALLOCATION_HAPPENED([] {
    void* p = nullptr;
    int result = posix_memalign(&p, sizeof(void*), kAllocAmount);
    if (result != 0) {
      return static_cast<void*>(nullptr);
    }
    return p;
  });
}
#endif

#if defined(XP_WIN)
#  include <windows.h>

#  undef ASSERT_ALLOCATION_HAPPENED
#  define ASSERT_ALLOCATION_HAPPENED(lambda)    \
    ALLOCATION_ASSERT(ValidateHookedAllocation( \
        lambda, [](void* p) { HeapFree(GetProcessHeap(), 0, p); }));

// See above for an explanation of the "*DeathTest" suffix used here.
TEST(AllocReplacementDeathTest, HeapAlloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] {
    HANDLE h = GetProcessHeap();
    return HeapAlloc(h, 0, kAllocAmount);
  });
}

// See above for an explanation of the "*DeathTest" suffix used here.
TEST(AllocReplacementDeathTest, HeapReAlloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] {
    HANDLE h = GetProcessHeap();
    void* p = HeapAlloc(h, 0, kAllocAmount / 2);

    if (!p) {
      return static_cast<void*>(nullptr);
    }

    return HeapReAlloc(h, 0, p, kAllocAmount);
  });
}
#endif
