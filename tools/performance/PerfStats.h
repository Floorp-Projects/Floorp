/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PerfStats_h
#define PerfStats_h

#include "mozilla/TimeStamp.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/MozPromise.h"
#include <memory>
#include <string>
#include <limits>

#define FOR_EACH_PERFSTATS_METRIC(MACRO) \
  MACRO(DisplayListBuilding)             \
  MACRO(Rasterizing)                     \
  MACRO(LayerBuilding)                   \
  MACRO(LayerTransactions)               \
  MACRO(Compositing)                     \
  MACRO(Reflowing)                       \
  MACRO(Styling)                         \
  MACRO(HttpChannelCompletion)           \
  MACRO(HttpChannelCompletion_Network)   \
  MACRO(HttpChannelCompletion_Cache)     \
  MACRO(JSBC_Compression)                \
  MACRO(JSBC_Decompression)              \
  MACRO(JSBC_IO_Read)                    \
  MACRO(JSBC_IO_Write)

namespace mozilla {

class PerfStats {
 public:
  typedef MozPromise<nsCString, bool, true> PerfStatsPromise;

  enum class Metric : uint32_t {
#define DECLARE_ENUM(metric) metric,
    FOR_EACH_PERFSTATS_METRIC(DECLARE_ENUM)
#undef DECLARE_ENUM
        Max
  };

  // MetricMask is a bitmask based on 'Metric', i.e. Metric::LayerBuilding (2)
  // is synonymous to 1 << 2 in MetricMask.
  using MetricMask = uint64_t;

  static void RecordMeasurementStart(Metric aMetric) {
    if (!(sCollectionMask & (1 << static_cast<uint64_t>(aMetric)))) {
      return;
    }
    RecordMeasurementStartInternal(aMetric);
  }

  static void RecordMeasurementEnd(Metric aMetric) {
    if (!(sCollectionMask & (1 << static_cast<uint64_t>(aMetric)))) {
      return;
    }
    RecordMeasurementEndInternal(aMetric);
  }

  static void RecordMeasurement(Metric aMetric, TimeDuration aDuration) {
    if (!(sCollectionMask & (1 << static_cast<uint64_t>(aMetric)))) {
      return;
    }
    RecordMeasurementInternal(aMetric, aDuration);
  }

  static void RecordMeasurementCounter(Metric aMetric,
                                       uint64_t aIncrementAmount) {
    if (!(sCollectionMask & (1 << static_cast<uint64_t>(aMetric)))) {
      return;
    }
    RecordMeasurementCounterInternal(aMetric, aIncrementAmount);
  }

  template <Metric N>
  class AutoMetricRecording {
   public:
    AutoMetricRecording() { PerfStats::RecordMeasurementStart(N); }
    ~AutoMetricRecording() { PerfStats::RecordMeasurementEnd(N); }
  };

  static void SetCollectionMask(MetricMask aMask);

  static RefPtr<PerfStatsPromise> CollectPerfStatsJSON() {
    return GetSingleton()->CollectPerfStatsJSONInternal();
  }

  static nsCString CollectLocalPerfStatsJSON() {
    return GetSingleton()->CollectLocalPerfStatsJSONInternal();
  }

 private:
  static PerfStats* GetSingleton();
  static void RecordMeasurementStartInternal(Metric aMetric);
  static void RecordMeasurementEndInternal(Metric aMetric);
  static void RecordMeasurementInternal(Metric aMetric, TimeDuration aDuration);
  static void RecordMeasurementCounterInternal(Metric aMetric,
                                               uint64_t aIncrementAmount);

  RefPtr<PerfStatsPromise> CollectPerfStatsJSONInternal();
  nsCString CollectLocalPerfStatsJSONInternal();

  static MetricMask sCollectionMask;
  static StaticMutex sMutex MOZ_UNANNOTATED;
  static StaticAutoPtr<PerfStats> sSingleton;
  TimeStamp mRecordedStarts[static_cast<size_t>(Metric::Max)];
  double mRecordedTimes[static_cast<size_t>(Metric::Max)];
};

static_assert(1 << (static_cast<uint64_t>(PerfStats::Metric::Max) - 1) <=
                  std::numeric_limits<PerfStats::MetricMask>::max(),
              "More metrics than can fit into sCollectionMask bitmask");

}  // namespace mozilla

#endif  // PerfStats_h
