/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/MemoryDistribution.h"

#include "mozilla/Components.h"
#include "mozilla/Maybe.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty

namespace mozilla::glean {

namespace impl {

void MemoryDistributionMetric::Accumulate(size_t aSample) const {
  auto hgramId = HistogramIdForMetric(mId);
  if (hgramId) {
    Telemetry::Accumulate(hgramId.extract(), aSample);
  }
  static_assert(sizeof(size_t) <= sizeof(uint64_t),
                "Memory distribution samples might overflow.");
  fog_memory_distribution_accumulate(mId, aSample);
}

Result<Maybe<DistributionData>, nsCString>
MemoryDistributionMetric::TestGetValue(const nsACString& aPingName) const {
  nsCString err;
  if (fog_memory_distribution_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_memory_distribution_test_has_value(mId, &aPingName)) {
    return Maybe<DistributionData>();
  }
  nsTArray<uint64_t> buckets;
  nsTArray<uint64_t> counts;
  uint64_t sum;
  fog_memory_distribution_test_get_value(mId, &aPingName, &sum, &buckets,
                                         &counts);
  return Some(DistributionData(buckets, counts, sum));
}

}  // namespace impl

/* virtual */
JSObject* GleanMemoryDistribution::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanMemoryDistribution_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanMemoryDistribution::Accumulate(uint64_t aSample) {
  mMemoryDist.Accumulate(aSample);
}

void GleanMemoryDistribution::TestGetValue(
    const nsACString& aPingName,
    dom::Nullable<dom::GleanDistributionData>& aRetval, ErrorResult& aRv) {
  auto result = mMemoryDist.TestGetValue(aPingName);
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
