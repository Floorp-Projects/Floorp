/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/String.h"

#include "jsapi.h"
#include "js/String.h"
#include "nsString.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

void StringMetric::Set(const nsACString& aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue));
  }
  fog_string_set(mId, &aValue);
}

Result<Maybe<nsCString>, nsCString> StringMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_string_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_string_test_has_value(mId, &aPingName)) {
    return Maybe<nsCString>();
  }
  nsCString ret;
  fog_string_test_get_value(mId, &aPingName, &ret);
  return Some(ret);
}

}  // namespace impl

/* virtual */
JSObject* GleanString::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanString_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanString::Set(const nsACString& aValue) {
  if (aValue.IsVoid()) {
    // TODO: Instrument this error (bug 1691073)
    return;
  }
  mString.Set(aValue);
}

void GleanString::TestGetValue(const nsACString& aPingName, nsCString& aResult,
                               ErrorResult& aRv) {
  auto result = mString.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return;
  }
  auto optresult = result.unwrap();
  if (!optresult.isNothing()) {
    aResult.Assign(optresult.extract());
  } else {
    aResult.SetIsVoid(true);
  }
}

}  // namespace mozilla::glean
