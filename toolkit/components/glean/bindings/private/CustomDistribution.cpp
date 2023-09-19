/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/CustomDistribution.h"

#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::glean {

namespace impl {

void CustomDistributionMetric::AccumulateSamples(
    const nsTArray<uint64_t>& aSamples) const {
  auto hgramId = HistogramIdForMetric(mId);
  if (hgramId) {
    auto id = hgramId.extract();
    // N.B.: There is an `Accumulate(nsTArray<T>)`, but `T` is `uint32_t` and
    // we got `uint64_t`s here.
    for (auto sample : aSamples) {
      Telemetry::Accumulate(id, sample);
    }
  }
  fog_custom_distribution_accumulate_samples(mId, &aSamples);
}

void CustomDistributionMetric::AccumulateSamplesSigned(
    const nsTArray<int64_t>& aSamples) const {
  auto hgramId = HistogramIdForMetric(mId);
  if (hgramId) {
    auto id = hgramId.extract();
    // N.B.: There is an `Accumulate(nsTArray<T>)`, but `T` is `uint32_t` and
    // we got `int64_t`s here.
    for (auto sample : aSamples) {
      Telemetry::Accumulate(id, sample);
    }
  }
  fog_custom_distribution_accumulate_samples_signed(mId, &aSamples);
}

Result<Maybe<DistributionData>, nsCString>
CustomDistributionMetric::TestGetValue(const nsACString& aPingName) const {
  nsCString err;
  if (fog_custom_distribution_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_custom_distribution_test_has_value(mId, &aPingName)) {
    return Maybe<DistributionData>();
  }
  nsTArray<uint64_t> buckets;
  nsTArray<uint64_t> counts;
  uint64_t sum;
  fog_custom_distribution_test_get_value(mId, &aPingName, &sum, &buckets,
                                         &counts);
  return Some(DistributionData(buckets, counts, sum));
}

}  // namespace impl

/* virtual */
JSObject* GleanCustomDistribution::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanCustomDistribution_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanCustomDistribution::AccumulateSamples(
    const dom::Sequence<int64_t>& aSamples) {
  mCustomDist.AccumulateSamplesSigned(aSamples);
}

void GleanCustomDistribution::TestGetValue(
    const nsACString& aPingName,
    dom::Nullable<dom::GleanDistributionData>& aRetval, ErrorResult& aRv) {
  auto result = mCustomDist.TestGetValue(aPingName);
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

}  // namespace mozilla::glean
