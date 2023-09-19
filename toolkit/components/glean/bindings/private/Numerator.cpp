/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Numerator.h"

#include "nsString.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "jsapi.h"

namespace mozilla::glean {

namespace impl {

void NumeratorMetric::AddToNumerator(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aAmount >= 0) {
    Telemetry::ScalarAdd(scalarId.extract(), u"numerator"_ns, aAmount);
  }
  fog_numerator_add_to_numerator(mId, aAmount);
}

Result<Maybe<std::pair<int32_t, int32_t>>, nsCString>
NumeratorMetric::TestGetValue(const nsACString& aPingName) const {
  nsCString err;
  if (fog_numerator_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_numerator_test_has_value(mId, &aPingName)) {
    return Maybe<std::pair<int32_t, int32_t>>();
  }
  int32_t num = 0;
  int32_t den = 0;
  fog_numerator_test_get_value(mId, &aPingName, &num, &den);
  return Some(std::make_pair(num, den));
}

}  // namespace impl

/* virtual */
JSObject* GleanNumerator::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanNumerator_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanNumerator::AddToNumerator(int32_t aAmount) {
  mNumerator.AddToNumerator(aAmount);
}

void GleanNumerator::TestGetValue(const nsACString& aPingName,
                                  dom::Nullable<dom::GleanRateData>& aResult,
                                  ErrorResult& aRv) {
  auto result = mNumerator.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return;
  }
  auto optresult = result.unwrap();
  if (!optresult.isNothing()) {
    dom::GleanRateData ret;
    auto pair = optresult.extract();
    ret.mNumerator = pair.first;
    ret.mDenominator = pair.second;
    aResult.SetValue(std::move(ret));
  }
}

}  // namespace mozilla::glean
