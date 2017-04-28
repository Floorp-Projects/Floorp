/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PseudoStack_h
#define PseudoStack_h

#include "mozilla/ArrayUtils.h"
#include "js/ProfilingStack.h"
#include "nsISupportsImpl.h"

#include <stdlib.h>
#include <stdint.h>

#include <algorithm>

// STORE_SEQUENCER: Because signals can interrupt our profile modification
//                  we need to make stores are not re-ordered by the compiler
//                  or hardware to make sure the profile is consistent at
//                  every point the signal can fire.
#if defined(__arm__)
// TODO Is there something cheaper that will prevent memory stores from being
// reordered?

typedef void (*LinuxKernelMemoryBarrierFunc)(void);
LinuxKernelMemoryBarrierFunc pLinuxKernelMemoryBarrier __attribute__((weak)) =
    (LinuxKernelMemoryBarrierFunc) 0xffff0fa0;

# define STORE_SEQUENCER() pLinuxKernelMemoryBarrier()

#elif defined(__i386__) || defined(__x86_64__) || \
      defined(_M_IX86) || defined(_M_X64)
# if defined(_MSC_VER)
#  include <intrin.h>
#  define STORE_SEQUENCER() _ReadWriteBarrier();
# elif defined(__INTEL_COMPILER)
#  define STORE_SEQUENCER() __memory_barrier();
# elif __GNUC__
#  define STORE_SEQUENCER() asm volatile("" ::: "memory");
# else
#  error "Memory clobber not supported for your compiler."
# endif

#else
# error "Memory clobber not supported for your platform."
#endif

class ProfilerMarkerPayload;
template<typename T>
class ProfilerLinkedList;
class SpliceableJSONWriter;
class UniqueStacks;

class ProfilerMarker
{
  friend class ProfilerLinkedList<ProfilerMarker>;

public:
  explicit ProfilerMarker(const char* aMarkerName,
                          ProfilerMarkerPayload* aPayload = nullptr,
                          double aTime = 0);
  ~ProfilerMarker();

  const char* GetMarkerName() const { return mMarkerName; }

  void StreamJSON(SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aStartTime,
                  UniqueStacks& aUniqueStacks) const;

  void SetGeneration(uint32_t aGenID);

  bool HasExpired(uint32_t aGenID) const { return mGenID + 2 <= aGenID; }

  double GetTime() const;

private:
  char* mMarkerName;
  ProfilerMarkerPayload* mPayload;
  ProfilerMarker* mNext;
  double mTime;
  uint32_t mGenID;
};

template<typename T>
class ProfilerLinkedList
{
public:
  ProfilerLinkedList()
    : mHead(nullptr)
    , mTail(nullptr)
  {}

  void insert(T* aElem)
  {
    if (!mTail) {
      mHead = aElem;
      mTail = aElem;
    } else {
      mTail->mNext = aElem;
      mTail = aElem;
    }
    aElem->mNext = nullptr;
  }

  T* popHead()
  {
    if (!mHead) {
      MOZ_ASSERT(false);
      return nullptr;
    }

    T* head = mHead;

    mHead = head->mNext;
    if (!mHead) {
      mTail = nullptr;
    }

    return head;
  }

  const T* peek() {
    return mHead;
  }

private:
  T* mHead;
  T* mTail;
};

typedef ProfilerLinkedList<ProfilerMarker> ProfilerMarkerLinkedList;

template<typename T>
class ProfilerSignalSafeLinkedList
{
public:
  ProfilerSignalSafeLinkedList()
    : mSignalLock(false)
  {}

  ~ProfilerSignalSafeLinkedList()
  {
    if (mSignalLock) {
      // Some thread is modifying the list. We should only be released on that
      // thread.
      abort();
    }

    while (mList.peek()) {
      delete mList.popHead();
    }
  }

  // Insert an item into the list. Must only be called from the owning thread.
  // Must not be called while the list from accessList() is being accessed.
  // In the profiler, we ensure that by interrupting the profiled thread
  // (which is the one that owns this list and calls insert() on it) until
  // we're done reading the list from the signal handler.
  void insert(T* aElement)
  {
    MOZ_ASSERT(aElement);

    mSignalLock = true;
    STORE_SEQUENCER();

    mList.insert(aElement);

    STORE_SEQUENCER();
    mSignalLock = false;
  }

  // Called within signal, from any thread, possibly while insert() is in the
  // middle of modifying the list (on the owning thread). Will return null if
  // that is the case.
  // Function must be reentrant.
  ProfilerLinkedList<T>* accessList()
  {
    return mSignalLock ? nullptr : &mList;
  }

private:
  ProfilerLinkedList<T> mList;

  // If this is set, then it's not safe to read the list because its contents
  // are being changed.
  volatile bool mSignalLock;
};

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

    // Prevent the optimizer from re-ordering these instructions
    STORE_SEQUENCER();
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
