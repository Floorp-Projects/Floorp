/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Rate.h"

#include "jsapi.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/Common.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

namespace impl {

void RateMetric::AddToNumerator(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aAmount >= 0) {
    Telemetry::ScalarAdd(scalarId.extract(), u"numerator"_ns, aAmount);
  }
  fog_rate_add_to_numerator(mId, aAmount);
}

void RateMetric::AddToDenominator(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aAmount >= 0) {
    Telemetry::ScalarAdd(scalarId.extract(), u"denominator"_ns, aAmount);
  }
  fog_rate_add_to_denominator(mId, aAmount);
}

Result<Maybe<std::pair<int32_t, int32_t>>, nsCString> RateMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_rate_test_get_error(mId, &aPingName, &err)) {
    return Err(err);
  }
  if (!fog_rate_test_has_value(mId, &aPingName)) {
    return Maybe<std::pair<int32_t, int32_t>>();
  }
  int32_t num = 0;
  int32_t den = 0;
  fog_rate_test_get_value(mId, &aPingName, &num, &den);
  return Some(std::make_pair(num, den));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanRate, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanRate, nsIGleanRate)

NS_IMETHODIMP
GleanRate::AddToNumerator(int32_t aAmount) {
  mRate.AddToNumerator(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanRate::AddToDenominator(int32_t aAmount) {
  mRate.AddToDenominator(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanRate::TestGetValue(const nsACString& aPingName, JSContext* aCx,
                        JS::MutableHandleValue aResult) {
  auto result = mRate.TestGetValue(aPingName);
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
    // Build return value of the form: { numerator: n, denominator: d }
    JS::RootedObject root(aCx, JS_NewPlainObject(aCx));
    if (!root) {
      return NS_ERROR_FAILURE;
    }
    auto pair = optresult.extract();
    int32_t num = pair.first;
    int32_t den = pair.second;
    if (!JS_DefineProperty(aCx, root, "numerator", static_cast<double>(num),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, root, "denominator", static_cast<double>(den),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    aResult.setObject(*root);
  }
  return NS_OK;
}

}  // namespace mozilla::glean
