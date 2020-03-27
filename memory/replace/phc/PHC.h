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

  // Default to no PHC info.
  AddrInfo()
      : mKind(Kind::Unknown),
        mBaseAddr(nullptr),
        mUsableSize(0),
        mAllocStack(),
        mFreeStack() {}
};

}  // namespace phc
}  // namespace mozilla

#endif /* PHC_h */
