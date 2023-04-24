/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Numerator.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

namespace impl {

void NumeratorMetric::AddToNumerator(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId && aAmount >= 0) {
    Telemetry::ScalarAdd(scalarId.extract(), u"numerator"_ns, aAmount);
  }
  fog_numerator_add_to_numerator(mId, aAmount);
}

Result<Maybe<std::pair<int32_t, int32_t>>, nsCString>
NumeratorMetric::TestGetValue(const nsACString& aPingName) const {
  nsCString err;
  if (fog_numerator_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_numerator_test_has_value(mId, &aPingName)) {
    return Maybe<std::pair<int32_t, int32_t>>();
  }
  int32_t num = 0;
  int32_t den = 0;
  fog_numerator_test_get_value(mId, &aPingName, &num, &den);
  return Some(std::make_pair(num, den));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanNumerator, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanNumerator, nsIGleanNumerator)

NS_IMETHODIMP
GleanNumerator::AddToNumerator(int32_t aAmount) {
  mNumerator.AddToNumerator(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanNumerator::TestGetValue(const nsACString& aPingName, JSContext* aCx,
                             JS::MutableHandle<JS::Value> aResult) {
  auto result = mNumerator.TestGetValue(aPingName);
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
    // Build return value of the form: { numerator: n, denominator: d }
    JS::Rooted<JSObject*> root(aCx, JS_NewPlainObject(aCx));
    if (!root) {
      return NS_ERROR_FAILURE;
    }
    auto pair = optresult.extract();
    int32_t num = pair.first;
    int32_t den = pair.second;
    if (!JS_DefineProperty(aCx, root, "numerator", static_cast<double>(num),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    if (!JS_DefineProperty(aCx, root, "denominator", static_cast<double>(den),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    aResult.setObject(*root);
  }
  return NS_OK;
}

}  // namespace mozilla::glean
