/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_PSEUDO_STACK_H_
#define PROFILER_PSEUDO_STACK_H_

#include "mozilla/ArrayUtils.h"
#include "js/ProfilingStack.h"
#include <stdlib.h>
#include "mozilla/Atomics.h"
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

// Stub eventMarker function for js-engine event generation.
void ProfilerJSEventMarker(const char* aEvent);

// Note that some of these fields (e.g. mSleep, mPrivacyMode) aren't really
// part of the PseudoStack, but they are part of this class so they can be
// stored in TLS.
//
// The PseudoStack members are read by signal handlers, so the mutation of them
// needs to be signal-safe.
struct PseudoStack
{
public:
  PseudoStack()
    : mStackPointer(0)
    , mSleep(AWAKE)
    , mContext(nullptr)
    , mStartJSSampling(false)
    , mPrivacyMode(false)
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

  // This is called on every profiler restart. Put things that should happen at
  // that time here.
  void reinitializeOnResume()
  {
    // This is needed to cause an initial sample to be taken from sleeping
    // threads that had been observed prior to the profiler stopping and
    // restarting. Otherwise sleeping threads would not have any samples to
    // copy forward while sleeping.
    (void)mSleep.compareExchange(SLEEPING_OBSERVED, SLEEPING_NOT_OBSERVED);
  }

  void addMarker(const char* aMarkerStr, ProfilerMarkerPayload* aPayload,
                 double aTime)
  {
    ProfilerMarker* marker = new ProfilerMarker(aMarkerStr, aPayload, aTime);
    mPendingMarkers.insert(marker);
  }

  // Called within signal. Function must be reentrant.
  ProfilerMarkerLinkedList* getPendingMarkers()
  {
    // The profiled thread is interrupted, so we can access the list safely.
    // Unless the profiled thread was in the middle of changing the list when
    // we interrupted it - in that case, accessList() will return null.
    return mPendingMarkers.accessList();
  }

  void push(const char* aName, js::ProfileEntry::Category aCategory,
            uint32_t line)
  {
    push(aName, aCategory, nullptr, false, line);
  }

  void push(const char* aName, js::ProfileEntry::Category aCategory,
            void* aStackAddress, bool aCopy, uint32_t line)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    volatile js::ProfileEntry& entry = mStack[mStackPointer];

    // Make sure we increment the pointer after the name has been written such
    // that mStack is always consistent.
    entry.initCppFrame(aStackAddress, line);
    entry.setLabel(aName);
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
    return std::min(mStackPointer,
                    mozilla::sig_safe_t(mozilla::ArrayLength(mStack)));
  }

  void sampleContext(JSContext* context)
  {
    if (mContext && !context) {
      // On JS shut down, flush the current buffer as stringifying JIT samples
      // requires a live JSContext.
      flushSamplerOnJSShutdown();
    }

    mContext = context;

    if (!context) {
      return;
    }

    static_assert(sizeof(mStack[0]) == sizeof(js::ProfileEntry),
                  "mStack must be binary compatible with js::ProfileEntry.");
    js::SetContextProfilingStack(context,
                                 (js::ProfileEntry*) mStack,
                                 (uint32_t*) &mStackPointer,
                                 (uint32_t) mozilla::ArrayLength(mStack));
    if (mStartJSSampling) {
      enableJSSampling();
    }
  }

  void enableJSSampling()
  {
    if (mContext) {
      js::EnableContextProfilingStack(mContext, true);
      js::RegisterContextProfilingEventMarker(mContext, &ProfilerJSEventMarker);
      mStartJSSampling = false;
    } else {
      mStartJSSampling = true;
    }
  }

  void jsOperationCallback()
  {
    if (mStartJSSampling) {
      enableJSSampling();
    }
  }

  void disableJSSampling()
  {
    mStartJSSampling = false;
    if (mContext) {
      js::EnableContextProfilingStack(mContext, false);
    }
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    size_t n = aMallocSizeOf(this);

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - things pointed to by mStack elements
    // - mPendingMarkers
    //
    // If these measurements are added, the code must be careful to avoid data
    // races. (The current code doesn't have any race issues because the
    // contents of the PseudoStack object aren't accessed; |this| is used only
    // as an address for lookup by aMallocSizeof).

    return n;
  }

  // This returns true for the second and subsequent calls in each sleep cycle.
  bool CanDuplicateLastSampleDueToSleep()
  {
    if (mSleep == AWAKE) {
      return false;
    }

    if (mSleep.compareExchange(SLEEPING_NOT_OBSERVED, SLEEPING_OBSERVED)) {
      return false;
    }

    return true;
  }

  // Call this whenever the current thread sleeps. Calling it twice in a row
  // without an intervening setAwake() call is an error.
  void setSleeping()
  {
    MOZ_ASSERT(mSleep == AWAKE);
    mSleep = SLEEPING_NOT_OBSERVED;
  }

  // Call this whenever the current thread wakes. Calling it twice in a row
  // without an intervening setSleeping() call is an error.
  void setAwake()
  {
    MOZ_ASSERT(mSleep != AWAKE);
    mSleep = AWAKE;
  }

  bool isSleeping() { return mSleep != AWAKE; }

private:
  // No copying.
  PseudoStack(const PseudoStack&) = delete;
  void operator=(const PseudoStack&) = delete;

  void flushSamplerOnJSShutdown();

public:
  // The list of active checkpoints.
  js::ProfileEntry volatile mStack[1024];

private:
  // A list of pending markers that must be moved to the circular buffer.
  ProfilerSignalSafeLinkedList<ProfilerMarker> mPendingMarkers;

  // This may exceed the length of mStack, so instead use the stackSize() method
  // to determine the number of valid samples in mStack.
  mozilla::sig_safe_t mStackPointer;

  // mSleep tracks whether the thread is sleeping, and if so, whether it has
  // been previously observed. This is used for an optimization: in some cases,
  // when a thread is asleep, we duplicate the previous sample, which is
  // cheaper than taking a new sample.
  //
  // mSleep is atomic because it is accessed from multiple threads.
  //
  // - It is written only by this thread, via setSleeping() and setAwake().
  //
  // - It is read by the SamplerThread (on Win32 and Mac) or the SigprofSender
  //   thread (on Linux and Android).
  //
  // There are two cases where racing between threads can cause an issue.
  //
  // - If CanDuplicateLastSampleDueToSleep() returns false but that result is
  //   invalidated before being acted upon, we will take a full sample
  //   unnecessarily. This is additional work but won't cause any correctness
  //   issues. (In actual fact, this case is impossible. In order to go from
  //   CanDuplicateLastSampleDueToSleep() returning false to it returning true
  //   requires an intermediate call to it in order for mSleep to go from
  //   SLEEPING_NOT_OBSERVED to SLEEPING_OBSERVED.)
  //
  // - If CanDuplicateLastSampleDueToSleep() returns true but that result is
  //   invalidated before being acted upon -- i.e. the thread wakes up before
  //   DuplicateLastSample() is called -- we will duplicate the previous
  //   sample. This is inaccurate, but only slightly... we will effectively
  //   treat the thread as having slept a tiny bit longer than it really did.
  //
  // This latter inaccuracy could be avoided by moving the
  // CanDuplicateLastSampleDueToSleep() check within the thread-freezing code,
  // e.g. the section where Tick() is called. But that would reduce the
  // effectiveness of the optimization because more code would have to be run
  // before we can tell that duplication is allowed.
  //
  static const int AWAKE = 0;
  static const int SLEEPING_NOT_OBSERVED = 1;
  static const int SLEEPING_OBSERVED = 2;
  mozilla::Atomic<int> mSleep;

public:
  // The context being sampled.
  JSContext* mContext;

private:
  // Start JS Profiling when possible.
  bool mStartJSSampling;

public:
  // Is private browsing on?
  bool mPrivacyMode;
};

#endif
