/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_PSEUDO_STACK_H_
#define PROFILER_PSEUDO_STACK_H_

#include "mozilla/ArrayUtils.h"
#include <stdint.h>
#include "js/ProfilingStack.h"
#include <stdlib.h>
#include "mozilla/Atomics.h"
#ifndef SPS_STANDALONE
#include "nsISupportsImpl.h"
#endif

/* we duplicate this code here to avoid header dependencies
 * which make it more difficult to include in other places */
#if defined(_M_X64) || defined(__x86_64__)
#define V8_HOST_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386)
#define V8_HOST_ARCH_IA32 1
#elif defined(__ARMEL__)
#define V8_HOST_ARCH_ARM 1
#else
#warning Please add support for your architecture in chromium_types.h
#endif

// STORE_SEQUENCER: Because signals can interrupt our profile modification
//                  we need to make stores are not re-ordered by the compiler
//                  or hardware to make sure the profile is consistent at
//                  every point the signal can fire.
#ifdef V8_HOST_ARCH_ARM
// TODO Is there something cheaper that will prevent
//      memory stores from being reordered

typedef void (*LinuxKernelMemoryBarrierFunc)(void);
LinuxKernelMemoryBarrierFunc pLinuxKernelMemoryBarrier __attribute__((weak)) =
    (LinuxKernelMemoryBarrierFunc) 0xffff0fa0;

# define STORE_SEQUENCER() pLinuxKernelMemoryBarrier()
#elif defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_X64)
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

// We can't include <algorithm> because it causes issues on OS X, so we use
// our own min function.
static inline uint32_t sMin(uint32_t l, uint32_t r) {
  return l < r ? l : r;
}

// A stack entry exists to allow the JS engine to inform SPS of the current
// backtrace, but also to instrument particular points in C++ in case stack
// walking is not available on the platform we are running on.
//
// Each entry has a descriptive string, a relevant stack address, and some extra
// information the JS engine might want to inform SPS of. This class inherits
// from the JS engine's version of the entry to ensure that the size and layout
// of the two representations are consistent.
class StackEntry : public js::ProfileEntry
{
};

class ProfilerMarkerPayload;
template<typename T>
class ProfilerLinkedList;
class SpliceableJSONWriter;
class UniqueStacks;

class ProfilerMarker {
  friend class ProfilerLinkedList<ProfilerMarker>;
public:
  explicit ProfilerMarker(const char* aMarkerName,
                          ProfilerMarkerPayload* aPayload = nullptr,
                          double aTime = 0);

  ~ProfilerMarker();

  const char* GetMarkerName() const {
    return mMarkerName;
  }

  void StreamJSON(SpliceableJSONWriter& aWriter, UniqueStacks& aUniqueStacks) const;

  void SetGeneration(uint32_t aGenID);

  bool HasExpired(uint32_t aGenID) const {
    return mGenID + 2 <= aGenID;
  }

  double GetTime() const;

private:
  char* mMarkerName;
  ProfilerMarkerPayload* mPayload;
  ProfilerMarker* mNext;
  double mTime;
  uint32_t mGenID;
};

template<typename T>
class ProfilerLinkedList {
public:
  ProfilerLinkedList()
    : mHead(nullptr)
    , mTail(nullptr)
  {}

  void insert(T* elem)
  {
    if (!mTail) {
      mHead = elem;
      mTail = elem;
    } else {
      mTail->mNext = elem;
      mTail = elem;
    }
    elem->mNext = nullptr;
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
class ProfilerSignalSafeLinkedList {
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

  // Insert an item into the list.
  // Must only be called from the owning thread.
  // Must not be called while the list from accessList() is being accessed.
  // In the profiler, we ensure that by interrupting the profiled thread
  // (which is the one that owns this list and calls insert() on it) until
  // we're done reading the list from the signal handler.
  void insert(T* aElement) {
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
    if (mSignalLock) {
      return nullptr;
    }
    return &mList;
  }

private:
  ProfilerLinkedList<T> mList;

  // If this is set, then it's not safe to read the list because its contents
  // are being changed.
  volatile bool mSignalLock;
};

// Stub eventMarker function for js-engine event generation.
void ProfilerJSEventMarker(const char *event);

// the PseudoStack members are read by signal
// handlers, so the mutation of them needs to be signal-safe.
struct PseudoStack
{
public:
  // Create a new PseudoStack and acquire a reference to it.
  static PseudoStack *create()
  {
    return new PseudoStack();
  }

  // This is called on every profiler restart. Put things that should happen at that time here.
  void reinitializeOnResume() {
    // This is needed to cause an initial sample to be taken from sleeping threads. Otherwise sleeping
    // threads would not have any samples to copy forward while sleeping.
    mSleepId++;
  }

  void addMarker(const char* aMarkerStr, ProfilerMarkerPayload* aPayload, double aTime)
  {
    ProfilerMarker* marker = new ProfilerMarker(aMarkerStr, aPayload, aTime);
    mPendingMarkers.insert(marker);
  }

  // called within signal. Function must be reentrant
  ProfilerMarkerLinkedList* getPendingMarkers()
  {
    // The profiled thread is interrupted, so we can access the list safely.
    // Unless the profiled thread was in the middle of changing the list when
    // we interrupted it - in that case, accessList() will return null.
    return mPendingMarkers.accessList();
  }

  void push(const char *aName, js::ProfileEntry::Category aCategory, uint32_t line)
  {
    push(aName, aCategory, nullptr, false, line);
  }

  void push(const char *aName, js::ProfileEntry::Category aCategory,
    void *aStackAddress, bool aCopy, uint32_t line)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    // In order to ensure this object is kept alive while it is
    // active, we acquire a reference at the outermost push.  This is
    // released by the corresponding pop.
    if (mStackPointer == 0) {
      ref();
    }

    volatile StackEntry &entry = mStack[mStackPointer];

    // Make sure we increment the pointer after the name has
    // been written such that mStack is always consistent.
    entry.initCppFrame(aStackAddress, line);
    entry.setLabel(aName);
    MOZ_ASSERT(entry.flags() == js::ProfileEntry::IS_CPP_ENTRY);
    entry.setCategory(aCategory);

    // Track if mLabel needs a copy.
    if (aCopy)
      entry.setFlag(js::ProfileEntry::FRAME_LABEL_COPY);
    else
      entry.unsetFlag(js::ProfileEntry::FRAME_LABEL_COPY);

    // Prevent the optimizer from re-ordering these instructions
    STORE_SEQUENCER();
    mStackPointer++;
  }

  // Pop the stack.  If the stack is empty and all other references to
  // this PseudoStack have been dropped, then the PseudoStack is
  // deleted and "false" is returned.  Otherwise "true" is returned.
  bool popAndMaybeDelete()
  {
    mStackPointer--;
    if (mStackPointer == 0) {
      // Release our self-owned reference count.  See 'push'.
      deref();
      return false;
    } else {
      return true;
    }
  }
  bool isEmpty()
  {
    return mStackPointer == 0;
  }
  uint32_t stackSize() const
  {
    return sMin(mStackPointer, mozilla::sig_safe_t(mozilla::ArrayLength(mStack)));
  }

  void sampleRuntime(JSRuntime* runtime) {
#ifndef SPS_STANDALONE
    if (mRuntime && !runtime) {
      // On JS shut down, flush the current buffer as stringifying JIT samples
      // requires a live JSRuntime.
      flushSamplerOnJSShutdown();
    }

    mRuntime = runtime;

    if (!runtime) {
      return;
    }

    static_assert(sizeof(mStack[0]) == sizeof(js::ProfileEntry),
                  "mStack must be binary compatible with js::ProfileEntry.");
    js::SetRuntimeProfilingStack(runtime,
                                 (js::ProfileEntry*) mStack,
                                 (uint32_t*) &mStackPointer,
                                 (uint32_t) mozilla::ArrayLength(mStack));
    if (mStartJSSampling)
      enableJSSampling();
#endif
  }
#ifndef SPS_STANDALONE
  void enableJSSampling() {
    if (mRuntime) {
      js::EnableRuntimeProfilingStack(mRuntime, true);
      js::RegisterRuntimeProfilingEventMarker(mRuntime, &ProfilerJSEventMarker);
      mStartJSSampling = false;
    } else {
      mStartJSSampling = true;
    }
  }
  void jsOperationCallback() {
    if (mStartJSSampling)
      enableJSSampling();
  }
  void disableJSSampling() {
    mStartJSSampling = false;
    if (mRuntime)
      js::EnableRuntimeProfilingStack(mRuntime, false);
  }
#endif

  // Keep a list of active checkpoints
  StackEntry volatile mStack[1024];
 private:

  // A PseudoStack can only be created via the "create" method.
  PseudoStack()
    : mStackPointer(0)
    , mSleepId(0)
    , mSleepIdObserved(0)
    , mSleeping(false)
    , mRefCnt(1)
#ifndef SPS_STANDALONE
    , mRuntime(nullptr)
#endif
    , mStartJSSampling(false)
    , mPrivacyMode(false)
  { }

  // A PseudoStack can only be deleted via deref.
  ~PseudoStack() {
    if (mStackPointer != 0) {
      // We're releasing the pseudostack while it's still in use.
      // The label macros keep a non ref counted reference to the
      // stack to avoid a TLS. If these are not all cleared we will
      // get a use-after-free so better to crash now.
      abort();
    }
  }

  // No copying.
  PseudoStack(const PseudoStack&) = delete;
  void operator=(const PseudoStack&) = delete;

  void flushSamplerOnJSShutdown();

  // Keep a list of pending markers that must be moved
  // to the circular buffer
  ProfilerSignalSafeLinkedList<ProfilerMarker> mPendingMarkers;
  // This may exceed the length of mStack, so instead use the stackSize() method
  // to determine the number of valid samples in mStack
  mozilla::sig_safe_t mStackPointer;
  // Incremented at every sleep/wake up of the thread
  int mSleepId;
  // Previous id observed. If this is not the same as mSleepId, this thread is not sleeping in the same place any more
  mozilla::Atomic<int> mSleepIdObserved;
  // Keeps tack of whether the thread is sleeping or not (1 when sleeping 0 when awake)
  mozilla::Atomic<int> mSleeping;
  // This class is reference counted because it must be kept alive by
  // the ThreadInfo, by the reference from tlsPseudoStack, and by the
  // current thread when callbacks are in progress.
  mozilla::Atomic<int> mRefCnt;

 public:
#ifndef SPS_STANDALONE
  // The runtime which is being sampled
  JSRuntime *mRuntime;
#endif
  // Start JS Profiling when possible
  bool mStartJSSampling;
  bool mPrivacyMode;

  enum SleepState {NOT_SLEEPING, SLEEPING_FIRST, SLEEPING_AGAIN};

  // The first time this is called per sleep cycle we return SLEEPING_FIRST
  // and any other subsequent call within the same sleep cycle we return SLEEPING_AGAIN
  SleepState observeSleeping() {
    if (mSleeping != 0) {
      if (mSleepIdObserved == mSleepId) {
        return SLEEPING_AGAIN;
      } else {
        mSleepIdObserved = mSleepId;
        return SLEEPING_FIRST;
      }
    } else {
      return NOT_SLEEPING;
    }
  }


  // Call this whenever the current thread sleeps or wakes up
  // Calling setSleeping with the same value twice in a row is an error
  void setSleeping(int sleeping) {
    MOZ_ASSERT(mSleeping != sleeping);
    mSleepId++;
    mSleeping = sleeping;
  }

  void ref() {
    ++mRefCnt;
  }

  void deref() {
    int newValue = --mRefCnt;
    if (newValue == 0) {
      delete this;
    }
  }
};

#endif
