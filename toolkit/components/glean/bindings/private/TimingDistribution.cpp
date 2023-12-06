/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/TimingDistribution.h"

#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
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
      auto tuple = std::make_tuple(aMetricId, aTimerId);
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
      auto optStart = lock.ref()->Extract(std::make_tuple(aMetricId, aTimerId));
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
          !lock.ref()->Remove(std::make_tuple(aMetricId, aTimerId)));
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

/* virtual */
JSObject* GleanTimingDistribution::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanTimingDistribution_Binding::Wrap(aCx, this, aGivenProto);
}

uint64_t GleanTimingDistribution::Start() { return mTimingDist.Start(); }

void GleanTimingDistribution::StopAndAccumulate(uint64_t aId) {
  mTimingDist.StopAndAccumulate(std::move(aId));
}

void GleanTimingDistribution::Cancel(uint64_t aId) {
  mTimingDist.Cancel(std::move(aId));
}

void GleanTimingDistribution::TestGetValue(
    const nsACString& aPingName,
    dom::Nullable<dom::GleanDistributionData>& aRetval, ErrorResult& aRv) {
  auto result = mTimingDist.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return;
  }
  auto optresult = result.unwrap();
  if (optresult.isNothing()) {
    return;
  }

  dom::GleanDistributionData ret;
  ret.mSum = optresult.ref().sum;
  auto& data = optresult.ref().values;
  for (const auto& entry : data) {
    dom::binding_detail::RecordEntry<nsCString, uint64_t> bucket;
    bucket.mKey = nsPrintfCString("%" PRIu64, entry.GetKey());
    bucket.mValue = entry.GetData();
    ret.mValues.Entries().EmplaceBack(std::move(bucket));
  }
  aRetval.SetValue(std::move(ret));
}

void GleanTimingDistribution::TestAccumulateRawMillis(uint64_t aSample) {
  mTimingDist.AccumulateRawDuration(TimeDuration::FromMilliseconds(aSample));
}

}  // namespace mozilla::glean
