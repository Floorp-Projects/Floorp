/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/TimingDistribution.h"

#include "Common.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

#ifdef MOZ_GLEAN_ANDROID
// No Glean around to generate these for us.
static Atomic<uint64_t> gNextTimerId(1);
#endif

TimerId TimingDistributionMetric::Start() const {
#ifdef MOZ_GLEAN_ANDROID
  TimerId id = gNextTimerId++;
#else
  TimerId id = fog_timing_distribution_start(mId);
#endif
  auto mirrorId = HistogramIdForMetric(mId);
  if (mirrorId) {
    auto lock = GetTimerIdToStartsLock();
    (void)NS_WARN_IF(lock.ref()->Remove(id));
    lock.ref()->InsertOrUpdate(id, TimeStamp::Now());
  }
  return id;
}

void TimingDistributionMetric::StopAndAccumulate(const TimerId&& aId) const {
  auto mirrorId = HistogramIdForMetric(mId);
  if (mirrorId) {
    auto lock = GetTimerIdToStartsLock();
    auto optStart = lock.ref()->Extract(aId);
    if (!NS_WARN_IF(!optStart)) {
      AccumulateTimeDelta(mirrorId.extract(), optStart.extract());
    }
  }
#ifndef MOZ_GLEAN_ANDROID
  fog_timing_distribution_stop_and_accumulate(mId, aId);
#endif
}

void TimingDistributionMetric::Cancel(const TimerId&& aId) const {
  auto mirrorId = HistogramIdForMetric(mId);
  if (mirrorId) {
    auto lock = GetTimerIdToStartsLock();
    (void)NS_WARN_IF(!lock.ref()->Remove(aId));
  }
#ifndef MOZ_GLEAN_ANDROID
  fog_timing_distribution_cancel(mId, aId);
#endif
}

Result<Maybe<DistributionData>, nsCString>
TimingDistributionMetric::TestGetValue(const nsACString& aPingName) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
  return Maybe<DistributionData>();
#else
  nsCString err;
  if (fog_timing_distribution_test_get_error(mId, &aPingName, &err)) {
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
#endif
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanTimingDistribution, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimingDistribution, nsIGleanTimingDistribution)

NS_IMETHODIMP
GleanTimingDistribution::Start(JSContext* aCx, JS::MutableHandleValue aResult) {
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
                                      JS::MutableHandleValue aResult) {
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
    JS::RootedObject root(aCx, JS_NewPlainObject(aCx));
    if (!root) {
      return NS_ERROR_FAILURE;
    }
    uint64_t sum = optresult.ref().sum;
    if (!JS_DefineProperty(aCx, root, "sum", static_cast<double>(sum),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedObject valuesObj(aCx, JS_NewPlainObject(aCx));
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

}  // namespace mozilla::glean
