/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Telemetry_h__
#define Telemetry_h__

#include "mozilla/GuardObjects.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StartupTimeline.h"
#include "nsTArray.h"
#include "nsStringGlue.h"
#include "nsXULAppAPI.h"

#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TelemetryScalarEnums.h"

/******************************************************************************
 * This implements the Telemetry system.
 * It allows recording into histograms as well some more specialized data
 * points and gives access to the data.
 *
 * For documentation on how to add and use new Telemetry probes, see:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Adding_a_new_Telemetry_probe
 *
 * For more general information on Telemetry see:
 * https://wiki.mozilla.org/Telemetry
 *****************************************************************************/

namespace mozilla {
namespace HangMonitor {
  class HangAnnotations;
} // namespace HangMonitor
namespace Telemetry {

struct Accumulation;
struct KeyedAccumulation;
struct ScalarAction;
struct KeyedScalarAction;
struct ChildEventData;

enum TimerResolution {
  Millisecond,
  Microsecond
};

/**
 * Create and destroy the underlying base::StatisticsRecorder singleton.
 * Creation has to be done very early in the startup sequence.
 */
void CreateStatisticsRecorder();
void DestroyStatisticsRecorder();

/**
 * Initialize the Telemetry service on the main thread at startup.
 */
void Init();

/**
 * Adds sample to a histogram defined in TelemetryHistogramEnums.h
 *
 * @param id - histogram id
 * @param sample - value to record.
 */
void Accumulate(HistogramID id, uint32_t sample);

/**
 * Adds sample to a keyed histogram defined in TelemetryHistogramEnums.h
 *
 * @param id - keyed histogram id
 * @param key - the string key
 * @param sample - (optional) value to record, defaults to 1.
 */
void Accumulate(HistogramID id, const nsCString& key, uint32_t sample = 1);

/**
 * Adds a sample to a histogram defined in TelemetryHistogramEnums.h.
 * This function is here to support telemetry measurements from Java,
 * where we have only names and not numeric IDs.  You should almost
 * certainly be using the by-enum-id version instead of this one.
 *
 * @param name - histogram name
 * @param sample - value to record
 */
void Accumulate(const char* name, uint32_t sample);

/**
 * Adds a sample to a histogram defined in TelemetryHistogramEnums.h.
 * This function is here to support telemetry measurements from Java,
 * where we have only names and not numeric IDs.  You should almost
 * certainly be using the by-enum-id version instead of this one.
 *
 * @param name - histogram name
 * @param key - the string key
 * @param sample - sample - (optional) value to record, defaults to 1.
 */
void Accumulate(const char *name, const nsCString& key, uint32_t sample = 1);

/**
 * Adds sample to a categorical histogram defined in TelemetryHistogramEnums.h
 * This is the typesafe - and preferred - way to use the categorical histograms
 * by passing values from the corresponding Telemetry::LABELS_* enum.
 *
 * @param enumValue - Label value from one of the Telemetry::LABELS_* enums.
 */
template<class E>
void AccumulateCategorical(E enumValue) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  Accumulate(static_cast<HistogramID>(CategoricalLabelId<E>::value),
             static_cast<uint32_t>(enumValue));
};

/**
 * Adds sample to a keyed categorical histogram defined in TelemetryHistogramEnums.h
 * This is the typesafe - and preferred - way to use the keyed categorical histograms
 * by passing values from the corresponding Telemetry::LABELS_* enum.
 *
 * @param key - the string key
 * @param enumValue - Label value from one of the Telemetry::LABELS_* enums.
 */
template<class E>
void AccumulateCategoricalKeyed(const nsCString& key, E enumValue) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  Accumulate(static_cast<HistogramID>(CategoricalLabelId<E>::value),
             key,
             static_cast<uint32_t>(enumValue));
};

/**
 * Adds sample to a categorical histogram defined in TelemetryHistogramEnums.h
 * This string will be matched against the labels defined in Histograms.json.
 * If the string does not match a label defined for the histogram, nothing will
 * be recorded.
 *
 * @param id - The histogram id.
 * @param label - A string label value that is defined in Histograms.json for this histogram.
 */
void AccumulateCategorical(HistogramID id, const nsCString& label);

/**
 * Adds time delta in milliseconds to a histogram defined in TelemetryHistogramEnums.h
 *
 * @param id - histogram id
 * @param start - start time
 * @param end - end time
 */
void AccumulateTimeDelta(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now());

/**
 * Enable/disable recording for this histogram at runtime.
 * Recording is enabled by default, unless listed at kRecordingInitiallyDisabledIDs[].
 * id must be a valid telemetry enum, otherwise an assertion is triggered.
 *
 * @param id - histogram id
 * @param enabled - whether or not to enable recording from now on.
 */
void SetHistogramRecordingEnabled(HistogramID id, bool enabled);

const char* GetHistogramName(HistogramID id);

/**
 * Those wrappers are needed because the VS versions we use do not support free
 * functions with default template arguments.
 */
template<TimerResolution res>
struct AccumulateDelta_impl
{
  static void compute(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now());
  static void compute(HistogramID id, const nsCString& key, TimeStamp start, TimeStamp end = TimeStamp::Now());
};

template<>
struct AccumulateDelta_impl<Millisecond>
{
  static void compute(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, static_cast<uint32_t>((end - start).ToMilliseconds()));
  }
  static void compute(HistogramID id, const nsCString& key, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, key, static_cast<uint32_t>((end - start).ToMilliseconds()));
  }
};

template<>
struct AccumulateDelta_impl<Microsecond>
{
  static void compute(HistogramID id, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, static_cast<uint32_t>((end - start).ToMicroseconds()));
  }
  static void compute(HistogramID id, const nsCString& key, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, key, static_cast<uint32_t>((end - start).ToMicroseconds()));
  }
};


template<HistogramID id, TimerResolution res = Millisecond>
class MOZ_RAII AutoTimer {
public:
  explicit AutoTimer(TimeStamp aStart = TimeStamp::Now() MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
     : start(aStart)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  explicit AutoTimer(const nsCString& aKey, TimeStamp aStart = TimeStamp::Now() MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : start(aStart)
    , key(aKey)
  {
    MOZ_ASSERT(!aKey.IsEmpty(), "The key must not be empty.");
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoTimer() {
    if (key.IsEmpty()) {
      AccumulateDelta_impl<res>::compute(id, start);
    } else {
      AccumulateDelta_impl<res>::compute(id, key, start);
    }
  }

private:
  const TimeStamp start;
  const nsCString key;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

template<HistogramID id>
class MOZ_RAII AutoCounter {
public:
  explicit AutoCounter(uint32_t counterStart = 0 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : counter(counterStart)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoCounter() {
    Accumulate(id, counter);
  }

  // Prefix increment only, to encourage good habits.
  void operator++() {
    ++counter;
  }

  // Chaining doesn't make any sense, don't return anything.
  void operator+=(int increment) {
    counter += increment;
  }

private:
  uint32_t counter;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * Indicates whether Telemetry base data recording is turned on. Added for future uses.
 */
bool CanRecordBase();

/**
 * Indicates whether Telemetry extended data recording is turned on.  This is intended
 * to guard calls to Accumulate when the statistic being recorded is expensive to compute.
 */
bool CanRecordExtended();

/**
 * Records slow SQL statements for Telemetry reporting.
 *
 * @param statement - offending SQL statement to record
 * @param dbName - DB filename
 * @param delay - execution time in milliseconds
 */
void RecordSlowSQLStatement(const nsACString &statement,
                            const nsACString &dbName,
                            uint32_t delay);

/**
 * Record Webrtc ICE candidate type combinations in a 17bit bitmask
 *
 * @param iceCandidateBitmask - the bitmask representing local and remote ICE
 *                              candidate types present for the connection
 * @param success - did the peer connection connected
 */
void
RecordWebrtcIceCandidates(const uint32_t iceCandidateBitmask,
                          const bool success);
/**
 * Initialize I/O Reporting
 * Initially this only records I/O for files in the binary directory.
 *
 * @param aXreDir - XRE directory
 */
void InitIOReporting(nsIFile* aXreDir);

/**
 * Set the profile directory. Once called, files in the profile directory will
 * be included in I/O reporting. We can't use the directory
 * service to obtain this information because it isn't running yet.
 */
void SetProfileDir(nsIFile* aProfD);

/**
 * Called to inform Telemetry that startup has completed.
 */
void LeavingStartupStage();

/**
 * Called to inform Telemetry that shutdown is commencing.
 */
void EnteringShutdownStage();

/**
 * Thresholds for a statement to be considered slow, in milliseconds
 */
const uint32_t kSlowSQLThresholdForMainThread = 50;
const uint32_t kSlowSQLThresholdForHelperThreads = 100;

class ProcessedStack;

/**
 * Record the main thread's call stack after it hangs.
 *
 * @param aDuration - Approximate duration of main thread hang, in seconds
 * @param aStack - Array of PCs from the hung call stack
 * @param aSystemUptime - System uptime at the time of the hang, in minutes
 * @param aFirefoxUptime - Firefox uptime at the time of the hang, in minutes
 * @param aAnnotations - Any annotations to be added to the report
 */
#if defined(MOZ_GECKO_PROFILER)
void RecordChromeHang(uint32_t aDuration,
                      ProcessedStack &aStack,
                      int32_t aSystemUptime,
                      int32_t aFirefoxUptime,
                      mozilla::UniquePtr<mozilla::HangMonitor::HangAnnotations>
                              aAnnotations);

/**
 * Record the current thread's call stack on demand. Note that, the stack is
 * only captured once. Subsequent calls result in incrementing the capture
 * counter.
 *
 * @param aKey - A user defined key associated with the captured stack.
 *
 * NOTE: Unwinding call stacks is an expensive operation performance-wise.
 */
void CaptureStack(const nsCString& aKey);
#endif

class ThreadHangStats;

/**
 * Move a ThreadHangStats to Telemetry storage. Normally Telemetry queries
 * for active ThreadHangStats through BackgroundHangMonitor, but once a
 * thread exits, the thread's copy of ThreadHangStats needs to be moved to
 * inside Telemetry using this function.
 *
 * @param aStats ThreadHangStats to save; the data inside aStats
 *               will be moved and aStats should be treated as
 *               invalid after this function returns
 */
void RecordThreadHangStats(ThreadHangStats&& aStats);

/**
 * Record a failed attempt at locking the user's profile.
 *
 * @param aProfileDir The profile directory whose lock attempt failed
 */
void WriteFailedProfileLock(nsIFile* aProfileDir);

/**
 * Adds the value to the given scalar.
 *
 * @param aId The scalar enum id.
 * @param aValue The value to add to the scalar.
 */
void ScalarAdd(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, bool aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aValue The value to set the scalar to, truncated to
 *        50 characters if exceeding that length.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aValue);

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aId The scalar enum id.
 * @param aValue The value the scalar is set to if its greater
 *        than the current value.
 */
void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, uint32_t aValue);

/**
 * Adds the value to the given scalar.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value to add to the scalar.
 */
void ScalarAdd(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, bool aValue);

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value the scalar is set to if its greater
 *        than the current value.
 */
void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey, uint32_t aValue);

} // namespace Telemetry
} // namespace mozilla

#endif // Telemetry_h__
