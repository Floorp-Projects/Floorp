/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Text.h"

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

void TextMetric::Set(const nsACString& aValue) const {
  fog_text_set(mId, &aValue);
}

Result<Maybe<nsCString>, nsCString> TextMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_text_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_text_test_has_value(mId, &aPingName)) {
    return Maybe<nsCString>();
  }
  nsCString ret;
  fog_text_test_get_value(mId, &aPingName, &ret);
  return Some(ret);
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanText, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanText, nsIGleanText)

NS_IMETHODIMP
GleanText::Set(const nsACString& aValue) {
  mText.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanText::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                        JS::MutableHandle<JS::Value> aResult) {
  auto result = mText.TestGetValue(aStorageName);
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
