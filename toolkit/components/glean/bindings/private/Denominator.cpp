/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Denominator.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

namespace impl {

void DenominatorMetric::Add(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aAmount >= 0) {
    Telemetry::ScalarAdd(scalarId.extract(), aAmount);
  }
  fog_denominator_add(mId, aAmount);
}

Result<Maybe<int32_t>, nsCString> DenominatorMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_denominator_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_denominator_test_has_value(mId, &aPingName)) {
    return Maybe<int32_t>();
  }
  return Some(fog_denominator_test_get_value(mId, &aPingName));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanDenominator, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanDenominator, nsIGleanDenominator)

NS_IMETHODIMP
GleanDenominator::Add(int32_t aAmount) {
  mDenominator.Add(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanDenominator::TestGetValue(const nsACString& aStorageName,
                               JS::MutableHandle<JS::Value> aResult) {
  auto result = mDenominator.TestGetValue(aStorageName);
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
    aResult.set(JS::Int32Value(optresult.value()));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
