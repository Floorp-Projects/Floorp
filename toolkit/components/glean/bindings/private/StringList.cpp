/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/StringList.h"

#include "mozilla/ResultVariant.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/GleanMetricsBinding.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

namespace impl {

void StringListMetric::Add(const nsACString& aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue),
                         true);
  }
  fog_string_list_add(mId, &aValue);
}

void StringListMetric::Set(const nsTArray<nsCString>& aValue) const {
  // Calling `Set` on a mirrored labeled_string is likely an error.
  // We can't remove keys from the mirror scalar and handle this 'properly',
  // so you shouldn't use this operation at all.
  (void)NS_WARN_IF(ScalarIdForMetric(mId).isSome());
  fog_string_list_set(mId, &aValue);
}

Result<Maybe<nsTArray<nsCString>>, nsCString> StringListMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_string_list_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_string_list_test_has_value(mId, &aPingName)) {
    return Maybe<nsTArray<nsCString>>();
  }
  nsTArray<nsCString> ret;
  fog_string_list_test_get_value(mId, &aPingName, &ret);
  return Some(std::move(ret));
}

}  // namespace impl

/* virtual */
JSObject* GleanStringList::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return dom::GleanStringList_Binding::Wrap(aCx, this, aGivenProto);
}

void GleanStringList::Add(const nsACString& aValue) { mStringList.Add(aValue); }

void GleanStringList::Set(const dom::Sequence<nsCString>& aValue) {
  mStringList.Set(aValue);
}

void GleanStringList::TestGetValue(const nsACString& aPingName,
                                   dom::Nullable<nsTArray<nsCString>>& aResult,
                                   ErrorResult& aRv) {
  auto result = mStringList.TestGetValue(aPingName);
  if (result.isErr()) {
    aRv.ThrowDataError(result.unwrapErr());
    return;
  }
  auto optresult = result.unwrap();
  if (!optresult.isNothing()) {
    aResult.SetValue(optresult.extract());
  }
}

}  // namespace mozilla::glean
