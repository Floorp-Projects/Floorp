/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* *************** SPS Sampler Information ****************
 *
 * SPS is an always on profiler that takes fast and low overheads samples
 * of the program execution using only userspace functionity for portability.
 * The goal of this module is to provide performance data in a generic
 * cross platform way without requiring custom tools or kernel support.
 *
 * Non goals: Support features that are platform specific or replace
 *            platform specific profilers.
 *
 * Samples are collected to form a timeline with optional timeline event (markers)
 * used for filtering.
 *
 * SPS collects samples in a platform independant way by using a speudo stack abstraction
 * of the real program stack by using 'sample stack frames'. When a sample is collected
 * all active sample stack frames and the program counter are recorded.
 */

/* *************** SPS Sampler File Format ****************
 *
 * Simple new line seperated tag format:
 * S      -> BOF tags EOF
 * tags   -> tag tags
 * tag    -> CHAR - STRING
 *
 * Tags:
 * 's' - Sample tag followed by the first stack frame followed by 0 or more 'c' tags.
 * 'c' - Continue Sample tag gives remaining tag element. If a 'c' tag is seen without
 *         a preceding 's' tag it should be ignored. This is to support the behavior
 *         of circular buffers.
 *         If the 'stackwalk' feature is enabled this tag will have the format
 *         'l-<library name>@<hex address>' and will expect an external tool to translate
 *         the tag into something readable through a symbolication processing step.
 * 'm' - Timeline marker. Zero or more may appear before a 's' tag.
 * 'l' - Information about the program counter library and address. Post processing
 *         can include function and source line. If built with leaf data enabled
 *         this tag will describe the last 'c' tag.
 * 'r' - Responsiveness tag following an 's' tag. Gives an indication on how well the
 *          application is responding to the event loop. Lower is better.
 * 't' - Elapse time since recording started.
 *
 */

#ifndef SAMPLER_H
#define SAMPLER_H

#include <stdint.h>
#include <stdarg.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "js/TypeDecls.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

namespace mozilla {
class TimeStamp;

namespace dom {
class Promise;
} // namespace dom

} // namespace mozilla

class nsIProfilerStartParams;

enum TracingMetadata {
  TRACING_DEFAULT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
  TRACING_EVENT,
  TRACING_EVENT_BACKTRACE,
  TRACING_TIMESTAMP
};

class ProfilerBacktrace;

struct ProfilerBacktraceDestructor
{
  void operator()(ProfilerBacktrace*);
};
using UniqueProfilerBacktrace =
  mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

#if !defined(MOZ_ENABLE_PROFILER_SPS)

// Insert a RAII in this scope to active a pseudo label. Any samples collected
// in this scope will contain this annotation. For dynamic strings use
// PROFILER_LABEL_PRINTF. Arguments must be string literals.
#define PROFILER_LABEL(name_space, info, category) do {} while (0)

// Similar to PROFILER_LABEL, PROFILER_LABEL_FUNC will push/pop the enclosing
// functon name as the pseudostack label.
#define PROFILER_LABEL_FUNC(category) do {} while (0)

// Format a dynamic string as a pseudo label. These labels will a considerable
// storage size in the circular buffer compared to regular labels. This function
// can be used to annotate custom information such as URL for the resource being
// decoded or the size of the paint.
#define PROFILER_LABEL_PRINTF(name_space, info, category, format, ...) do {} while (0)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike profiler_label that are
// only recorded if a sample is collected while it is active, marker will always
// be collected.
#define PROFILER_MARKER(info) do {} while (0)
#define PROFILER_MARKER_PAYLOAD(info, payload) do { mozilla::UniquePtr<ProfilerMarkerPayload> payloadDeletor(payload); } while (0)

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    TracingMetadata metaData = TRACING_DEFAULT) {}
static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    UniqueProfilerBacktrace aCause,
                                    TracingMetadata metaData = TRACING_DEFAULT) {}

// Initilize the profiler TLS, signal handlers on linux. If MOZ_PROFILER_STARTUP
// is set the profiler will be started. This call must happen before any other
// sampler calls. Particularly sampler_label/sampler_marker.
static inline void profiler_init(void* stackTop) {};

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all pseudo labels should have been popped.
static inline void profiler_shutdown() {};

// Start the profiler with the selected options. The samples will be
// recorded in a circular buffer.
//   "aProfileEntries" is an abstract size indication of how big
//       the profile's circular buffer should be. Multiply by 4
//       words to get the cost.
//   "aInterval" the sampling interval. The profiler will do its
//       best to sample at this interval. The profiler visualization
//       should represent the actual sampling accuracy.
static inline void profiler_start(int aProfileEntries, double aInterval,
                              const char** aFeatures, uint32_t aFeatureCount,
                              const char** aThreadNameFilters, uint32_t aFilterCount) {}

// Stop the profiler and discard the profile. Call 'profiler_save' before this
// to retrieve the profile.
static inline void profiler_stop() {}

// These functions pause and resume the profiler. While paused the profile will not
// take any samples and will not record any data into its buffers. The profiler
// remains fully initialized in this state. Timeline markers will still be stored.
// This feature will keep javascript profiling enabled, thus allowing toggling the
// profiler without invalidating the JIT.
static inline bool profiler_is_paused() { return false; }
static inline void profiler_pause() {}
static inline void profiler_resume() {}


// Immediately capture the current thread's call stack and return it
static inline UniqueProfilerBacktrace profiler_get_backtrace() { return nullptr; }
static inline void profiler_get_backtrace_noalloc(char *output, size_t outputSize) { return; }

// Free a ProfilerBacktrace returned by profiler_get_backtrace()
inline void ProfilerBacktraceDestructor::operator()(ProfilerBacktrace* aBacktrace) {}

static inline bool profiler_is_active() { return false; }

// Check if an external profiler feature is active.
// Supported:
//  * gpu
static inline bool profiler_feature_active(const char*) { return false; }

// Internal-only. Used by the event tracer.
static inline void profiler_responsiveness(const mozilla::TimeStamp& aTime) {}

// Internal-only.
static inline void profiler_set_frame_number(int frameNumber) {}

// Get the profile encoded as a JSON string.
static inline mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0) {
  return nullptr;
}

// Get the profile encoded as a JSON object.
static inline JSObject* profiler_get_profile_jsobject(JSContext* aCx,
                                                      double aSinceTime = 0) {
  return nullptr;
}

// Get the profile encoded as a JSON object.
static inline void profiler_get_profile_jsobject_async(double aSinceTime = 0,
                                                       mozilla::dom::Promise* = 0) {}
static inline void profiler_get_start_params(int* aEntrySize,
                                             double* aInterval,
                                             mozilla::Vector<const char*>* aFilters,
                                             mozilla::Vector<const char*>* aFeatures) {}

// Get the profile and write it into a file
static inline void profiler_save_profile_to_file(char* aFilename) { }

// Get the features supported by the profiler that are accepted by profiler_init.
// Returns a null terminated char* array.
static inline char** profiler_get_features() { return nullptr; }

// Get information about the current buffer status.
// Retursn (using outparams) the current write position in the buffer,
// the total size of the buffer, and the generation of the buffer.
// This information may be useful to a user-interface displaying the
// current status of the profiler, allowing the user to get a sense
// for how fast the buffer is being written to, and how much
// data is visible.
static inline void profiler_get_buffer_info(uint32_t *aCurrentPosition,
                                            uint32_t *aTotalSize,
                                            uint32_t *aGeneration)
{
  *aCurrentPosition = 0;
  *aTotalSize = 0;
  *aGeneration = 0;
}

// Discard the profile, throw away the profile and notify 'profiler-locked'.
// This function is to be used when entering private browsing to prevent
// the profiler from collecting sensitive data.
static inline void profiler_lock() {}

// Re-enable the profiler and notify 'profiler-unlocked'.
static inline void profiler_unlock() {}

static inline void profiler_register_thread(const char* name, void* guessStackTop) {}
static inline void profiler_unregister_thread() {}

// These functions tell the profiler that a thread went to sleep so that we can avoid
// sampling it while it's sleeping. Calling profiler_sleep_start() twice without
// profiler_sleep_end() is an error.
static inline void profiler_sleep_start() {}
static inline void profiler_sleep_end() {}
static inline bool profiler_is_sleeping() { return false; }

// Call by the JSRuntime's operation callback. This is used to enable
// profiling on auxilerary threads.
static inline void profiler_js_operation_callback() {}

static inline double profiler_time() { return 0; }
static inline double profiler_time(const mozilla::TimeStamp& aTime) { return 0; }

static inline bool profiler_in_privacy_mode() { return false; }

static inline void profiler_log(const char *str) {}
static inline void profiler_log(const char *fmt, va_list args) {}

#else

#include <stdlib.h>
#include <signal.h>
#include "js/ProfilingStack.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ThreadLocal.h"
#include "nscore.h"
#include "PseudoStack.h"
#include "ProfilerBacktrace.h"

// Make sure that we can use std::min here without the Windows headers messing with us.
#ifdef min
#undef min
#endif

class GeckoSampler;
class nsISupports;
class ProfilerMarkerPayload;

namespace mozilla {
class TimeStamp;

namespace dom {
class Promise;
} // namespace dom
} // namespace mozilla


extern MOZ_THREAD_LOCAL(PseudoStack *) tlsPseudoStack;
extern MOZ_THREAD_LOCAL(GeckoSampler *) tlsTicker;
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

// Returns a handle to pass on exit. This can check that we are popping the
// correct callstack.
inline void* mozilla_sampler_call_enter(const char *aInfo, js::ProfileEntry::Category aCategory,
                                        void *aFrameAddress = nullptr, bool aCopy = false,
                                        uint32_t line = 0);

inline void mozilla_sampler_call_exit(void* handle);

void mozilla_sampler_add_marker(const char *aInfo,
                                ProfilerMarkerPayload *aPayload = nullptr);

void mozilla_sampler_start(int aEntries, double aInterval,
                           const char** aFeatures, uint32_t aFeatureCount,
                           const char** aThreadNameFilters, uint32_t aFilterCount);

void mozilla_sampler_stop();

bool mozilla_sampler_is_paused();
void mozilla_sampler_pause();
void mozilla_sampler_resume();

UniqueProfilerBacktrace mozilla_sampler_get_backtrace();
void mozilla_sampler_get_backtrace_noalloc(char *output, size_t outputSize);

bool mozilla_sampler_is_active();

bool mozilla_sampler_feature_active(const char* aName);

void mozilla_sampler_responsiveness(const mozilla::TimeStamp& time);

void mozilla_sampler_frame_number(int frameNumber);

mozilla::UniquePtr<char[]> mozilla_sampler_get_profile(double aSinceTime);

JSObject *mozilla_sampler_get_profile_data(JSContext* aCx, double aSinceTime);
void mozilla_sampler_get_profile_data_async(double aSinceTime,
                                            mozilla::dom::Promise* aPromise);
MOZ_EXPORT
void mozilla_sampler_save_profile_to_file_async(double aSinceTime,
                                                const char* aFileName);
void mozilla_sampler_get_profiler_start_params(int* aEntrySize,
                                               double* aInterval,
                                               mozilla::Vector<const char*>* aFilters,
                                               mozilla::Vector<const char*>* aFeatures);
void mozilla_sampler_get_gatherer(nsISupports** aRetVal);

// Make this function easily callable from a debugger in a build without
// debugging information (work around http://llvm.org/bugs/show_bug.cgi?id=22211)
extern "C" {
  void mozilla_sampler_save_profile_to_file(const char* aFilename);
}

const char** mozilla_sampler_get_features();

void mozilla_sampler_get_buffer_info(uint32_t *aCurrentPosition, uint32_t *aTotalSize,
                                     uint32_t *aGeneration);

void mozilla_sampler_init(void* stackTop);

void mozilla_sampler_shutdown();

// Lock the profiler. When locked the profiler is (1) stopped,
// (2) profile data is cleared, (3) profiler-locked is fired.
// This is used to lock down the profiler during private browsing
void mozilla_sampler_lock();

// Unlock the profiler, leaving it stopped and fires profiler-unlocked.
void mozilla_sampler_unlock();

// Register/unregister threads with the profiler
bool mozilla_sampler_register_thread(const char* name, void* stackTop);
void mozilla_sampler_unregister_thread();

void mozilla_sampler_sleep_start();
void mozilla_sampler_sleep_end();
bool mozilla_sampler_is_sleeping();

double mozilla_sampler_time();
double mozilla_sampler_time(const mozilla::TimeStamp& aTime);

void mozilla_sampler_tracing(const char* aCategory, const char* aInfo,
                             TracingMetadata aMetaData);

void mozilla_sampler_tracing(const char* aCategory, const char* aInfo,
                             UniqueProfilerBacktrace aCause,
                             TracingMetadata aMetaData);

void mozilla_sampler_log(const char *fmt, va_list args);

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
UniqueProfilerBacktrace profiler_get_backtrace()
{
  return mozilla_sampler_get_backtrace();
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

static inline
bool profiler_is_sleeping()
{
  return mozilla_sampler_is_sleeping();
}

static inline
void profiler_js_operation_callback()
{
  PseudoStack *stack = tlsPseudoStack.get();
  if (!stack) {
    return;
  }

  stack->jsOperationCallback();
}

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
                                    UniqueProfilerBacktrace aCause,
                                    TracingMetadata aMetaData = TRACING_DEFAULT)
{
  // Don't insert a marker if we're not profiling to avoid
  // the heap copy (malloc).
  if (!stack_key_initialized || !profiler_is_active()) {
    return;
  }

  mozilla_sampler_tracing(aCategory, aInfo, mozilla::Move(aCause), aMetaData);
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

class MOZ_RAII SamplerStackFrameRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFrameRAII(const char *aInfo,
    js::ProfileEntry::Category aCategory, uint32_t line
    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mHandle = mozilla_sampler_call_enter(aInfo, aCategory, this, false, line);
  }
  ~SamplerStackFrameRAII() {
    mozilla_sampler_call_exit(mHandle);
  }
private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  void* mHandle;
};

static const int SAMPLER_MAX_STRING = 128;
class MOZ_RAII SamplerStackFramePrintfRAII {
public:
  // we only copy the strings at save time, so to take multiple parameters we'd need to copy them then.
  SamplerStackFramePrintfRAII(const char *aInfo,
    js::ProfileEntry::Category aCategory, uint32_t line, const char *aFormat, ...)
    : mHandle(nullptr)
  {
    if (profiler_is_active() && !profiler_in_privacy_mode()) {
      va_list args;
      va_start(args, aFormat);
      char buff[SAMPLER_MAX_STRING];

      // We have to use seperate printf's because we're using
      // the vargs.
      VsprintfLiteral(buff, aFormat, args);
      SprintfLiteral(mDest, "%s %s", aInfo, buff);

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

#endif

namespace mozilla {

class MOZ_RAII GeckoProfilerInitRAII {
public:
  explicit GeckoProfilerInitRAII(void* stackTop) {
    profiler_init(stackTop);
  }
  ~GeckoProfilerInitRAII() {
    profiler_shutdown();
  }
};

class MOZ_RAII GeckoProfilerSleepRAII {
public:
  GeckoProfilerSleepRAII() {
    profiler_sleep_start();
  }
  ~GeckoProfilerSleepRAII() {
    profiler_sleep_end();
  }
};

/**
 * Temporarily wake up the profiler while servicing events such as
 * Asynchronous Procedure Calls (APCs).
 */
class MOZ_RAII GeckoProfilerWakeRAII {
public:
  GeckoProfilerWakeRAII()
    : mIssuedWake(profiler_is_sleeping())
  {
    if (mIssuedWake) {
      profiler_sleep_end();
    }
  }
  ~GeckoProfilerWakeRAII() {
    if (mIssuedWake) {
      MOZ_ASSERT(!profiler_is_sleeping());
      profiler_sleep_start();
    }
  }
private:
  bool mIssuedWake;
};

class MOZ_RAII GeckoProfilerTracingRAII {
public:
  GeckoProfilerTracingRAII(const char* aCategory, const char* aInfo,
                           UniqueProfilerBacktrace aBacktrace
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mInfo(aInfo)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mInfo, Move(aBacktrace), TRACING_INTERVAL_START);
  }

  GeckoProfilerTracingRAII(const char* aCategory, const char* aInfo
                           MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mInfo(aInfo)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory, mInfo, TRACING_INTERVAL_START);
  }

  ~GeckoProfilerTracingRAII() {
    profiler_tracing(mCategory, mInfo, TRACING_INTERVAL_END);
  }

protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mCategory;
  const char* mInfo;
};

/**
 * Convenience class to register and unregister a thread with the profiler.
 * Needs to be the first object on the stack of the thread.
 */
class MOZ_STACK_CLASS AutoProfilerRegister final
{
public:
  explicit AutoProfilerRegister(const char* aName)
  {
    profiler_register_thread(aName, this);
  }
  ~AutoProfilerRegister()
  {
    profiler_unregister_thread();
  }
private:
  AutoProfilerRegister(const AutoProfilerRegister&) = delete;
  AutoProfilerRegister& operator=(const AutoProfilerRegister&) = delete;
};

} // namespace mozilla

#endif // ifndef SAMPLER_H
