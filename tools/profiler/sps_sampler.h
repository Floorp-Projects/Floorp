/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include "mozilla/ThreadLocal.h"
#include "nscore.h"
#include "jsapi.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Util.h"

/* QT has a #define for the word "slots" and jsfriendapi.h has a struct with
 * this variable name, causing compilation problems. Alleviate this for now by
 * removing this #define */
#ifdef MOZ_WIDGET_QT
#undef slots
#endif
#include "jsfriendapi.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

struct ProfileStack;
class TableTicker;

extern mozilla::ThreadLocal<ProfileStack *> tlsStack;
extern mozilla::ThreadLocal<TableTicker *> tlsTicker;
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

#define SAMPLER_INIT() mozilla_sampler_init()
#define SAMPLER_DEINIT() mozilla_sampler_deinit()
#define SAMPLER_START(entries, interval, features, featureCount) mozilla_sampler_start(entries, interval, features, featureCount)
#define SAMPLER_STOP() mozilla_sampler_stop()
#define SAMPLER_IS_ACTIVE() mozilla_sampler_is_active()
#define SAMPLER_RESPONSIVENESS(time) mozilla_sampler_responsiveness(time)
#define SAMPLER_GET_RESPONSIVENESS() mozilla_sampler_get_responsiveness()
#define SAMPLER_SAVE() mozilla_sampler_save()
#define SAMPLER_GET_PROFILE() mozilla_sampler_get_profile()
#define SAMPLER_GET_PROFILE_DATA(ctx) mozilla_sampler_get_profile_data(ctx)
#define SAMPLER_GET_FEATURES() mozilla_sampler_get_features()
// we want the class and function name but can't easily get that using preprocessor macros
// __func__ doesn't have the class name and __PRETTY_FUNCTION__ has the parameters

#define SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, line) SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line)
#define SAMPLER_APPEND_LINE_NUMBER(id) SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#define SAMPLE_LABEL(name_space, info) mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info)
#define SAMPLE_LABEL_PRINTF(name_space, info, format, ...) mozilla::SamplerStackFramePrintfRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, format, __VA_ARGS__)
#define SAMPLE_MARKER(info) mozilla_sampler_add_marker(info)

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

#define PROFILE_DEFAULT_ENTRY 100000
#ifdef ANDROID
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

// Returns a handdle to pass on exit. This can check that we are popping the
// correct callstack.
inline void* mozilla_sampler_call_enter(const char *aInfo, void *aFrameAddress = NULL, bool aCopy = false);
inline void  mozilla_sampler_call_exit(void* handle);
inline void  mozilla_sampler_add_marker(const char *aInfo);

void mozilla_sampler_start(int aEntries, int aInterval, const char** aFeatures, uint32_t aFeatureCount);
void mozilla_sampler_stop();
bool mozilla_sampler_is_active();
void mozilla_sampler_responsiveness(TimeStamp time);
const double* mozilla_sampler_get_responsiveness();
void mozilla_sampler_save();
char* mozilla_sampler_get_profile();
JSObject *mozilla_sampler_get_profile_data(JSContext *aCx);
const char** mozilla_sampler_get_features();
void mozilla_sampler_init();

namespace mozilla {

class NS_STACK_CLASS SamplerStackFrameRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFrameRAII(const char *aInfo) {
    mHandle = mozilla_sampler_call_enter(aInfo, this, false);
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
  SamplerStackFramePrintfRAII(const char *aDefault, const char *aFormat, ...) {
    if (mozilla_sampler_is_active()) {
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
      mHandle = mozilla_sampler_call_enter(mDest, this, true);
      va_end(args);
    } else {
      mHandle = mozilla_sampler_call_enter(aDefault);
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

class StackEntry
{
public:
  // Encode the address and aCopy by dropping the last bit of aStackAddress
  // and storing aCopy there.
  static const void* EncodeStackAddress(const void* aStackAddress, bool aCopy) {
    aStackAddress = reinterpret_cast<const void*>(
                      reinterpret_cast<uintptr_t>(aStackAddress) & ~0x1);
    if (!aCopy)
      aStackAddress = reinterpret_cast<const void*>(
                        reinterpret_cast<uintptr_t>(aStackAddress) | 0x1);
    return aStackAddress;
  }

  bool isCopyLabel() const volatile {
    return !((uintptr_t)mStackAddress & 0x1);
  }

  const char* mLabel;
  // Tagged pointer. Less significant bit used to
  // track if mLabel needs a copy. Note that we don't
  // need the last bit of the stack address for proper ordering.
  // Last bit 1 = Don't copy, Last bit 0 = Copy.
  const void* mStackAddress;
};

// the SamplerStack members are read by signal
// handlers, so the mutation of them needs to be signal-safe.
struct ProfileStack
{
public:
  ProfileStack()
    : mStackPointer(0)
    , mMarkerPointer(0)
    , mQueueClearMarker(false)
    , mStartJSSampling(false)
  { }

  void addMarker(const char *aMarker)
  {
    if (mQueueClearMarker) {
      clearMarkers();
    }
    if (!aMarker) {
      return; //discard
    }
    if (size_t(mMarkerPointer) == mozilla::ArrayLength(mMarkers)) {
      return; //array full, silently drop
    }
    mMarkers[mMarkerPointer] = aMarker;
    STORE_SEQUENCER();
    mMarkerPointer++;
  }

  // called within signal. Function must be reentrant
  const char* getMarker(int aMarkerId)
  {
    if (mQueueClearMarker) {
      clearMarkers();
    }
    if (aMarkerId < 0 ||
      static_cast<mozilla::sig_safe_t>(aMarkerId) >= mMarkerPointer) {
      return NULL;
    }
    return mMarkers[aMarkerId];
  }

  // called within signal. Function must be reentrant
  void clearMarkers()
  {
    mMarkerPointer = 0;
    mQueueClearMarker = false;
  }

  void push(const char *aName)
  {
    push(aName, NULL, false);
  }

  void push(const char *aName, void *aStackAddress, bool aCopy)
  {
    if (size_t(mStackPointer) >= mozilla::ArrayLength(mStack)) {
      mStackPointer++;
      return;
    }

    // Make sure we increment the pointer after the name has
    // been written such that mStack is always consistent.
    mStack[mStackPointer].mLabel = aName;
    mStack[mStackPointer].mStackAddress = StackEntry::EncodeStackAddress(aStackAddress, aCopy);

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

  void sampleRuntime(JSRuntime *runtime) {
    mRuntime = runtime;
    if (mStartJSSampling)
      installJSSampling();
  }
  void installJSSampling() {
    JS_STATIC_ASSERT(sizeof(mStack[0]) == sizeof(js::ProfileEntry));
    if (mRuntime) {
      js::SetRuntimeProfilingStack(mRuntime,
                                   (js::ProfileEntry*) mStack,
                                   (uint32_t*) &mStackPointer,
                                   mozilla::ArrayLength(mStack));
      mStartJSSampling = false;
    } else {
      mStartJSSampling = true;
    }
  }
  void uninstallJSSampling() {
    mStartJSSampling = false;
    if (mRuntime)
      js::SetRuntimeProfilingStack(mRuntime, NULL, NULL, 0);
  }

  // Keep a list of active checkpoints
  StackEntry volatile mStack[1024];
  // Keep a list of active markers to be applied to the next sample taken
  char const * volatile mMarkers[1024];
  volatile mozilla::sig_safe_t mStackPointer;
  volatile mozilla::sig_safe_t mMarkerPointer;
  // We don't want to modify _markers from within the signal so we allow
  // it to queue a clear operation.
  volatile mozilla::sig_safe_t mQueueClearMarker;
  // The runtime which is being sampled
  JSRuntime *mRuntime;
  // Start JS Profiling when possible
  bool mStartJSSampling;
};

inline ProfileStack* mozilla_profile_stack(void)
{
  if (!stack_key_initialized)
    return NULL;
  return tlsStack.get();
}

inline void* mozilla_sampler_call_enter(const char *aInfo, void *aFrameAddress, bool aCopy)
{
  // check if we've been initialized to avoid calling pthread_getspecific
  // with a null tlsStack which will return undefined results.
  if (!stack_key_initialized)
    return NULL;

  ProfileStack *stack = tlsStack.get();
  // we can't infer whether 'stack' has been initialized
  // based on the value of stack_key_intiailized because
  // 'stack' is only intialized when a thread is being
  // profiled.
  if (!stack) {
    return stack;
  }
  stack->push(aInfo, aFrameAddress, aCopy);

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

  ProfileStack *stack = (ProfileStack*)aHandle;
  stack->pop();
}

inline void mozilla_sampler_add_marker(const char *aMarker)
{
  ProfileStack *stack = tlsStack.get();
  if (!stack) {
    return;
  }
  stack->addMarker(aMarker);
}

