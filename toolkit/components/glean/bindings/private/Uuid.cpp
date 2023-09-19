/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Uuid.h"

#include "jsapi.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

void UuidMetric::Set(const nsACString& aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue));
  }
  fog_uuid_set(mId, &aValue);
}

void UuidMetric::GenerateAndSet() const {
  // We don't have the generated value to mirror to the scalar,
  // so calling this function on a mirrored metric is likely an error.
  (void)NS_WARN_IF(ScalarIdForMetric(mId).isSome());
  fog_uuid_generate_and_set(mId);
}

Result<Maybe<nsCString>, nsCString> UuidMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_uuid_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_uuid_test_has_value(mId, &aPingName)) {
    return Maybe<nsCString>();
  }
  nsCString ret;
  fog_uuid_test_get_value(mId, &aPingName, &ret);
  return Some(ret);
}

}  // namespace impl

/* virtual */
JSObject* GleanUuid::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanUuid_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanUuid::Set(const nsACString& aValue) { mUuid.Set(aValue); }

void GleanUuid::GenerateAndSet() { mUuid.GenerateAndSet(); }

void GleanUuid::TestGetValue(const nsACString& aPingName, nsCString& aResult,
                             ErrorResult& aRv) {
  auto result = mUuid.TestGetValue(aPingName);
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
