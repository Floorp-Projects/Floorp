/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/String.h"

#include "Common.h"
#include "jsapi.h"
#include "js/String.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

namespace impl {

void StringMetric::Set(const nsACString& aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarSet(scalarId.extract(), NS_ConvertUTF8toUTF16(aValue));
  }
#ifndef MOZ_GLEAN_ANDROID
  fog_string_set(mId, &aValue);
#endif
}

Result<Maybe<nsCString>, nsCString> StringMetric::TestGetValue(
    const nsACString& aPingName) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
  return Maybe<nsCString>();
#else
  nsCString err;
  if (fog_string_test_get_error(mId, &aPingName, &err)) {
    return Err(err);
  }
  if (!fog_string_test_has_value(mId, &aPingName)) {
    return Maybe<nsCString>();
  }
  nsCString ret;
  fog_string_test_get_value(mId, &aPingName, &ret);
  return Some(ret);
#endif
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanString, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanString, nsIGleanString)

NS_IMETHODIMP
GleanString::Set(const nsACString& aValue) {
  mString.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanString::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                          JS::MutableHandleValue aResult) {
  auto result = mString.TestGetValue(aStorageName);
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
    const NS_ConvertUTF8toUTF16 str(optresult.ref());
    aResult.set(
        JS::StringValue(JS_NewUCStringCopyN(aCx, str.Data(), str.Length())));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
