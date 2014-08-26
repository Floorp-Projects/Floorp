/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CountingAllocatorBase_h
#define CountingAllocatorBase_h

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "nsIMemoryReporter.h"

namespace mozilla {

// This CRTP class handles several details of wrapping allocators and should
// be preferred to manually counting with MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC
// and MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE.  The typical use is in a memory
// reporter for a particular third party library:
//
//   class MyMemoryReporter : public CountingAllocatorBase<MyMemoryReporter>
//   {
//     ...
//     NS_IMETHODIMP
//     CollectReports(nsIHandleReportCallback* aHandleReport,
//                    nsISupports* aData, bool aAnonymize)
//     {
//        return MOZ_COLLECT_REPORT(
//          "explicit/path/to/somewhere", KIND_HEAP, UNITS_BYTES,
//          MemoryAllocated(),
//          "A description of what we are reporting."
//     }
//   };
//
//   ...somewhere later in the code...
//   SetThirdPartyMemoryFunctions(MyMemoryReporter::CountingAlloc,
//                                MyMemoryReporter::CountingFree);
template<typename T>
class CountingAllocatorBase
{
public:
  CountingAllocatorBase()
  {
#ifdef DEBUG
    // There must be only one instance of this class, due to |sAmount| being
    // static.
    static bool hasRun = false;
    MOZ_ASSERT(!hasRun);
    hasRun = true;
#endif
  }

  static size_t
  MemoryAllocated()
  {
    return sAmount;
  }

  static void*
  CountingMalloc(size_t size)
  {
    void* p = malloc(size);
    sAmount += MallocSizeOfOnAlloc(p);
    return p;
  }

  static void*
  CountingCalloc(size_t nmemb, size_t size)
  {
    void* p = calloc(nmemb, size);
    sAmount += MallocSizeOfOnAlloc(p);
    return p;
  }

  static void*
  CountingRealloc(void* p, size_t size)
  {
    size_t oldsize = MallocSizeOfOnFree(p);
    void *pnew = realloc(p, size);
    if (pnew) {
      size_t newsize = MallocSizeOfOnAlloc(pnew);
      sAmount += newsize - oldsize;
    } else if (size == 0) {
      // We asked for a 0-sized (re)allocation of some existing pointer
      // and received NULL in return.  0-sized allocations are permitted
      // to either return NULL or to allocate a unique object per call (!).
      // For a malloc implementation that chooses the second strategy,
      // that allocation may fail (unlikely, but possible).
      //
      // Given a NULL return value and an allocation size of 0, then, we
      // don't know if that means the original pointer was freed or if
      // the allocation of the unique object failed.  If the original
      // pointer was freed, then we have nothing to do here.  If the
      // allocation of the unique object failed, the original pointer is
      // still valid and we ought to undo the decrement from above.
      // However, we have no way of knowing how the underlying realloc
      // implementation is behaving.  Assuming that the original pointer
      // was freed is the safest course of action.  We do, however, need
      // to note that we freed memory.
      sAmount -= oldsize;
    } else {
      // realloc failed.  The amount allocated hasn't changed.
    }
    return pnew;
  }

  // Some library code expects that realloc(x, 0) will free x, which is not
  // the behavior of the version of jemalloc we're using, so this wrapped
  // version of realloc is needed.
  static void*
  CountingFreeingRealloc(void* p, size_t size)
  {
    if (size == 0) {
      CountingFree(p);
      return nullptr;
    }
    return CountingRealloc(p, size);
  }

  static void
  CountingFree(void* p)
  {
    sAmount -= MallocSizeOfOnFree(p);
    free(p);
  }

private:
  // |sAmount| can be (implicitly) accessed by multiple threads, so it
  // must be thread-safe.
  static Atomic<size_t> sAmount;

  MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC(MallocSizeOfOnAlloc)
  MOZ_DEFINE_MALLOC_SIZE_OF_ON_FREE(MallocSizeOfOnFree)
};

} // namespace mozilla

#endif // CountingAllocatorBase_h
