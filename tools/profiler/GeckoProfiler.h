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

#include "mozilla/NullPtr.h"
#include "js/TypeDecls.h"

namespace mozilla {
class TimeStamp;
}

enum TracingMetadata {
  TRACING_DEFAULT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END
};

#ifndef MOZ_ENABLE_PROFILER_SPS

#include <stdint.h>

// Insert a RAII in this scope to active a pseudo label. Any samples collected
// in this scope will contain this annotation. For dynamic strings use
// PROFILER_LABEL_PRINTF. Arguments must be string literals.
#define PROFILER_LABEL(name_space, info) do {} while (0)

// Format a dynamic string as a pseudo label. These labels will a considerable
// storage size in the circular buffer compared to regular labels. This function
// can be used to annotate custom information such as URL for the resource being
// decoded or the size of the paint.
#define PROFILER_LABEL_PRINTF(name_space, info, format, ...) do {} while (0)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike profiler_label that are
// only recorded if a sample is collected while it is active, marker will always
// be collected.
#define PROFILER_MARKER(info) do {} while (0)
#define PROFILER_MARKER_PAYLOAD(info, payload) do {} while (0)

// Main thread specilization to avoid TLS lookup for performance critical use.
#define PROFILER_MAIN_THREAD_LABEL(name_space, info) do {} while (0)
#define PROFILER_MAIN_THREAD_LABEL_PRINTF(name_space, info, format, ...) do {} while (0)

static inline void profiler_tracing(const char* aCategory, const char* aInfo,
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

class ProfilerBacktrace;

// Immediately capture the current thread's call stack and return it
static inline ProfilerBacktrace* profiler_get_backtrace() { return nullptr; }

// Free a ProfilerBacktrace returned by profiler_get_backtrace()
static inline void profiler_free_backtrace(ProfilerBacktrace* aBacktrace) {}

static inline bool profiler_is_active() { return false; }

// Internal-only. Used by the event tracer.
static inline void profiler_responsiveness(const mozilla::TimeStamp& aTime) {}

// Internal-only. Used by the event tracer.
static inline double* profiler_get_responsiveness() { return nullptr; }

// Internal-only.
static inline void profiler_set_frame_number(int frameNumber) {}

// Get the profile encoded as a JSON string.
static inline char* profiler_get_profile() { return nullptr; }

// Get the profile encoded as a JSON object.
static inline JSObject* profiler_get_profile_jsobject(JSContext* aCx) { return nullptr; }

// Get the features supported by the profiler that are accepted by profiler_init.
// Returns a null terminated char* array.
static inline char** profiler_get_features() { return nullptr; }

// Print the current location to the console. This functill will do it best effort
// to show the profiler's combined js/c++ if the profiler is running. Note that
// printing the location require symbolicating which is very slow.
static inline void profiler_print_location() {}

// Discard the profile, throw away the profile and notify 'profiler-locked'.
// This function is to be used when entering private browsing to prevent
// the profiler from collecting sensitive data.
static inline void profiler_lock() {}

// Re-enable the profiler and notify 'profiler-unlocked'.
static inline void profiler_unlock() {}

static inline void profiler_register_thread(const char* name, void* stackTop) {}
static inline void profiler_unregister_thread() {}

// These functions tell the profiler that a thread went to sleep so that we can avoid
// sampling it while it's sleeping. Calling profiler_sleep_start() twice without
// profiler_sleep_end() is an error.
static inline void profiler_sleep_start() {}
static inline void profiler_sleep_end() {}

// Call by the JSRuntime's operation callback. This is used to enable
// profiling on auxilerary threads.
static inline void profiler_js_operation_callback() {}

static inline double profiler_time() { return 0; }
static inline double profiler_time(const mozilla::TimeStamp& aTime) { return 0; }

static inline bool profiler_in_privacy_mode() { return false; }

#else

#include "GeckoProfilerImpl.h"

#endif

class GeckoProfilerInitRAII {
public:
  GeckoProfilerInitRAII(void* stackTop) {
    profiler_init(stackTop);
  }
  ~GeckoProfilerInitRAII() {
    profiler_shutdown();
  }
};

class GeckoProfilerSleepRAII {
public:
  GeckoProfilerSleepRAII() {
    profiler_sleep_start();
  }
  ~GeckoProfilerSleepRAII() {
    profiler_sleep_end();
  }
};

#endif // ifndef SAMPLER_H
