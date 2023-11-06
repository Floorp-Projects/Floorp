/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PHC_h
#define PHC_h

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include <stdint.h>
#include <stdlib.h>

#include "mozmemory_wrap.h"

namespace mozilla {
namespace phc {

// Note: a stack trace may have no frames due to a collection problem.
//
// Also note: a more compact stack trace representation could be achieved with
// some effort.
struct StackTrace {
 public:
  static const size_t kMaxFrames = 16;

  // The number of PCs in the stack trace.
  size_t mLength;

  // The PCs in the stack trace. Only the first mLength are initialized.
  const void* mPcs[kMaxFrames];

 public:
  StackTrace() : mLength(0) {}
};

// Info from PHC about an address in memory.
class AddrInfo {
 public:
  enum class Kind {
    // The address is not in PHC-managed memory.
    Unknown = 0,

    // The address is within a PHC page that has never been allocated. A crash
    // involving such an address is unlikely in practice, because it would
    // require the crash to happen quite early.
    NeverAllocatedPage = 1,

    // The address is within a PHC page that is in use.
    InUsePage = 2,

    // The address is within a PHC page that has been allocated and then freed.
    // A crash involving such an address most likely indicates a
    // use-after-free. (A sufficiently wild write -- e.g. a large buffer
    // overflow -- could also trigger it, but this is less likely.)
    FreedPage = 3,

    // The address is within a PHC guard page. A crash involving such an
    // address most likely indicates a buffer overflow. (Again, a sufficiently
    // wild write could unluckily trigger it, but this is less likely.)
    GuardPage = 4,
  };

  // The page kind.
  Kind mKind;

  // The starting address of the allocation.
  // - Unknown | NeverAllocatedPage: nullptr.
  // - InUsePage | FreedPage: the address of the allocation within the page.
  // - GuardPage: the mBaseAddr value from the preceding allocation page.
  const void* mBaseAddr;

  // The usable size, which could be bigger than the requested size.
  // - Unknown | NeverAllocatePage: 0.
  // - InUsePage | FreedPage: the usable size of the allocation within the page.
  // - GuardPage: the mUsableSize value from the preceding allocation page.
  size_t mUsableSize;

  // The allocation stack.
  // - Unknown | NeverAllocatedPage: Nothing.
  // - InUsePage | FreedPage: Some.
  // - GuardPage: the mAllocStack value from the preceding allocation page.
  mozilla::Maybe<StackTrace> mAllocStack;

  // The free stack.
  // - Unknown | NeverAllocatedPage | InUsePage: Nothing.
  // - FreedPage: Some.
  // - GuardPage: the mFreeStack value from the preceding allocation page.
  mozilla::Maybe<StackTrace> mFreeStack;

  // True if PHC was locked and therefore we couldn't retrive some infomation.
  bool mPhcWasLocked = false;

  // Default to no PHC info.
  AddrInfo() : mKind(Kind::Unknown), mBaseAddr(nullptr), mUsableSize(0) {}
};

// Global instance that is retrieved by the process generating the crash report
extern AddrInfo gAddrInfo;

// If this is a PHC-handled address, return true, and if an AddrInfo is
// provided, fill in all of its fields. Otherwise, return false and leave
// AddrInfo unchanged.
MOZ_JEMALLOC_API bool IsPHCAllocation(const void*, AddrInfo*);

// Disable PHC allocations on the current thread. Only useful for tests. Note
// that PHC deallocations will still occur as needed.
MOZ_JEMALLOC_API void DisablePHCOnCurrentThread();

// Re-enable PHC allocations on the current thread. Only useful for tests.
MOZ_JEMALLOC_API void ReenablePHCOnCurrentThread();

// Test whether PHC allocations are enabled on the current thread. Only
// useful for tests.
MOZ_JEMALLOC_API bool IsPHCEnabledOnCurrentThread();

// PHC has three different states:
//  * Not compiled in
//  * OnlyFree         - The memory allocator is hooked but new allocations
//                       requests will be forwarded to mozjemalloc, free() will
//                       correctly free any PHC allocations and realloc() will
//                       "move" PHC allocations to mozjemalloc allocations.
//  * Enabled          - Full use.
enum PHCState {
  OnlyFree,
  Enabled,
};

MOZ_JEMALLOC_API void SetPHCState(PHCState aState);

struct MemoryUsage {
  // The amount of memory used for PHC metadata, eg information about each
  // allocation including stacks.
  size_t mMetadataBytes = 0;

  // The amount of memory lost due to rounding allocation sizes up to the
  // nearest page.  AKA internal fragmentation.
  size_t mFragmentationBytes = 0;
};

MOZ_JEMALLOC_API void PHCMemoryUsage(MemoryUsage& aMemoryUsage);

struct PHCStats {
  size_t mSlotsAllocated = 0;
  size_t mSlotsFreed = 0;
  size_t mSlotsUnused = 0;
};

// Return PHC memory usage information by filling in the supplied structure.
MOZ_JEMALLOC_API void GetPHCStats(PHCStats& aStats);

}  // namespace phc
}  // namespace mozilla

#endif /* PHC_h */
