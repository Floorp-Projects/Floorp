/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Counter.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

namespace impl {

void CounterMetric::Add(int32_t aAmount) const {
  auto scalarId = ScalarIdForMetric(mId);
  if (scalarId) {
    Telemetry::ScalarAdd(scalarId.extract(), aAmount);
  } else if (IsSubmetricId(mId)) {
    auto lock = GetLabeledMirrorLock();
    auto tuple = lock.ref()->MaybeGet(mId);
    if (tuple && aAmount > 0) {
      Telemetry::ScalarSet(Get<0>(tuple.ref()), Get<1>(tuple.ref()),
                           (uint32_t)aAmount);
    }
  }
#ifndef MOZ_GLEAN_ANDROID
  fog_counter_add(mId, aAmount);
#endif
}

Maybe<int32_t> CounterMetric::TestGetValue(const nsACString& aPingName) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
  return Nothing();
#else
  if (!fog_counter_test_has_value(mId, &aPingName)) {
    return Nothing();
  }
  return Some(fog_counter_test_get_value(mId, &aPingName));
#endif
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanCounter, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanCounter, nsIGleanCounter)

NS_IMETHODIMP
GleanCounter::Add(uint32_t aAmount) {
  mCounter.Add(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanCounter::TestGetValue(const nsACString& aStorageName,
                           JS::MutableHandleValue aResult) {
  auto result = mCounter.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    aResult.set(JS::Int32Value(result.value()));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
