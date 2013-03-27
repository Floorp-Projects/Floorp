/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_PSEUDO_STACK_H_
#define PROFILER_PSEUDO_STACK_H_

#include "mozilla/NullPtr.h"
#include "mozilla/StandardInteger.h"
#include "jsfriendapi.h"
#include <stdlib.h>

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
#if _MSC_VER > 1400
#  include <intrin.h>
#else // _MSC_VER > 1400
    // MSVC2005 has a name collision bug caused when both <intrin.h> and <winnt.h> are included together.
#ifdef _WINNT_
#  define _interlockedbittestandreset _interlockedbittestandreset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  define _interlockedbittestandset _interlockedbittestandset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  include <intrin.h>
#else
#  include <intrin.h>
#  define _interlockedbittestandreset _interlockedbittestandreset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#  define _interlockedbittestandset _interlockedbittestandset_NAME_CHANGED_TO_AVOID_MSVS2005_ERROR
#endif
   // Even though MSVC2005 has the intrinsic _ReadWriteBarrier, it fails to link to it when it's
   // not explicitly declared.
#  pragma intrinsic(_ReadWriteBarrier)
#endif // _MSC_VER > 1400
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
public:

  bool isCopyLabel() volatile {
    return !((uintptr_t)stackAddress() & 0x1);
  }

  void setStackAddressCopy(void *sparg, bool copy) volatile {
    // Tagged pointer. Less significant bit used to track if mLabel needs a
    // copy. Note that we don't need the last bit of the stack address for
    // proper ordering. This is optimized for encoding within the JS engine's
    // instrumentation, so we do the extra work here of encoding a bit.
    // Last bit 1 = Don't copy, Last bit 0 = Copy.
    if (copy) {
      setStackAddress(reinterpret_cast<void*>(
                        reinterpret_cast<uintptr_t>(sparg) & ~0x1));
    } else {
      setStackAddress(reinterpret_cast<void*>(
                        reinterpret_cast<uintptr_t>(sparg) | 0x1));
    }
  }
};

// the PseudoStack members are read by signal
// handlers, so the mutation of them needs to be signal-safe.
struct PseudoStack
{
public:
  PseudoStack()
    : mStackPointer(0)
    , mSignalLock(false)
    , mMarkerPointer(0)
    , mQueueClearMarker(false)
    , mRuntime(nullptr)
    , mStartJSSampling(false)
  { }

  void addMarker(const char *aMarker)
  {
    char* markerCopy = strdup(aMarker);
    mSignalLock = true;
    STORE_SEQUENCER();

    if (mQueueClearMarker) {
      clearMarkers();
    }
    if (!aMarker) {
      return; //discard
    }
    if (size_t(mMarkerPointer) == mozilla::ArrayLength(mMarkers)) {
      return; //array full, silently drop
    }
    mMarkers[mMarkerPointer] = markerCopy;
    mMarkerPointer++;

    mSignalLock = false;
    STORE_SEQUENCER();
  }

  // called within signal. Function must be reentrant
  const char* getMarker(int aMarkerId)
  {
    // if mSignalLock then the stack is inconsistent because it's being
    // modified by the profiled thread. Post pone these markers
    // for the next sample. The odds of a livelock are nearly impossible
    // and would show up in a profile as many sample in 'addMarker' thus
    // we ignore this scenario.
    // if mQueueClearMarker then we've the sampler thread has already
    // thread the markers then they are pending deletion.
    if (mSignalLock || mQueueClearMarker || aMarkerId < 0 ||
      static_cast<mozilla::sig_safe_t>(aMarkerId) >= mMarkerPointer) {
      return nullptr;
    }
    return mMarkers[aMarkerId];
  }

  // called within signal. Function must be reentrant
  void clearMarkers()
  {
    for (mozilla::sig_safe_t i = 0; i < mMarkerPointer; i++) {
      free(mMarkers[i]);
    }
    mMarkerPointer = 0;
    mQueueClearMarker = false;
  }

  void push(const char *aName, uint32_t line)
  {
    push(aName, nullptr, false, line);
  }

  void push(const char *aName, void *aStackAddress, bool aCopy, uint32_t line)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    // Make sure we increment the pointer after the name has
    // been written such that mStack is always consistent.
    mStack[mStackPointer].setLabel(aName);
    mStack[mStackPointer].setStackAddressCopy(aStackAddress, aCopy);
    mStack[mStackPointer].setLine(line);

    // Prevent the optimizer from re-ordering these instructions
    STORE_SEQUENCER();
    mStackPointer++;
  }
  void pop()
  {
    mStackPointer--;
  }
  bool isEmpty()
  {
    return mStackPointer == 0;
  }
  uint32_t stackSize() const
  {
    return std::min<uint32_t>(mStackPointer, mozilla::sig_safe_t(mozilla::ArrayLength(mStack)));
  }

  void sampleRuntime(JSRuntime *runtime) {
    mRuntime = runtime;
    if (!runtime) {
      // JS shut down
      return;
    }

    JS_STATIC_ASSERT(sizeof(mStack[0]) == sizeof(js::ProfileEntry));
    js::SetRuntimeProfilingStack(runtime,
                                 (js::ProfileEntry*) mStack,
                                 (uint32_t*) &mStackPointer,
                                 uint32_t(mozilla::ArrayLength(mStack)));
    if (mStartJSSampling)
      enableJSSampling();
  }
  void enableJSSampling() {
    if (mRuntime) {
      js::EnableRuntimeProfilingStack(mRuntime, true);
      mStartJSSampling = false;
    } else {
      mStartJSSampling = true;
    }
  }
  void disableJSSampling() {
    mStartJSSampling = false;
    if (mRuntime)
      js::EnableRuntimeProfilingStack(mRuntime, false);
  }

  // Keep a list of active checkpoints
  StackEntry volatile mStack[1024];
  // Keep a list of active markers to be applied to the next sample taken
  char* mMarkers[1024];
 private:
  // This may exceed the length of mStack, so instead use the stackSize() method
  // to determine the number of valid samples in mStack
  mozilla::sig_safe_t mStackPointer;
  // If this is set then it's not safe to read mStackPointer from the signal handler
  volatile bool mSignalLock;
 public:
  volatile mozilla::sig_safe_t mMarkerPointer;
  // We don't want to modify _markers from within the signal so we allow
  // it to queue a clear operation.
  volatile mozilla::sig_safe_t mQueueClearMarker;
  // The runtime which is being sampled
  JSRuntime *mRuntime;
  // Start JS Profiling when possible
  bool mStartJSSampling;
};

#endif

