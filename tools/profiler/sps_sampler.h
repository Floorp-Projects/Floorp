/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLS_SPS_SAMPLER_H_
#define TOOLS_SPS_SAMPLER_H_

#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include "mozilla/ThreadLocal.h"
#include "nscore.h"
#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Util.h"
#include "nsAlgorithm.h"
#include <algorithm>


/* QT has a #define for the word "slots" and jsfriendapi.h has a struct with
 * this variable name, causing compilation problems. Alleviate this for now by
 * removing this #define */
#ifdef MOZ_WIDGET_QT
#undef slots
#endif
#include "jsfriendapi.h"

// Make sure that we can use std::min here without the Windows headers messing with us.
#ifdef min
#undef min
#endif

using mozilla::TimeStamp;
using mozilla::TimeDuration;

struct PseudoStack;
class TableTicker;
class JSCustomObject;

extern mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
extern bool stack_key_initialized;

#ifndef SAMPLE_FUNCTION_NAME
# ifdef __GNUC__
#  define SAMPLE_FUNCTION_NAME __FUNCTION__
# elif defined(_MSC_VER)
#  define SAMPLE_FUNCTION_NAME __FUNCTION__
# else
#  define SAMPLE_FUNCTION_NAME __func__  // defined in C99, supported in various C++ compilers. Just raw function name.
# endif
#endif

/* Returns true if env var SPS_NEW is set to anything, else false. */
extern bool sps_version2();

#define SAMPLER_INIT() \
  do { \
    if (!sps_version2()) mozilla_sampler_init1(); \
                    else mozilla_sampler_init2(); \
  } while (0)

#define SAMPLER_SHUTDOWN() \
  do { \
    if (!sps_version2()) mozilla_sampler_shutdown1(); \
                    else mozilla_sampler_shutdown2(); \
  } while (0)

#define SAMPLER_START(entries, interval, features, featureCount) \
  do { \
    if (!sps_version2()) \
      mozilla_sampler_start1(entries, interval, features, featureCount); \
    else \
      mozilla_sampler_start2(entries, interval, features, featureCount); \
  } while (0)

#define SAMPLER_STOP() \
  do { \
    if (!sps_version2()) mozilla_sampler_stop1(); \
                    else mozilla_sampler_stop2(); \
  } while (0)

#define SAMPLER_IS_ACTIVE() \
    (!sps_version2() ? mozilla_sampler_is_active1() \
                     : mozilla_sampler_is_active2() )

#define SAMPLER_RESPONSIVENESS(time) \
  do { \
    if (!sps_version2()) mozilla_sampler_responsiveness1(time); \
                    else mozilla_sampler_responsiveness2(time); \
  } while (0)

#define SAMPLER_GET_RESPONSIVENESS() \
    (!sps_version2() ? mozilla_sampler_get_responsiveness1() \
                     : mozilla_sampler_get_responsiveness2() )

#define SAMPLER_FRAME_NUMBER(frameNumber) \
  do { \
    if (!sps_version2()) mozilla_sampler_frame_number1(frameNumber); \
                    else mozilla_sampler_frame_number2(frameNumber); \
  } while (0)

#define SAMPLER_SAVE() \
  do { \
    if (!sps_version2()) mozilla_sampler_save1(); \
                    else mozilla_sampler_save2(); \
  } while (0)

#define SAMPLER_GET_PROFILE() \
   (!sps_version2() ? mozilla_sampler_get_profile1() \
                    : mozilla_sampler_get_profile2() )

#define SAMPLER_GET_PROFILE_DATA(ctx) \
   (!sps_version2() ? mozilla_sampler_get_profile_data1(ctx) \
                    : mozilla_sampler_get_profile_data2(ctx) )

#define SAMPLER_GET_FEATURES() \
   (!sps_version2() ? mozilla_sampler_get_features1() \
                    : mozilla_sampler_get_features2() )

// we want the class and function name but can't easily get that using preprocessor macros
// __func__ doesn't have the class name and __PRETTY_FUNCTION__ has the parameters

#define SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, line) SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line)
#define SAMPLER_APPEND_LINE_NUMBER(id) SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#define SAMPLE_LABEL(name_space, info) mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, __LINE__)
#define SAMPLE_LABEL_PRINTF(name_space, info, ...) mozilla::SamplerStackFramePrintfRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, __LINE__, __VA_ARGS__)
#define SAMPLE_MARKER(info) mozilla_sampler_add_marker(info)
#define SAMPLE_MAIN_THREAD_LABEL(name_space, info)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, __LINE__)
#define SAMPLE_MAIN_THREAD_LABEL_PRINTF(name_space, info, ...)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla::SamplerStackFramePrintfRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, __LINE__, __VA_ARGS__)
#define SAMPLE_MAIN_THREAD_MARKER(info)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla_sampler_add_marker(info)

#define SAMPLER_PRINT_LOCATION() \
    do { \
      if (!sps_version2()) mozilla_sampler_print_location1(); \
                      else mozilla_sampler_print_location2(); \
    } while (0)

#define SAMPLER_LOCK() \
    do { \
      if (!sps_version2()) mozilla_sampler_lock1(); \
                      else mozilla_sampler_lock2(); \
    } while (0)

#define SAMPLER_UNLOCK() \
    do { \
      if (!sps_version2()) mozilla_sampler_unlock1(); \
                      else mozilla_sampler_unlock2(); \
    } while (0)

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

/* FIXME/bug 789667: memory constraints wouldn't much of a problem for
 * this small a sample buffer size, except that serializing the
 * profile data is extremely, unnecessarily memory intensive. */
#ifdef MOZ_WIDGET_GONK
# define PLATFORM_LIKELY_MEMORY_CONSTRAINED
#endif

#if !defined(PLATFORM_LIKELY_MEMORY_CONSTRAINED) && !defined(ARCH_ARMV6)
# define PROFILE_DEFAULT_ENTRY 1000000
#else
# define PROFILE_DEFAULT_ENTRY 100000
#endif

#if defined(PLATFORM_LIKELY_MEMORY_CONSTRAINED)
/* A 1ms sampling interval has been shown to be a large perf hit
 * (10fps) on memory-contrained (low-end) platforms, and additionally
 * to yield different results from the profiler.  Where this is the
 * important case, b2g, there are also many gecko processes which
 * magnify these effects. */
# define PROFILE_DEFAULT_INTERVAL 10
#elif defined(ANDROID)
// We use a lower frequency on Android, in order to make things work
// more smoothly on phones.  This value can be adjusted later with
// some libunwind optimizations.
// In one sample measurement on Galaxy Nexus, out of about 700 backtraces,
// 60 of them took more than 25ms, and the average and standard deviation
// were 6.17ms and 9.71ms respectively.

// For now since we don't support stackwalking let's use 1ms since it's fast
// enough.
#define PROFILE_DEFAULT_INTERVAL 1
#else
#define PROFILE_DEFAULT_INTERVAL 1
#endif
#define PROFILE_DEFAULT_FEATURES NULL
#define PROFILE_DEFAULT_FEATURE_COUNT 0

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

// Returns a handle to pass on exit. This can check that we are popping the
// correct callstack.
inline void* mozilla_sampler_call_enter(const char *aInfo, void *aFrameAddress = NULL,
                                        bool aCopy = false, uint32_t line = 0);
inline void  mozilla_sampler_call_exit(void* handle);
inline void  mozilla_sampler_add_marker(const char *aInfo);

void mozilla_sampler_start1(int aEntries, int aInterval, const char** aFeatures,
                            uint32_t aFeatureCount);
void mozilla_sampler_start2(int aEntries, int aInterval, const char** aFeatures,
                            uint32_t aFeatureCount);

void mozilla_sampler_stop1();
void mozilla_sampler_stop2();

bool mozilla_sampler_is_active1();
bool mozilla_sampler_is_active2();

void mozilla_sampler_responsiveness1(TimeStamp time);
void mozilla_sampler_responsiveness2(TimeStamp time);

void mozilla_sampler_frame_number1(int frameNumber);
void mozilla_sampler_frame_number2(int frameNumber);

const double* mozilla_sampler_get_responsiveness1();
const double* mozilla_sampler_get_responsiveness2();

void mozilla_sampler_save1();
void mozilla_sampler_save2();

char* mozilla_sampler_get_profile1();
char* mozilla_sampler_get_profile2();

JSObject *mozilla_sampler_get_profile_data1(JSContext *aCx);
JSObject *mozilla_sampler_get_profile_data2(JSContext *aCx);

const char** mozilla_sampler_get_features1();
const char** mozilla_sampler_get_features2();

void mozilla_sampler_init1();
void mozilla_sampler_init2();

void mozilla_sampler_shutdown1();
void mozilla_sampler_shutdown2();

void mozilla_sampler_print_location1();
void mozilla_sampler_print_location2();

// Lock the profiler. When locked the profiler is (1) stopped,
// (2) profile data is cleared, (3) profiler-locked is fired.
// This is used to lock down the profiler during private browsing
void mozilla_sampler_lock1();
void mozilla_sampler_lock2();

// Unlock the profiler, leaving it stopped and fires profiler-unlocked.
void mozilla_sampler_unlock1();
void mozilla_sampler_unlock2();

namespace mozilla {

class NS_STACK_CLASS SamplerStackFrameRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFrameRAII(const char *aInfo, uint32_t line) {
    mHandle = mozilla_sampler_call_enter(aInfo, this, false, line);
  }
  ~SamplerStackFrameRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  void* mHandle;
};

static const int SAMPLER_MAX_STRING = 128;
class NS_STACK_CLASS SamplerStackFramePrintfRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFramePrintfRAII(const char *aDefault, uint32_t line, const char *aFormat, ...) {
    if (SAMPLER_IS_ACTIVE()) {
      va_list args;
      va_start(args, aFormat);
      char buff[SAMPLER_MAX_STRING];

      // We have to use seperate printf's because we're using
      // the vargs.
#if _MSC_VER
      _vsnprintf(buff, SAMPLER_MAX_STRING, aFormat, args);
      _snprintf(mDest, SAMPLER_MAX_STRING, "%s %s", aDefault, buff);
#else
      vsnprintf(buff, SAMPLER_MAX_STRING, aFormat, args);
      snprintf(mDest, SAMPLER_MAX_STRING, "%s %s", aDefault, buff);
#endif
      mHandle = mozilla_sampler_call_enter(mDest, this, true, line);
      va_end(args);
    } else {
      mHandle = mozilla_sampler_call_enter(aDefault, NULL, false, line);
    }
  }
  ~SamplerStackFramePrintfRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  char mDest[SAMPLER_MAX_STRING];
  void* mHandle;
};

} //mozilla

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
    , mRuntime(NULL)
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
      return NULL;
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
    push(aName, NULL, false, line);
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
    return std::min<uint32_t>(mStackPointer, mozilla::ArrayLength(mStack));
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
                                 mozilla::ArrayLength(mStack));
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

inline PseudoStack* mozilla_get_pseudo_stack(void)
{
  if (!stack_key_initialized)
    return NULL;
  return tlsPseudoStack.get();
}

inline void* mozilla_sampler_call_enter(const char *aInfo, void *aFrameAddress,
                                        bool aCopy, uint32_t line)
{
  // check if we've been initialized to avoid calling pthread_getspecific
  // with a null tlsStack which will return undefined results.
  if (!stack_key_initialized)
    return NULL;

  PseudoStack *stack = tlsPseudoStack.get();
  // we can't infer whether 'stack' has been initialized
  // based on the value of stack_key_intiailized because
  // 'stack' is only intialized when a thread is being
  // profiled.
  if (!stack) {
    return stack;
  }
  stack->push(aInfo, aFrameAddress, aCopy, line);

  // The handle is meant to support future changes
  // but for now it is simply use to save a call to
  // pthread_getspecific on exit. It also supports the
  // case where the sampler is initialized between
  // enter and exit.
  return stack;
}

inline void mozilla_sampler_call_exit(void *aHandle)
{
  if (!aHandle)
    return;

  PseudoStack *stack = (PseudoStack*)aHandle;
  stack->pop();
}

inline void mozilla_sampler_add_marker(const char *aMarker)
{
  if (!stack_key_initialized)
    return;

  // Don't insert a marker if we're not profiling to avoid
  // the heap copy (malloc).
  if (!SAMPLER_IS_ACTIVE()) {
    return;
  }

  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }
  stack->addMarker(aMarker);
}

#endif /* ndef TOOLS_SPS_SAMPLER_H_ */
