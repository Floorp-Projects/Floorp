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

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#ifndef SPS_STANDALONE
#include "js/TypeDecls.h"
#endif
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

namespace mozilla {
class TimeStamp;

namespace dom {
class Promise;
} // namespace dom

} // namespace mozilla

#ifndef SPS_STANDALONE
class nsIProfilerStartParams;
#endif

enum TracingMetadata {
  TRACING_DEFAULT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
  TRACING_EVENT,
  TRACING_EVENT_BACKTRACE,
  TRACING_TIMESTAMP
};

#if !defined(MOZ_ENABLE_PROFILER_SPS)

#include <stdint.h>
#include <stdarg.h>

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

// Main thread specilization to avoid TLS lookup for performance critical use.
#define PROFILER_MAIN_THREAD_LABEL(name_space, info, category) do {} while (0)
#define PROFILER_MAIN_THREAD_LABEL_PRINTF(name_space, info, category, format, ...) do {} while (0)

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    TracingMetadata metaData = TRACING_DEFAULT) {}
class ProfilerBacktrace;

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
                                    ProfilerBacktrace* aCause,
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
static inline ProfilerBacktrace* profiler_get_backtrace() { return nullptr; }
static inline void profiler_get_backtrace_noalloc(char *output, size_t outputSize) { return; }

// Free a ProfilerBacktrace returned by profiler_get_backtrace()
static inline void profiler_free_backtrace(ProfilerBacktrace* aBacktrace) {}

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

#ifndef SPS_STANDALONE
// Get the profile encoded as a JSON object.
static inline void profiler_get_profile_jsobject_async(double aSinceTime = 0,
                                                       mozilla::dom::Promise* = 0) {}
static inline void profiler_get_start_params(int* aEntrySize,
                                             double* aInterval,
                                             mozilla::Vector<const char*>* aFilters,
                                             mozilla::Vector<const char*>* aFeatures) {}
#endif

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

#include "GeckoProfilerImpl.h"

#endif

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

#endif // ifndef SAMPLER_H
