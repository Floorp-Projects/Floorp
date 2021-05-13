/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/StringList.h"

#include "mozilla/Components.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
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
#ifndef MOZ_GLEAN_ANDROID
  fog_string_list_add(mId, &aValue);
#endif
}

void StringListMetric::Set(const nsTArray<nsCString>& aValue) const {
  // Calling `Set` on a mirrored labeled_string is likely an error.
  // We can't remove keys from the mirror scalar and handle this 'properly',
  // so you shouldn't use this operation at all.
  (void)NS_WARN_IF(ScalarIdForMetric(mId).isSome());
#ifndef MOZ_GLEAN_ANDROID
  fog_string_list_set(mId, &aValue);
#endif
}

Maybe<nsTArray<nsCString>> StringListMetric::TestGetValue(
    const nsACString& aPingName) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
  return Nothing();
#else
  if (!fog_string_list_test_has_value(mId, &aPingName)) {
    return Nothing();
  }
  nsTArray<nsCString> ret;
  fog_string_list_test_get_value(mId, &aPingName, &ret);
  return Some(std::move(ret));
#endif
}

}  // namespace impl

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
