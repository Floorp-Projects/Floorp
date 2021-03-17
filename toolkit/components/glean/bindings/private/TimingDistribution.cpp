/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/TimingDistribution.h"

#include "mozilla/Components.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsString.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanTimingDistribution, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimingDistribution, nsIGleanTimingDistribution)

NS_IMETHODIMP
GleanTimingDistribution::Start(JSContext* aCx, JS::MutableHandleValue aResult) {
  if (!dom::ToJSValue(aCx, mTimingDist.Start(), aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::StopAndAccumulate(uint64_t aId) {
  mTimingDist.StopAndAccumulate(std::move(aId));
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::Cancel(uint64_t aId) {
  mTimingDist.Cancel(std::move(aId));
  return NS_OK;
}

NS_IMETHODIMP
GleanTimingDistribution::TestGetValue(const nsACString& aPingName,
                                      JSContext* aCx,
                                      JS::MutableHandleValue aResult) {
  auto result = mTimingDist.TestGetValue(aPingName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    // Build return value of the form: { sum: #, values: {bucket1: count1, ...}
    JS::RootedObject root(aCx, JS_NewPlainObject(aCx));
    if (!root) {
      return NS_ERROR_FAILURE;
    }
    uint64_t sum = result.ref().sum;
    if (!JS_DefineProperty(aCx, root, "sum", static_cast<double>(sum),
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    JS::RootedObject valuesObj(aCx, JS_NewPlainObject(aCx));
    if (!valuesObj ||
        !JS_DefineProperty(aCx, root, "values", valuesObj, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    auto& data = result.ref().values;
    for (const auto& entry : data) {
      const uint64_t bucket = entry.GetKey();
      const uint64_t count = entry.GetData();
      if (!JS_DefineProperty(aCx, valuesObj,
                             nsPrintfCString("%" PRIu64, bucket).get(),
                             static_cast<double>(count), JSPROP_ENUMERATE)) {
        return NS_ERROR_FAILURE;
      }
    }
    aResult.setObject(*root);
  }
  return NS_OK;
}

}  // namespace mozilla::glean
