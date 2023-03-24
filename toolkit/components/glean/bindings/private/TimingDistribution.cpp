/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/TimingDistribution.h"

#include "Common.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/Tuple.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

// Called from within FOG's Rust impl.
extern "C" NS_EXPORT void GIFFT_TimingDistributionStart(
    uint32_t aMetricId, mozilla::glean::TimerId aTimerId) {
  auto mirrorId = mozilla::glean::HistogramIdForMetric(aMetricId);
  if (mirrorId) {
    mozilla::glean::GetTimerIdToStartsLock().apply([&](auto& lock) {
      auto tuple = mozilla::MakeTuple(aMetricId, aTimerId);
      // It should be all but impossible for anyone to have already inserted
      // this timer for this metric given the monotonicity of timer ids.
      (void)NS_WARN_IF(lock.ref()->Remove(tuple));
      lock.ref()->InsertOrUpdate(tuple, mozilla::TimeStamp::Now());
    });
  }
}

// Called from within FOG's Rust impl.
extern "C" NS_EXPORT void GIFFT_TimingDistributionStopAndAccumulate(
    uint32_t aMetricId, mozilla::glean::TimerId aTimerId) {
  auto mirrorId = mozilla::glean::HistogramIdForMetric(aMetricId);
  if (mirrorId) {
    mozilla::glean::GetTimerIdToStartsLock().apply([&](auto& lock) {
      auto optStart =
          lock.ref()->Extract(mozilla::MakeTuple(aMetricId, aTimerId));
      // The timer might not be in the map to be removed if it's already been
      // cancelled or stop_and_accumulate'd.
      if (!NS_WARN_IF(!optStart)) {
        AccumulateTimeDelta(mirrorId.extract(), optStart.extract());
      }
    });
  }
}

// Called from within FOG's Rust impl.
extern "C" NS_EXPORT void GIFFT_TimingDistributionAccumulateRawMillis(
    uint32_t aMetricId, uint32_t aMS) {
  auto mirrorId = mozilla::glean::HistogramIdForMetric(aMetricId);
  if (mirrorId) {
    Accumulate(mirrorId.extract(), aMS);
  }
}

// Called from within FOG's Rust impl.
extern "C" NS_EXPORT void GIFFT_TimingDistributionCancel(
    uint32_t aMetricId, mozilla::glean::TimerId aTimerId) {
  auto mirrorId = mozilla::glean::HistogramIdForMetric(aMetricId);
  if (mirrorId) {
    mozilla::glean::GetTimerIdToStartsLock().apply([&](auto& lock) {
      // The timer might not be in the map to be removed if it's already been
      // cancelled or stop_and_accumulate'd.
      (void)NS_WARN_IF(
          !lock.ref()->Remove(mozilla::MakeTuple(aMetricId, aTimerId)));
    });
  }
}

namespace mozilla::glean {

namespace impl {

TimerId TimingDistributionMetric::Start() const {
  return fog_timing_distribution_start(mId);
}

void TimingDistributionMetric::StopAndAccumulate(const TimerId&& aId) const {
  fog_timing_distribution_stop_and_accumulate(mId, aId);
}

// Intentionally not exposed to JS for lack of use case and a time duration
// type.
void TimingDistributionMetric::AccumulateRawDuration(
    const TimeDuration& aDuration) const {
  fog_timing_distribution_accumulate_raw_nanos(
      mId, uint64_t(aDuration.ToMicroseconds() * 1000.00));
}

void TimingDistributionMetric::Cancel(const TimerId&& aId) const {
  fog_timing_distribution_cancel(mId, aId);
}

Result<Maybe<DistributionData>, nsCString>
TimingDistributionMetric::TestGetValue(const nsACString& aPingName) const {
  nsCString err;
  if (fog_timing_distribution_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_timing_distribution_test_has_value(mId, &aPingName)) {
    return Maybe<DistributionData>();
  }
  nsTArray<uint64_t> buckets;
  nsTArray<uint64_t> counts;
  uint64_t sum;
  fog_timing_distribution_test_get_value(mId, &aPingName, &sum, &buckets,
                                         &counts);
  return Some(DistributionData(buckets, counts, sum));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanTimingDistribution, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimingDistribution, nsIGleanTimingDistribution)

NS_IMETHODIMP
GleanTimingDistribution::Start(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aResult) {
  if (!dom::ToJSValue(aCx, mTimingDist.Start(), aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::StopAndAccumulate(uint64_t aId) {
  mTimingDist.StopAndAccumulate(std::move(aId));
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::Cancel(uint64_t aId) {
  mTimingDist.Cancel(std::move(aId));
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::TestGetValue(const nsACString& aPingName,
                                      JSContext* aCx,
                                      JS::MutableHandle<JS::Value> aResult) {
  auto result = mTimingDist.TestGetValue(aPingName);
  if (result.isErr()) {
    aResult.set(JS::UndefinedValue());
    LogToBrowserConsole(nsIScriptError::errorFlag,
                        NS_ConvertUTF8toUTF16(result.unwrapErr()));
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }
  auto optresult = result.unwrap();
  if (optresult.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    // Build return value of the form: { sum: #, values: {bucket1: count1,
    // ...}
    JS::Rooted<JSObject*> root(aCx, JS_NewPlainObject(aCx));
    if (!root) {
      return NS_ERROR_FAILURE;
    }
    uint64_t sum = optresult.ref().sum;
    if (!JS_DefineProperty(aCx, root, "sum", static_cast<double>(sum),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    JS::Rooted<JSObject*> valuesObj(aCx, JS_NewPlainObject(aCx));
    if (!valuesObj ||
        !JS_DefineProperty(aCx, root, "values", valuesObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    auto& data = optresult.ref().values;
    for (const auto& entry : data) {
      const uint64_t bucket = entry.GetKey();
      const uint64_t count = entry.GetData();
      if (!JS_DefineProperty(aCx, valuesObj,
                             nsPrintfCString("%" PRIu64, bucket).get(),
                             static_cast<double>(count), JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
    aResult.setObject(*root);
  }
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::TestAccumulateRawMillis(uint64_t aSample) {
  mTimingDist.AccumulateRawDuration(TimeDuration::FromMilliseconds(aSample));
  return NS_OK;
}

}  // namespace mozilla::glean
