/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PseudoStack_h
#define PseudoStack_h

#include "mozilla/ArrayUtils.h"
#include "js/ProfilingStack.h"
#include "nsISupportsImpl.h"  // for MOZ_COUNT_{CTOR,DTOR}

#include <stdlib.h>
#include <stdint.h>

#include <algorithm>

// The PseudoStack members are read by signal handlers, so the mutation of them
// needs to be signal-safe.
class PseudoStack
{
public:
  PseudoStack()
    : mStackPointer(0)
  {
    MOZ_COUNT_CTOR(PseudoStack);
  }

  ~PseudoStack()
  {
    MOZ_COUNT_DTOR(PseudoStack);

    // The label macros keep a reference to the PseudoStack to avoid a TLS
    // access. If these are somehow not all cleared we will get a
    // use-after-free so better to crash now.
    MOZ_RELEASE_ASSERT(mStackPointer == 0);
  }

  void push(const char* aName, js::ProfileEntry::Category aCategory,
            void* aStackAddress, bool aCopy, uint32_t line,
            const char* aDynamicString)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    volatile js::ProfileEntry& entry = mStack[int(mStackPointer)];

    // Make sure we increment the pointer after the name has been written such
    // that mStack is always consistent.
    entry.initCppFrame(aStackAddress, line);
    entry.setLabel(aName);
    entry.setDynamicString(aDynamicString);
    MOZ_ASSERT(entry.flags() == js::ProfileEntry::IS_CPP_ENTRY);
    entry.setCategory(aCategory);

    // Track if mLabel needs a copy.
    if (aCopy) {
      entry.setFlag(js::ProfileEntry::FRAME_LABEL_COPY);
    } else {
      entry.unsetFlag(js::ProfileEntry::FRAME_LABEL_COPY);
    }

    // This must happen at the end! The compiler will not reorder this update
    // because mStackPointer is Atomic.
    mStackPointer++;
  }

  // Pop the stack.
  void pop() { mStackPointer--; }

  uint32_t stackSize() const
  {
    return std::min(uint32_t(mStackPointer), uint32_t(mozilla::ArrayLength(mStack)));
  }

  mozilla::Atomic<uint32_t>* AddressOfStackPointer() { return &mStackPointer; }

private:
  // No copying.
  PseudoStack(const PseudoStack&) = delete;
  void operator=(const PseudoStack&) = delete;

public:
  // The list of active checkpoints.
  js::ProfileEntry volatile mStack[1024];

protected:
  // This may exceed the length of mStack, so instead use the stackSize() method
  // to determine the number of valid samples in mStack.
  mozilla::Atomic<uint32_t> mStackPointer;
};

#endif  // PseudoStack_h
