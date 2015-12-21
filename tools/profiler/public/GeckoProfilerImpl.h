/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "GeckoProfiler.h"

#ifndef TOOLS_SPS_SAMPLER_H_
#define TOOLS_SPS_SAMPLER_H_

#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include "mozilla/ThreadLocal.h"
#include "mozilla/Assertions.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/UniquePtr.h"
#ifndef SPS_STANDALONE
#include "nscore.h"
#include "nsISupports.h"
#endif
#include "GeckoProfilerFunc.h"
#include "PseudoStack.h"
#include "ProfilerBacktrace.h"

/* QT has a #define for the word "slots" and jsfriendapi.h has a struct with
 * this variable name, causing compilation problems. Alleviate this for now by
 * removing this #define */
#ifdef MOZ_WIDGET_QT
#undef slots
#endif

// Make sure that we can use std::min here without the Windows headers messing with us.
#ifdef min
#undef min
#endif

class GeckoSampler;

namespace mozilla {
class TimeStamp;
} // namespace mozilla

extern mozilla::ThreadLocal<PseudoStack *> tlsPseudoStack;
extern mozilla::ThreadLocal<GeckoSampler *> tlsTicker;
extern mozilla::ThreadLocal<void *> tlsStackTop;
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

static inline
void profiler_init(void* stackTop)
{
  mozilla_sampler_init(stackTop);
}

static inline
void profiler_shutdown()
{
  mozilla_sampler_shutdown();
}

static inline
void profiler_start(int aProfileEntries, double aInterval,
                    const char** aFeatures, uint32_t aFeatureCount,
                    const char** aThreadNameFilters, uint32_t aFilterCount)
{
  mozilla_sampler_start(aProfileEntries, aInterval, aFeatures, aFeatureCount, aThreadNameFilters, aFilterCount);
}

static inline
void profiler_stop()
{
  mozilla_sampler_stop();
}

static inline
bool profiler_is_paused()
{
  return mozilla_sampler_is_paused();
}

static inline
void profiler_pause()
{
  mozilla_sampler_pause();
}

static inline
void profiler_resume()
{
  mozilla_sampler_resume();
}

static inline
ProfilerBacktrace* profiler_get_backtrace()
{
  return mozilla_sampler_get_backtrace();
}

static inline
void profiler_free_backtrace(ProfilerBacktrace* aBacktrace)
{
  mozilla_sampler_free_backtrace(aBacktrace);
}

static inline
void profiler_get_backtrace_noalloc(char *output, size_t outputSize)
{
  return mozilla_sampler_get_backtrace_noalloc(output, outputSize);
}

static inline
bool profiler_is_active()
{
  return mozilla_sampler_is_active();
}

static inline
bool profiler_feature_active(const char* aName)
{
  return mozilla_sampler_feature_active(aName);
}

static inline
void profiler_responsiveness(const mozilla::TimeStamp& aTime)
{
  mozilla_sampler_responsiveness(aTime);
}

static inline
void profiler_set_frame_number(int frameNumber)
{
  return mozilla_sampler_frame_number(frameNumber);
}

static inline
mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0)
{
  return mozilla_sampler_get_profile(aSinceTime);
}

#ifndef SPS_STANDALONE
static inline
JSObject* profiler_get_profile_jsobject(JSContext* aCx, double aSinceTime = 0)
{
  return mozilla_sampler_get_profile_data(aCx, aSinceTime);
}

static inline
void profiler_get_profile_jsobject_async(double aSinceTime = 0,
                                         mozilla::dom::Promise* aPromise = 0)
{
  mozilla_sampler_get_profile_data_async(aSinceTime, aPromise);
}

static inline
void profiler_get_start_params(int* aEntrySize,
                               double* aInterval,
                               mozilla::Vector<const char*>* aFilters,
                               mozilla::Vector<const char*>* aFeatures)
{
  mozilla_sampler_get_profiler_start_params(aEntrySize, aInterval, aFilters, aFeatures);
}

static inline
void profiler_get_gatherer(nsISupports** aRetVal)
{
  mozilla_sampler_get_gatherer(aRetVal);
}

#endif

static inline
void profiler_save_profile_to_file(const char* aFilename)
{
  return mozilla_sampler_save_profile_to_file(aFilename);
}

static inline
const char** profiler_get_features()
{
  return mozilla_sampler_get_features();
}

static inline
void profiler_get_buffer_info(uint32_t *aCurrentPosition, uint32_t *aTotalSize,
                              uint32_t *aGeneration)
{
  return mozilla_sampler_get_buffer_info(aCurrentPosition, aTotalSize, aGeneration);
}

static inline
void profiler_lock()
{
  return mozilla_sampler_lock();
}

static inline
void profiler_unlock()
{
  return mozilla_sampler_unlock();
}

static inline
void profiler_register_thread(const char* name, void* guessStackTop)
{
  mozilla_sampler_register_thread(name, guessStackTop);
}

static inline
void profiler_unregister_thread()
{
  mozilla_sampler_unregister_thread();
}

static inline
void profiler_sleep_start()
{
  mozilla_sampler_sleep_start();
}

static inline
void profiler_sleep_end()
{
  mozilla_sampler_sleep_end();
}

#ifndef SPS_STANDALONE
static inline
void profiler_js_operation_callback()
{
  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->jsOperationCallback();
}
#endif

static inline
double profiler_time()
{
  return mozilla_sampler_time();
}

static inline
double profiler_time(const mozilla::TimeStamp& aTime)
{
  return mozilla_sampler_time(aTime);
}

static inline
bool profiler_in_privacy_mode()
{
  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return false;
  }
  return stack->mPrivacyMode;
}

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    ProfilerBacktrace* aCause,
                                    TracingMetadata aMetaData = TRACING_DEFAULT)
{
  // Don't insert a marker if we're not profiling to avoid
  // the heap copy (malloc).
  if (!stack_key_initialized || !profiler_is_active()) {
    delete aCause;
    return;
  }

  mozilla_sampler_tracing(aCategory, aInfo, aCause, aMetaData);
}

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    TracingMetadata aMetaData = TRACING_DEFAULT)
{
  if (!stack_key_initialized)
    return;

  // Don't insert a marker if we're not profiling to avoid
  // the heap copy (malloc).
  if (!profiler_is_active()) {
    return;
  }

  mozilla_sampler_tracing(aCategory, aInfo, aMetaData);
}

#define SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, line) SAMPLER_APPEND_LINE_NUMBER_PASTE(id, line)
#define SAMPLER_APPEND_LINE_NUMBER(id) SAMPLER_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

// Uncomment this to turn on systrace or build with
// ac_add_options --enable-systace
//#define MOZ_USE_SYSTRACE
#ifdef MOZ_USE_SYSTRACE
#ifndef ATRACE_TAG
# define ATRACE_TAG ATRACE_TAG_ALWAYS
#endif
// We need HAVE_ANDROID_OS to be defined for Trace.h.
// If its not set we will set it temporary and remove it.
# ifndef HAVE_ANDROID_OS
#   define HAVE_ANDROID_OS
#   define REMOVE_HAVE_ANDROID_OS
# endif
// Android source code will include <cutils/trace.h> before this. There is no
// HAVE_ANDROID_OS defined in Firefox OS build at that time. Enabled it globally
// will cause other build break. So atrace_begin and atrace_end are not defined.
// It will cause a build-break when we include <utils/Trace.h>. Use undef
// _LIBS_CUTILS_TRACE_H will force <cutils/trace.h> to define atrace_begin and
// atrace_end with defined HAVE_ANDROID_OS again. Then there is no build-break.
# undef _LIBS_CUTILS_TRACE_H
# include <utils/Trace.h>
# define MOZ_PLATFORM_TRACING(name) android::ScopedTrace SAMPLER_APPEND_LINE_NUMBER(scopedTrace)(ATRACE_TAG, name);
# ifdef REMOVE_HAVE_ANDROID_OS
#  undef HAVE_ANDROID_OS
#  undef REMOVE_HAVE_ANDROID_OS
# endif
#else
# define MOZ_PLATFORM_TRACING(name)
#endif

// we want the class and function name but can't easily get that using preprocessor macros
// __func__ doesn't have the class name and __PRETTY_FUNCTION__ has the parameters

#define PROFILER_LABEL(name_space, info, category) MOZ_PLATFORM_TRACING(name_space "::" info) mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, category, __LINE__)
#define PROFILER_LABEL_FUNC(category) MOZ_PLATFORM_TRACING(SAMPLE_FUNCTION_NAME) mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(SAMPLE_FUNCTION_NAME, category, __LINE__)
#define PROFILER_LABEL_PRINTF(name_space, info, category, ...) MOZ_PLATFORM_TRACING(name_space "::" info) mozilla::SamplerStackFramePrintfRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, category, __LINE__, __VA_ARGS__)

#define PROFILER_MARKER(info) mozilla_sampler_add_marker(info)
#define PROFILER_MARKER_PAYLOAD(info, payload) mozilla_sampler_add_marker(info, payload)
#define PROFILER_MAIN_THREAD_MARKER(info)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla_sampler_add_marker(info)

#define PROFILER_MAIN_THREAD_LABEL(name_space, info, category)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla::SamplerStackFrameRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, category, __LINE__)
#define PROFILER_MAIN_THREAD_LABEL_PRINTF(name_space, info, category, ...)  MOZ_ASSERT(NS_IsMainThread(), "This can only be called on the main thread"); mozilla::SamplerStackFramePrintfRAII SAMPLER_APPEND_LINE_NUMBER(sampler_raii)(name_space "::" info, category, __LINE__, __VA_ARGS__)


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

// In the case of profiler_get_backtrace we know that we only need enough space
// for a single backtrace.
#define GET_BACKTRACE_DEFAULT_ENTRY 1000

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

namespace mozilla {

class MOZ_RAII GeckoProfilerTracingRAII {
public:
  GeckoProfilerTracingRAII(const char* aCategory, const char* aInfo,
                           mozilla::UniquePtr<ProfilerBacktrace> aBacktrace
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mInfo(aInfo)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mInfo, aBacktrace.release(), TRACING_INTERVAL_START);
  }

  ~GeckoProfilerTracingRAII() {
    profiler_tracing(mCategory, mInfo, TRACING_INTERVAL_END);
  }

protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mCategory;
  const char* mInfo;
};

class MOZ_STACK_CLASS SamplerStackFrameRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFrameRAII(const char *aInfo,
    js::ProfileEntry::Category aCategory, uint32_t line)
  {
    mHandle = mozilla_sampler_call_enter(aInfo, aCategory, this, false, line);
  }
  ~SamplerStackFrameRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  void* mHandle;
};

static const int SAMPLER_MAX_STRING = 128;
class MOZ_STACK_CLASS SamplerStackFramePrintfRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFramePrintfRAII(const char *aInfo,
    js::ProfileEntry::Category aCategory, uint32_t line, const char *aFormat, ...)
  {
    if (profiler_is_active() && !profiler_in_privacy_mode()) {
      va_list args;
      va_start(args, aFormat);
      char buff[SAMPLER_MAX_STRING];

      // We have to use seperate printf's because we're using
      // the vargs.
#if _MSC_VER
      _vsnprintf(buff, SAMPLER_MAX_STRING, aFormat, args);
      _snprintf(mDest, SAMPLER_MAX_STRING, "%s %s", aInfo, buff);
#else
      ::vsnprintf(buff, SAMPLER_MAX_STRING, aFormat, args);
      ::snprintf(mDest, SAMPLER_MAX_STRING, "%s %s", aInfo, buff);
#endif
      mHandle = mozilla_sampler_call_enter(mDest, aCategory, this, true, line);
      va_end(args);
    } else {
      mHandle = mozilla_sampler_call_enter(aInfo, aCategory, this, false, line);
    }
  }
  ~SamplerStackFramePrintfRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  char mDest[SAMPLER_MAX_STRING];
  void* mHandle;
};

} // namespace mozilla

inline PseudoStack* mozilla_get_pseudo_stack(void)
{
  if (!stack_key_initialized)
    return nullptr;
  return tlsPseudoStack.get();
}

inline void* mozilla_sampler_call_enter(const char *aInfo,
  js::ProfileEntry::Category aCategory, void *aFrameAddress, bool aCopy, uint32_t line)
{
  // check if we've been initialized to avoid calling pthread_getspecific
  // with a null tlsStack which will return undefined results.
  if (!stack_key_initialized)
    return nullptr;

  PseudoStack *stack = tlsPseudoStack.get();
  // we can't infer whether 'stack' has been initialized
  // based on the value of stack_key_intiailized because
  // 'stack' is only intialized when a thread is being
  // profiled.
  if (!stack) {
    return stack;
  }
  stack->push(aInfo, aCategory, aFrameAddress, aCopy, line);

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
  stack->popAndMaybeDelete();
}

void mozilla_sampler_add_marker(const char *aMarker, ProfilerMarkerPayload *aPayload);

static inline
void profiler_log(const char *str)
{
  profiler_tracing("log", str, TRACING_EVENT);
}

static inline
void profiler_log(const char *fmt, va_list args)
{
  mozilla_sampler_log(fmt, args);
}

#endif /* ndef TOOLS_SPS_SAMPLER_H_ */
