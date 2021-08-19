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

#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ProfileBufferEntrySerialization.h"
#include "mozilla/ProfilerUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "nsString.h"

#include <cstddef>
#include <cstdint>
#include <functional>

class ProfilerCodeAddressService;

namespace mozilla {
struct SymbolTable;
}

extern mozilla::LazyLogModule gProfilerLog;

// These are for MOZ_LOG="prof:3" or higher. It's the default logging level for
// the profiler, and should be used sparingly.
#define LOG_TEST MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Info)
#define LOG(arg, ...)                            \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Info, \
          ("[%" PRIu64 "d] " arg,                \
           uint64_t(profiler_current_process_id().ToNumber()), ##__VA_ARGS__))

// These are for MOZ_LOG="prof:4" or higher. It should be used for logging that
// is somewhat more verbose than LOG.
#define DEBUG_LOG_TEST MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Debug)
#define DEBUG_LOG(arg, ...)                       \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Debug, \
          ("[%" PRIu64 "] " arg,                  \
           uint64_t(profiler_current_process_id().ToNumber()), ##__VA_ARGS__))

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
UniquePlatformData AllocPlatformData(ProfilerThreadId aThreadId);

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
  TraceLogging = 0x2,
  Allocations = 0x4,
};

// Write out the information of the active profiling configuration.
void profiler_write_active_configuration(mozilla::JSONWriter& aWriter);

// Extract all received exit profiles that have not yet expired (i.e., they
// still intersect with this process' buffer range).
mozilla::Vector<nsCString> profiler_move_exit_profiles();

// If the "MOZ_PROFILER_SYMBOLICATE" env-var is set, we return a new
// ProfilerCodeAddressService object to use for local symbolication of profiles.
// This is off by default, and mainly intended for local development.
mozilla::UniquePtr<ProfilerCodeAddressService>
profiler_code_address_service_for_presymbolication();

extern "C" {
// This function is defined in the profiler rust module at
// tools/profiler/rust-helper. mozilla::SymbolTable and CompactSymbolTable
// have identical memory layout.
bool profiler_get_symbol_table(const char* debug_path, const char* breakpad_id,
                               mozilla::SymbolTable* symbol_table);

bool profiler_demangle_rust(const char* mangled, char* buffer, size_t len);
}

// For each running times value, call MACRO(index, name, unit, jsonProperty)
#define PROFILER_FOR_EACH_RUNNING_TIME(MACRO) \
  MACRO(0, ThreadCPU, Delta, threadCPUDelta)

// This class contains all "running times" such as CPU usage measurements.
// All measurements are listed in `PROFILER_FOR_EACH_RUNNING_TIME` above.
// Each measurement is optional and only takes a value when explicitly set.
// Two RunningTimes object may be subtracted, to get the difference between
// known values.
class RunningTimes {
 public:
  constexpr RunningTimes() = default;

  // Constructor with only a timestamp, useful when no measurements will be
  // taken.
  constexpr explicit RunningTimes(const mozilla::TimeStamp& aTimeStamp)
      : mPostMeasurementTimeStamp(aTimeStamp) {}

  constexpr void Clear() { *this = RunningTimes{}; }

  constexpr bool IsEmpty() const { return mKnownBits == 0; }

  // This should be called right after CPU measurements have been taken.
  void SetPostMeasurementTimeStamp(const mozilla::TimeStamp& aTimeStamp) {
    mPostMeasurementTimeStamp = aTimeStamp;
  }

  const mozilla::TimeStamp& PostMeasurementTimeStamp() const {
    return mPostMeasurementTimeStamp;
  }

  // Should be filled for any registered thread.

#define RUNNING_TIME_MEMBER(index, name, unit, jsonProperty)          \
  constexpr bool Is##name##unit##Known() const {                      \
    return (mKnownBits & mGot##name##unit) != 0;                      \
  }                                                                   \
                                                                      \
  constexpr void Clear##name##unit() {                                \
    m##name##unit = 0;                                                \
    mKnownBits &= ~mGot##name##unit;                                  \
  }                                                                   \
                                                                      \
  constexpr void Reset##name##unit(uint64_t a##name##unit) {          \
    m##name##unit = a##name##unit;                                    \
    mKnownBits |= mGot##name##unit;                                   \
  }                                                                   \
                                                                      \
  constexpr void Set##name##unit(uint64_t a##name##unit) {            \
    MOZ_ASSERT(!Is##name##unit##Known(), #name #unit " already set"); \
    Reset##name##unit(a##name##unit);                                 \
  }                                                                   \
                                                                      \
  constexpr mozilla::Maybe<uint64_t> Get##name##unit() const {        \
    if (Is##name##unit##Known()) {                                    \
      return mozilla::Some(m##name##unit);                            \
    }                                                                 \
    return mozilla::Nothing{};                                        \
  }

  PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_MEMBER)

#undef RUNNING_TIME_MEMBER

  // Take values from another RunningTimes.
  RunningTimes& TakeFrom(RunningTimes& aOther) {
    if (!aOther.IsEmpty()) {
#define RUNNING_TIME_TAKE(index, name, unit, jsonProperty)   \
  if (aOther.Is##name##unit##Known()) {                      \
    Set##name##unit(std::exchange(aOther.m##name##unit, 0)); \
  }

      PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_TAKE)

#undef RUNNING_TIME_TAKE

      aOther.mKnownBits = 0;
    }
    return *this;
  }

  // Difference from `aBefore` to `this`. Any unknown makes the result unknown.
  // PostMeasurementTimeStamp set to `this` PostMeasurementTimeStamp, to keep
  // the most recent timestamp associated with the end of the interval over
  // which the difference applies.
  RunningTimes operator-(const RunningTimes& aBefore) const {
    RunningTimes diff;
    diff.mPostMeasurementTimeStamp = mPostMeasurementTimeStamp;
#define RUNNING_TIME_SUB(index, name, unit, jsonProperty)           \
  if (Is##name##unit##Known() && aBefore.Is##name##unit##Known()) { \
    diff.Set##name##unit(m##name##unit - aBefore.m##name##unit);    \
  }

    PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_SUB)

#undef RUNNING_TIME_SUB
    return diff;
  }

 private:
  friend mozilla::ProfileBufferEntryWriter::Serializer<RunningTimes>;
  friend mozilla::ProfileBufferEntryReader::Deserializer<RunningTimes>;

  mozilla::TimeStamp mPostMeasurementTimeStamp;

  uint32_t mKnownBits = 0u;

#define RUNNING_TIME_MEMBER(index, name, unit, jsonProperty) \
  static constexpr uint32_t mGot##name##unit = 1u << index;  \
  uint64_t m##name##unit = 0;

  PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_MEMBER)

#undef RUNNING_TIME_MEMBER
};

template <>
struct mozilla::ProfileBufferEntryWriter::Serializer<RunningTimes> {
  static Length Bytes(const RunningTimes& aRunningTimes) {
    Length bytes = 0;

#define RUNNING_TIME_SERIALIZATION_BYTES(index, name, unit, jsonProperty) \
  if (aRunningTimes.Is##name##unit##Known()) {                            \
    bytes += ULEB128Size(aRunningTimes.m##name##unit);                    \
  }

    PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_SERIALIZATION_BYTES)

#undef RUNNING_TIME_SERIALIZATION_BYTES
    return ULEB128Size(aRunningTimes.mKnownBits) + bytes;
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const RunningTimes& aRunningTimes) {
    aEW.WriteULEB128(aRunningTimes.mKnownBits);

#define RUNNING_TIME_SERIALIZE(index, name, unit, jsonProperty) \
  if (aRunningTimes.Is##name##unit##Known()) {                  \
    aEW.WriteULEB128(aRunningTimes.m##name##unit);              \
  }

    PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_SERIALIZE)

#undef RUNNING_TIME_SERIALIZE
  }
};

template <>
struct mozilla::ProfileBufferEntryReader::Deserializer<RunningTimes> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       RunningTimes& aRunningTimes) {
    aRunningTimes = Read(aER);
  }

  static RunningTimes Read(ProfileBufferEntryReader& aER) {
    // Start with empty running times, everything is cleared.
    RunningTimes times;

    // This sets all the bits into mKnownBits, we don't need to modify it
    // further.
    times.mKnownBits = aER.ReadULEB128<uint32_t>();

    // For each member that should be known, read its value.
#define RUNNING_TIME_DESERIALIZE(index, name, unit, jsonProperty)           \
  if (times.Is##name##unit##Known()) {                                      \
    times.m##name##unit = aER.ReadULEB128<decltype(times.m##name##unit)>(); \
  }

    PROFILER_FOR_EACH_RUNNING_TIME(RUNNING_TIME_DESERIALIZE)

#undef RUNNING_TIME_DESERIALIZE

    return times;
  }
};

#endif /* ndef TOOLS_PLATFORM_H_ */
