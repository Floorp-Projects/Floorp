/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/CustomDistribution.h"

#include "Common.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/HistogramGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
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

NS_IMPL_CLASSINFO(GleanCustomDistribution, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanCustomDistribution, nsIGleanCustomDistribution)

NS_IMETHODIMP
GleanCustomDistribution::AccumulateSamples(const nsTArray<int64_t>& aSamples) {
  mCustomDist.AccumulateSamplesSigned(aSamples);
  return NS_OK;
}

NS_IMETHODIMP
GleanCustomDistribution::TestGetValue(const nsACString& aPingName,
                                      JSContext* aCx,
                                      JS::MutableHandle<JS::Value> aResult) {
  auto result = mCustomDist.TestGetValue(aPingName);
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
    // Build return value of the form: { sum: #, values: {bucket1: count1, ...}
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

}  // namespace mozilla::glean
