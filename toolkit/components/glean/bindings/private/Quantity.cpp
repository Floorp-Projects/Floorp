/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Quantity.h"

#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

void QuantityMetric::Set(int64_t aValue) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aValue >= 0) {
    uint32_t theValue = static_cast<uint32_t>(aValue);
    if (aValue > std::numeric_limits<uint32_t>::max()) {
      theValue = std::numeric_limits<uint32_t>::max();
    }
    Telemetry::ScalarSet(scalarId.extract(), theValue);
  }
  fog_quantity_set(mId, aValue);
}

Result<Maybe<int64_t>, nsCString> QuantityMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_quantity_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_quantity_test_has_value(mId, &aPingName)) {
    return Maybe<int64_t>();
  }
  return Some(fog_quantity_test_get_value(mId, &aPingName));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanQuantity, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanQuantity, nsIGleanQuantity)

NS_IMETHODIMP
GleanQuantity::Set(int64_t aValue) {
  mQuantity.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanQuantity::TestGetValue(const nsACString& aPingName,
                            JS::MutableHandle<JS::Value> aResult) {
  auto result = mQuantity.TestGetValue(aPingName);
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
    aResult.set(JS::DoubleValue(static_cast<double>(optresult.value())));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
