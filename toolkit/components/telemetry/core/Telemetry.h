/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Telemetry_h__
#define Telemetry_h__

#include "mozilla/Maybe.h"
#include "mozilla/TelemetryEventEnums.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TelemetryOriginEnums.h"
#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"

/******************************************************************************
 * This implements the Telemetry system.
 * It allows recording into histograms as well some more specialized data
 * points and gives access to the data.
 *
 * For documentation on how to add and use new Telemetry probes, see:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/start/adding-a-new-probe.html
 *
 * For more general information on Telemetry see:
 * https://wiki.mozilla.org/Telemetry
 *****************************************************************************/

namespace mozilla {
namespace Telemetry {

struct HistogramAccumulation;
struct KeyedHistogramAccumulation;
struct ScalarAction;
struct KeyedScalarAction;
struct ChildEventData;

struct EventExtraEntry {
  nsCString key;
  nsCString value;
};

/**
 * Initialize the Telemetry service on the main thread at startup.
 */
void Init();

/**
 * Shutdown the Telemetry service.
 */
void ShutdownTelemetry();

/**
 * Adds sample to a histogram defined in TelemetryHistogramEnums.h
 *
 * @param id - histogram id
 * @param sample - value to record.
 */
void Accumulate(HistogramID id, uint32_t sample);

/**
 * Adds an array of samples to a histogram defined in TelemetryHistograms.h
 * @param id - histogram id
 * @param samples - values to record.
 */
void Accumulate(HistogramID id, const nsTArray<uint32_t>& samples);

/**
 * Adds sample to a keyed histogram defined in TelemetryHistogramEnums.h
 *
 * @param id - keyed histogram id
 * @param key - the string key
 * @param sample - (optional) value to record, defaults to 1.
 */
void Accumulate(HistogramID id, const nsCString& key, uint32_t sample = 1);

/**
 * Adds an array of samples to a histogram defined in TelemetryHistograms.h
 * @param id - histogram id
 * @param samples - values to record.
 * @param key - the string key
 */
void Accumulate(HistogramID id, const nsCString& key,
                const nsTArray<uint32_t>& samples);

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
void Accumulate(const char* name, const nsCString& key, uint32_t sample = 1);

/**
 * Adds sample to a categorical histogram defined in TelemetryHistogramEnums.h
 * This is the typesafe - and preferred - way to use the categorical histograms
 * by passing values from the corresponding Telemetry::LABELS_* enum.
 *
 * @param enumValue - Label value from one of the Telemetry::LABELS_* enums.
 */
template <class E>
void AccumulateCategorical(E enumValue) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  Accumulate(static_cast<HistogramID>(CategoricalLabelId<E>::value),
             static_cast<uint32_t>(enumValue));
};

/**
 * Adds an array of samples to categorical histograms defined in
 * TelemetryHistogramEnums.h This is the typesafe - and preferred - way to use
 * the categorical histograms by passing values from the corresponding
 * Telemetry::LABELS_* enums.
 *
 * @param enumValues - Array of labels from Telemetry::LABELS_* enums.
 */
template <class E>
void AccumulateCategorical(const nsTArray<E>& enumValues) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  nsTArray<uint32_t> intSamples(enumValues.Length());

  for (E aValue : enumValues) {
    intSamples.AppendElement(static_cast<uint32_t>(aValue));
  }

  HistogramID categoricalId =
      static_cast<HistogramID>(CategoricalLabelId<E>::value);

  Accumulate(categoricalId, intSamples);
}

/**
 * Adds sample to a keyed categorical histogram defined in
 * TelemetryHistogramEnums.h This is the typesafe - and preferred - way to use
 * the keyed categorical histograms by passing values from the corresponding
 * Telemetry::LABELS_* enum.
 *
 * @param key - the string key
 * @param enumValue - Label value from one of the Telemetry::LABELS_* enums.
 */
template <class E>
void AccumulateCategoricalKeyed(const nsCString& key, E enumValue) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  Accumulate(static_cast<HistogramID>(CategoricalLabelId<E>::value), key,
             static_cast<uint32_t>(enumValue));
};

/**
 * Adds an array of samples to a keyed categorical histogram defined in
 * TelemetryHistogramEnums.h. This is the typesafe - and preferred - way to use
 * the keyed categorical histograms by passing values from the corresponding
 * Telemetry::LABELS_*enum.
 *
 * @param key - the string key
 * @param enumValue - Label value from one of the Telemetry::LABELS_* enums.
 */
template <class E>
void AccumulateCategoricalKeyed(const nsCString& key,
                                const nsTArray<E>& enumValues) {
  static_assert(IsCategoricalLabelEnum<E>::value,
                "Only categorical label enum types are supported.");
  nsTArray<uint32_t> intSamples(enumValues.Length());

  for (E aValue : enumValues) {
    intSamples.AppendElement(static_cast<uint32_t>(aValue));
  }

  Accumulate(static_cast<HistogramID>(CategoricalLabelId<E>::value), key,
             intSamples);
};

/**
 * Adds sample to a categorical histogram defined in TelemetryHistogramEnums.h
 * This string will be matched against the labels defined in Histograms.json.
 * If the string does not match a label defined for the histogram, nothing will
 * be recorded.
 *
 * @param id - The histogram id.
 * @param label - A string label value that is defined in Histograms.json for
 *                this histogram.
 */
void AccumulateCategorical(HistogramID id, const nsCString& label);

/**
 * Adds an array of samples to a categorical histogram defined in
 * Histograms.json
 *
 * @param id - The histogram id
 * @param labels - The array of labels to accumulate
 */
void AccumulateCategorical(HistogramID id, const nsTArray<nsCString>& labels);

/**
 * Adds time delta in milliseconds to a histogram defined in
 * TelemetryHistogramEnums.h
 *
 * @param id - histogram id
 * @param start - start time
 * @param end - end time
 */
void AccumulateTimeDelta(HistogramID id, TimeStamp start,
                         TimeStamp end = TimeStamp::Now());

/**
 * Adds time delta in milliseconds to a keyed histogram defined in
 * TelemetryHistogramEnums.h
 *
 * @param id - histogram id
 * @param key - the string key
 * @param start - start time
 * @param end - end time
 */
void AccumulateTimeDelta(HistogramID id, const nsCString& key, TimeStamp start,
                         TimeStamp end = TimeStamp::Now());

/**
 * Enable/disable recording for this histogram in this process at runtime.
 * Recording is enabled by default, unless listed at
 * kRecordingInitiallyDisabledIDs[]. id must be a valid telemetry enum,
 *
 * @param id - histogram id
 * @param enabled - whether or not to enable recording from now on.
 */
void SetHistogramRecordingEnabled(HistogramID id, bool enabled);

const char* GetHistogramName(HistogramID id);

class MOZ_RAII RuntimeAutoTimer {
 public:
  explicit RuntimeAutoTimer(Telemetry::HistogramID aId,
                            TimeStamp aStart = TimeStamp::Now())
      : id(aId), start(aStart) {}
  explicit RuntimeAutoTimer(Telemetry::HistogramID aId, const nsCString& aKey,
                            TimeStamp aStart = TimeStamp::Now())
      : id(aId), key(aKey), start(aStart) {
    MOZ_ASSERT(!aKey.IsEmpty(), "The key must not be empty.");
  }

  ~RuntimeAutoTimer() {
    if (key.IsEmpty()) {
      AccumulateTimeDelta(id, start);
    } else {
      AccumulateTimeDelta(id, key, start);
    }
  }

 private:
  Telemetry::HistogramID id;
  const nsCString key;
  const TimeStamp start;
};

template <HistogramID id>
class MOZ_RAII AutoTimer {
 public:
  explicit AutoTimer(TimeStamp aStart = TimeStamp::Now()) : start(aStart) {}

  explicit AutoTimer(const nsCString& aKey, TimeStamp aStart = TimeStamp::Now())
      : start(aStart), key(aKey) {
    MOZ_ASSERT(!aKey.IsEmpty(), "The key must not be empty.");
  }

  ~AutoTimer() {
    if (key.IsEmpty()) {
      AccumulateTimeDelta(id, start);
    } else {
      AccumulateTimeDelta(id, key, start);
    }
  }

 private:
  const TimeStamp start;
  const nsCString key;
};

class MOZ_RAII RuntimeAutoCounter {
 public:
  explicit RuntimeAutoCounter(HistogramID aId, uint32_t counterStart = 0)
      : id(aId), counter(counterStart) {}

  ~RuntimeAutoCounter() { Accumulate(id, counter); }

  // Prefix increment only, to encourage good habits.
  void operator++() {
    if (NS_WARN_IF(counter == std::numeric_limits<uint32_t>::max())) {
      return;
    }
    ++counter;
  }

  // Chaining doesn't make any sense, don't return anything.
  void operator+=(int increment) {
    if (NS_WARN_IF(increment > 0 &&
                   static_cast<uint32_t>(increment) >
                       (std::numeric_limits<uint32_t>::max() - counter))) {
      counter = std::numeric_limits<uint32_t>::max();
      return;
    }
    if (NS_WARN_IF(increment < 0 &&
                   static_cast<uint32_t>(-increment) > counter)) {
      counter = std::numeric_limits<uint32_t>::min();
      return;
    }
    counter += increment;
  }

 private:
  HistogramID id;
  uint32_t counter;
};

template <HistogramID id>
class MOZ_RAII AutoCounter {
 public:
  explicit AutoCounter(uint32_t counterStart = 0) : counter(counterStart) {}

  ~AutoCounter() { Accumulate(id, counter); }

  // Prefix increment only, to encourage good habits.
  void operator++() {
    if (NS_WARN_IF(counter == std::numeric_limits<uint32_t>::max())) {
      return;
    }
    ++counter;
  }

  // Chaining doesn't make any sense, don't return anything.
  void operator+=(int increment) {
    if (NS_WARN_IF(increment > 0 &&
                   static_cast<uint32_t>(increment) >
                       (std::numeric_limits<uint32_t>::max() - counter))) {
      counter = std::numeric_limits<uint32_t>::max();
      return;
    }
    if (NS_WARN_IF(increment < 0 &&
                   static_cast<uint32_t>(-increment) > counter)) {
      counter = std::numeric_limits<uint32_t>::min();
      return;
    }
    counter += increment;
  }

 private:
  uint32_t counter;
};

/**
 * Indicates whether Telemetry base data recording is turned on. Added for
 * future uses.
 */
bool CanRecordBase();

/**
 * Indicates whether Telemetry extended data recording is turned on.  This is
 * intended to guard calls to Accumulate when the statistic being recorded is
 * expensive to compute.
 */
bool CanRecordExtended();

/**
 * Indicates whether Telemetry release data recording is turned on. Usually
 * true.
 *
 * @see nsITelemetry.canRecordReleaseData
 */
bool CanRecordReleaseData();

/**
 * Indicates whether Telemetry pre-release data recording is turned on. Tends
 * to be true on pre-release channels.
 *
 * @see nsITelemetry.canRecordPrereleaseData
 */
bool CanRecordPrereleaseData();

/**
 * Records slow SQL statements for Telemetry reporting.
 *
 * @param statement - offending SQL statement to record
 * @param dbName - DB filename
 * @param delay - execution time in milliseconds
 */
void RecordSlowSQLStatement(const nsACString& statement,
                            const nsACString& dbName, uint32_t delay);

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
void ScalarAdd(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
               uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
               uint32_t aValue);

/**
 * Sets the scalar to the given value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value to set the scalar to.
 */
void ScalarSet(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
               bool aValue);

/**
 * Sets the scalar to the maximum of the current and the passed value.
 *
 * @param aId The scalar enum id.
 * @param aKey The scalar key.
 * @param aValue The value the scalar is set to if its greater
 *        than the current value.
 */
void ScalarSetMaximum(mozilla::Telemetry::ScalarID aId, const nsAString& aKey,
                      uint32_t aValue);

template <ScalarID id>
class MOZ_RAII AutoScalarTimer {
 public:
  explicit AutoScalarTimer(TimeStamp aStart = TimeStamp::Now())
      : start(aStart) {}

  explicit AutoScalarTimer(const nsAString& aKey,
                           TimeStamp aStart = TimeStamp::Now())
      : start(aStart), key(aKey) {
    MOZ_ASSERT(!aKey.IsEmpty(), "The key must not be empty.");
  }

  ~AutoScalarTimer() {
    TimeStamp end = TimeStamp::Now();
    uint32_t delta = static_cast<uint32_t>((end - start).ToMilliseconds());
    if (key.IsEmpty()) {
      mozilla::Telemetry::ScalarSet(id, delta);
    } else {
      mozilla::Telemetry::ScalarSet(id, key, delta);
    }
  }

 private:
  const TimeStamp start;
  const nsString key;
};

/**
 * Records an event. See the Event documentation for more information:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/events.html
 *
 * @param aId The event enum id.
 * @param aValue Optional. The event value.
 * @param aExtra Optional. The event's extra key/value pairs.
 */
void RecordEvent(mozilla::Telemetry::EventID aId,
                 const mozilla::Maybe<nsCString>& aValue,
                 const mozilla::Maybe<CopyableTArray<EventExtraEntry>>& aExtra);

/**
 * Enables recording of events in a category.
 * Events default to recording disabled.
 * This toggles recording for all events in the specified category.
 *
 * @param aCategory The category name.
 * @param aEnabled Whether recording should be enabled or disabled.
 */
void SetEventRecordingEnabled(const nsACString& aCategory, bool aEnabled);

/**
 * YOU PROBABLY SHOULDN'T USE THIS.
 * THIS IS AN EXPERIMENTAL API NOT YET READY FOR GENERAL USE.
 *
 * Records that the metric is true for the stated origin.
 *
 * @param aId the metric.
 * @param aOrigin the origin on which to record the metric as true.
 */
void RecordOrigin(mozilla::Telemetry::OriginMetricID aId,
                  const nsACString& aOrigin);

}  // namespace Telemetry
}  // namespace mozilla

#endif  // Telemetry_h__
