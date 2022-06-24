/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozmemory.h"
#include "gtest/gtest.h"

// We want to ensure that various functions are hooked properly and that
// allocations are getting routed through jemalloc.  The strategy
// pursued below relies on jemalloc_info_ptr knowing about the pointers
// returned by the allocator.  If the function has been hooked correctly,
// then jemalloc_info_ptr returns a TagLiveAlloc tag, or TagUnknown
// otherwise.
// We could also check the hooking of |free| and similar functions: once
// we free() the returned pointer, jemalloc_info_ptr would return a tag
// that is not TagLiveAlloc. However, in the GTests environment, with
// other threads running in the background, it is possible for some of
// them to get a new allocation at the same location we just freed, and
// jemalloc_info_ptr would return a TagLiveAlloc tag.

#define ASSERT_ALLOCATION_HAPPENED(lambda) \
  ASSERT_TRUE(ValidateHookedAllocation(lambda, free));

// We do run the risk of OOM'ing when we allocate something...all we can
// do is try to allocate something so small that OOM'ing is unlikely.
const size_t kAllocAmount = 16;

static bool ValidateHookedAllocation(void* (*aAllocator)(void),
                                     void (*aFreeFunction)(void*)) {
  void* p = aAllocator();

  if (!p) {
    return false;
  }

  jemalloc_ptr_info_t info;
  jemalloc_ptr_info(p, &info);

  // Regardless of whether that call succeeded or failed, we are done with
  // the allocated buffer now.
  aFreeFunction(p);

  return (info.tag == PtrInfoTag::TagLiveAlloc);
}

TEST(AllocReplacement, malloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return malloc(kAllocAmount); });
}

TEST(AllocReplacement, calloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return calloc(1, kAllocAmount); });
}

TEST(AllocReplacement, realloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] { return realloc(nullptr, kAllocAmount); });
}

#if defined(HAVE_POSIX_MEMALIGN)
TEST(AllocReplacement, posix_memalign_check)
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
#  define ASSERT_ALLOCATION_HAPPENED(lambda) \
    ASSERT_TRUE(ValidateHookedAllocation(    \
        lambda, [](void* p) { HeapFree(GetProcessHeap(), 0, p); }));

TEST(AllocReplacement, HeapAlloc_check)
{
  ASSERT_ALLOCATION_HAPPENED([] {
    HANDLE h = GetProcessHeap();
    return HeapAlloc(h, 0, kAllocAmount);
  });
}

TEST(AllocReplacement, HeapReAlloc_check)
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
