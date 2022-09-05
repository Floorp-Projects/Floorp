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

#include "json/json.h"
#include "mozilla/Atomics.h"
#include "mozilla/BaseProfilerDetail.h"
#include "mozilla/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/ProfileBufferEntrySerialization.h"
#include "mozilla/ProfileJSONWriter.h"
#include "mozilla/ProfilerUtils.h"
#include "mozilla/ProgressLogger.h"
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
          ("[%" PRIu64 "] " arg,                 \
           uint64_t(profiler_current_process_id().ToNumber()), ##__VA_ARGS__))

// These are for MOZ_LOG="prof:4" or higher. It should be used for logging that
// is somewhat more verbose than LOG.
#define DEBUG_LOG_TEST MOZ_LOG_TEST(gProfilerLog, mozilla::LogLevel::Debug)
#define DEBUG_LOG(arg, ...)                       \
  MOZ_LOG(gProfilerLog, mozilla::LogLevel::Debug, \
          ("[%" PRIu64 "] " arg,                  \
           uint64_t(profiler_current_process_id().ToNumber()), ##__VA_ARGS__))

typedef uint8_t* Address;

// Stringify the given JSON value, in the most compact format.
// Note: Numbers are limited to a precision of 6 decimal digits, so that
// timestamps in ms have a precision in ns.
Json::String ToCompactString(const Json::Value& aJsonValue);

// Profiling log stored in a Json::Value. The actual log only exists while the
// profiler is running, and will be inserted at the end of the JSON profile.
class ProfilingLog {
 public:
  // These will be called by ActivePS when the profiler starts/stops.
  static void Init();
  static void Destroy();

  // Access the profiling log JSON object, in order to modify it.
  // Only calls the given function if the profiler is active.
  // Thread-safe. But `aF` must not call other locking profiler functions.
  // This is intended to capture some internal logging that doesn't belong in
  // other places like markers. The log is accessible through the JS console on
  // profiler.firefox.com, in the `profile.profilingLog` object; the data format
  // is intentionally not defined, and not intended to be shown in the
  // front-end.
  // Please use caution not to output too much data.
  template <typename F>
  static void Access(F&& aF) {
    mozilla::baseprofiler::detail::BaseProfilerAutoLock lock{gMutex};
    if (gLog) {
      std::forward<F>(aF)(*gLog);
    }
  }

#define DURATION_JSON_SUFFIX "_ms"

  // Convert a TimeDuration to the value to be stored in the log.
  // Use DURATION_JSON_SUFFIX as suffix in the property name.
  static Json::Value Duration(const mozilla::TimeDuration& aDuration) {
    return Json::Value{aDuration.ToMilliseconds()};
  }

#define TIMESTAMP_JSON_SUFFIX "_TSms"

  // Convert a TimeStamp to the value to be stored in the log.
  // Use TIMESTAMP_JSON_SUFFIX as suffix in the property name.
  static Json::Value Timestamp(
      const mozilla::TimeStamp& aTimestamp = mozilla::TimeStamp::Now()) {
    if (aTimestamp.IsNull()) {
      return Json::Value{0.0};
    }
    return Duration(aTimestamp - mozilla::TimeStamp::ProcessCreation());
  }

  static bool IsLockedOnCurrentThread();

 private:
  static mozilla::baseprofiler::detail::BaseProfilerMutex gMutex;
  static mozilla::UniquePtr<Json::Value> gLog;
};

// ----------------------------------------------------------------------------
// Miscellaneous

// If positive, skip stack-sampling in the sampler thread loop.
// Users should increment it atomically when samplings should be avoided, and
// later decrement it back. Multiple uses can overlap.
// There could be a sampling in progress when this is first incremented, so if
// it is critical to prevent any sampling, lock the profiler mutex instead.
// Relaxed ordering, because it's used to request that the profiler pause
// future sampling; this is not time critical, nor dependent on anything else.
extern mozilla::Atomic<int, mozilla::MemoryOrdering::Relaxed> gSkipSampling;

void AppendSharedLibraries(mozilla::JSONWriter& aWriter);

// Convert the array of strings to a bitfield.
uint32_t ParseFeaturesFromStringArray(const char** aFeatures,
                                      uint32_t aFeatureCount,
                                      bool aIsStartup = false);

// Add the begin/end 'Awake' markers for the thread.
void profiler_mark_thread_awake();

void profiler_mark_thread_asleep();

[[nodiscard]] bool profiler_get_profile_json(
    SpliceableChunkedJSONWriter& aSpliceableChunkedJSONWriter,
    double aSinceTime, bool aIsShuttingDown,
    mozilla::ProgressLogger aProgressLogger);

// Flags to conveniently track various JS instrumentations.
enum class JSInstrumentationFlags {
  StackSampling = 0x1,
  Allocations = 0x2,
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
  }                                                                   \
                                                                      \
  constexpr mozilla::Maybe<uint64_t> GetJson##name##unit() const {    \
    if (Is##name##unit##Known()) {                                    \
      return mozilla::Some(ConvertRawToJson(m##name##unit));          \
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

  // Platform-dependent.
  static uint64_t ConvertRawToJson(uint64_t aRawValue);

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
