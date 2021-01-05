/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/StringList.h"

#include "mozilla/Components.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIClassInfoImpl.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanStringList, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanStringList, nsIGleanStringList)

NS_IMETHODIMP
GleanStringList::Add(const nsACString& aValue) {
  mStringList.Add(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanStringList::Set(const nsTArray<nsCString>& aValue) {
  mStringList.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanStringList::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                              JS::MutableHandleValue aResult) {
  auto result = mStringList.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    if (!dom::ToJSValue(aCx, result.ref(), aResult)) {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

}  // namespace mozilla::glean
