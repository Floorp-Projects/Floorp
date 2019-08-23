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

#include "GeckoProfiler.h"

#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "nsString.h"

#include <functional>
#include <stdint.h>

extern mozilla::LazyLogModule gProfilerLog;

// These are for MOZ_LOG="prof:3" or higher. It's the default logging level for
// the profiler, and should be used sparingly.
#define LOG_TEST MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Info)
#define LOG(arg, ...)                            \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Info, \
          ("[%d] " arg, profiler_current_process_id(), ##__VA_ARGS__))

// These are for MOZ_LOG="prof:4" or higher. It should be used for logging that
// is somewhat more verbose than LOG.
#define DEBUG_LOG_TEST MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Debug)
#define DEBUG_LOG(arg, ...)                       \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Debug, \
          ("[%d] " arg, profiler_current_process_id(), ##__VA_ARGS__))

typedef uint8_t* Address;

// ----------------------------------------------------------------------------
// Miscellaneous

class PlatformData;

// We can't new/delete the type safely without defining it
// (-Wdelete-incomplete).  Use these to hide the details from clients.
struct PlatformDataDestructor {
  void operator()(PlatformData*);
};

typedef mozilla::UniquePtr<PlatformData, PlatformDataDestructor>
    UniquePlatformData;
UniquePlatformData AllocPlatformData(int aThreadId);

namespace mozilla {
class JSONWriter;
}
void AppendSharedLibraries(mozilla::JSONWriter& aWriter);

// Convert the array of strings to a bitfield.
uint32_t ParseFeaturesFromStringArray(const char** aFeatures,
                                      uint32_t aFeatureCount,
                                      bool aIsStartup = false);

void profiler_get_profile_json_into_lazily_allocated_buffer(
    const std::function<char*(size_t)>& aAllocator, double aSinceTime,
    bool aIsShuttingDown);

// Flags to conveniently track various JS instrumentations.
enum class JSInstrumentationFlags {
  StackSampling = 0x1,
  TrackOptimizations = 0x2,
  TraceLogging = 0x4,
  Allocations = 0x8
};

// Record an exit profile from a child process.
void profiler_received_exit_profile(const nsCString& aExitProfile);

// Extract all received exit profiles that have not yet expired (i.e., they
// still intersect with this process' buffer range).
mozilla::Vector<nsCString> profiler_move_exit_profiles();

// If the "MOZ_PROFILER_SYMBOLICATE" env-var is set, we return a new
// ProfilerCodeAddressService object to use for local symbolication of profiles.
// This is off by default, and mainly intended for local development.
mozilla::UniquePtr<ProfilerCodeAddressService>
profiler_code_address_service_for_presymbolication();

#endif /* ndef TOOLS_PLATFORM_H_ */
