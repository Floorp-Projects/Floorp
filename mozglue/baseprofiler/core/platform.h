// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of Google, Inc. nor the names of its contributors
//    may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#ifndef TOOLS_PLATFORM_H_
#define TOOLS_PLATFORM_H_

#include "PlatformMacros.h"

#include "BaseProfiler.h"

#include "mozilla/Atomics.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include <functional>
#include <stdint.h>
#include <string>

namespace mozilla {
namespace baseprofiler {
bool LogTest(int aLevelToTest);
void PrintToConsole(const char* aFmt, ...) MOZ_FORMAT_PRINTF(1, 2);
}  // namespace baseprofiler
}  // namespace mozilla

// These are for MOZ_BASE_PROFILER_LOGGING and above. It's the default logging
// level for the profiler, and should be used sparingly.
#define LOG_TEST ::mozilla::baseprofiler::LogTest(3)
#define LOG(arg, ...)                                                \
  do {                                                               \
    if (LOG_TEST) {                                                  \
      ::mozilla::baseprofiler::PrintToConsole(                       \
          "[I %d/%d] " arg "\n",                                     \
          int(::mozilla::baseprofiler::profiler_current_process_id() \
                  .ToNumber()),                                      \
          int(::mozilla::baseprofiler::profiler_current_thread_id()  \
                  .ToNumber()),                                      \
          ##__VA_ARGS__);                                            \
    }                                                                \
  } while (0)

// These are for MOZ_BASE_PROFILER_DEBUG_LOGGING. It should be used for logging
// that is somewhat more verbose than LOG.
#define DEBUG_LOG_TEST ::mozilla::baseprofiler::LogTest(4)
#define DEBUG_LOG(arg, ...)                                          \
  do {                                                               \
    if (DEBUG_LOG_TEST) {                                            \
      ::mozilla::baseprofiler::PrintToConsole(                       \
          "[D %d/%d] " arg "\n",                                     \
          int(::mozilla::baseprofiler::profiler_current_process_id() \
                  .ToNumber()),                                      \
          int(::mozilla::baseprofiler::profiler_current_thread_id()  \
                  .ToNumber()),                                      \
          ##__VA_ARGS__);                                            \
    }                                                                \
  } while (0)

// These are for MOZ_BASE_PROFILER_VERBOSE_LOGGING. It should be used for
// logging that is somewhat more verbose than DEBUG_LOG.
#define VERBOSE_LOG_TEST ::mozilla::baseprofiler::LogTest(5)
#define VERBOSE_LOG(arg, ...)                                        \
  do {                                                               \
    if (VERBOSE_LOG_TEST) {                                          \
      ::mozilla::baseprofiler::PrintToConsole(                       \
          "[V %d/%d] " arg "\n",                                     \
          int(::mozilla::baseprofiler::profiler_current_process_id() \
                  .ToNumber()),                                      \
          int(::mozilla::baseprofiler::profiler_current_thread_id()  \
                  .ToNumber()),                                      \
          ##__VA_ARGS__);                                            \
    }                                                                \
  } while (0)

namespace mozilla {

class JSONWriter;

namespace baseprofiler {

// If positive, skip stack-sampling in the sampler thread loop.
// Users should increment it atomically when samplings should be avoided, and
// later decrement it back. Multiple uses can overlap.
// There could be a sampling in progress when this is first incremented, so if
// it is critical to prevent any sampling, lock the profiler mutex instead.
// Relaxed ordering, because it's used to request that the profiler pause
// future sampling; this is not time critical, nor dependent on anything else.
extern mozilla::Atomic<int, mozilla::MemoryOrdering::Relaxed> gSkipSampling;

typedef uint8_t* Address;

class PlatformData;

// We can't new/delete the type safely without defining it
// (-Wdelete-incomplete).  Use these to hide the details from clients.
struct PlatformDataDestructor {
  void operator()(PlatformData*);
};

typedef UniquePtr<PlatformData, PlatformDataDestructor> UniquePlatformData;
UniquePlatformData AllocPlatformData(BaseProfilerThreadId aThreadId);

// Convert the array of strings to a bitfield.
uint32_t ParseFeaturesFromStringArray(const char** aFeatures,
                                      uint32_t aFeatureCount,
                                      bool aIsStartup = false);

// Flags to conveniently track various JS instrumentations.
enum class JSInstrumentationFlags {
  StackSampling = 0x1,
  Allocations = 0x2,
};

// Record an exit profile from a child process.
void profiler_received_exit_profile(const std::string& aExitProfile);

// Extract all received exit profiles that have not yet expired (i.e., they
// still intersect with this process' buffer range).
Vector<std::string> profiler_move_exit_profiles();

}  // namespace baseprofiler
}  // namespace mozilla

#endif /* ndef TOOLS_PLATFORM_H_ */
