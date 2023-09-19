/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Boolean.h"

#include "nsString.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"

namespace mozilla::glean {

namespace impl {

void BooleanMetric::Set(bool aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarSet(scalarId.extract(), aValue);
  } else if (IsSubmetricId(mId)) {
    GetLabeledMirrorLock().apply([&](auto& lock) {
      auto tuple = lock.ref()->MaybeGet(mId);
      if (tuple) {
        Telemetry::ScalarSet(std::get<0>(tuple.ref()), std::get<1>(tuple.ref()),
                             aValue);
      }
    });
  }
  fog_boolean_set(mId, int(aValue));
}

Result<Maybe<bool>, nsCString> BooleanMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_boolean_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_boolean_test_has_value(mId, &aPingName)) {
    return Maybe<bool>();
  }
  return Some(fog_boolean_test_get_value(mId, &aPingName));
}

}  // namespace impl

JSObject* GleanBoolean::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanBoolean_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanBoolean::Set(bool aValue) { mBoolean.Set(aValue); }

dom::Nullable<bool> GleanBoolean::TestGetValue(const nsACString& aPingName,
                                               ErrorResult& aRv) {
  dom::Nullable<bool> ret;
  auto result = mBoolean.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return ret;
  }
  auto optresult = result.unwrap();
  if (!optresult.isNothing()) {
    ret.SetValue(optresult.value());
  }
  return ret;
}

}  // namespace mozilla::glean
