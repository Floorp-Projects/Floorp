/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Boolean.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "Common.h"

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
        Telemetry::ScalarSet(Get<0>(tuple.ref()), Get<1>(tuple.ref()), aValue);
      }
    });
  }
  fog_boolean_set(mId, int(aValue));
}

Result<Maybe<bool>, nsCString> BooleanMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_boolean_test_get_error(mId, &aPingName, &err)) {
    return Err(err);
  }
  if (!fog_boolean_test_has_value(mId, &aPingName)) {
    return Maybe<bool>();
  }
  return Some(fog_boolean_test_get_value(mId, &aPingName));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanBoolean, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanBoolean, nsIGleanBoolean)

NS_IMETHODIMP
GleanBoolean::Set(bool aValue) {
  mBoolean.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanBoolean::TestGetValue(const nsACString& aStorageName,
                           JS::MutableHandleValue aResult) {
  auto result = mBoolean.TestGetValue(aStorageName);
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
    aResult.set(JS::BooleanValue(optresult.value()));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
