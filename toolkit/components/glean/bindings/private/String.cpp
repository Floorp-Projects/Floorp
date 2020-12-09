/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/String.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

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
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    const NS_ConvertUTF8toUTF16 str(result.value());
    aResult.set(
        JS::StringValue(JS_NewUCStringCopyN(aCx, str.Data(), str.Length())));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
