/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Ping.h"

#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

#ifndef MOZ_GLEAN_ANDROID
using CallbackMapType = nsTHashMap<uint32_t, PingTestCallback>;
using MetricIdToCallbackMutex = StaticDataMutex<UniquePtr<CallbackMapType>>;
static MetricIdToCallbackMutex::AutoLock GetCallbackMapLock() {
  static MetricIdToCallbackMutex sCallbacks("sCallbacks");
  auto lock = sCallbacks.Lock();
  if (!*lock) {
    *lock = MakeUnique<CallbackMapType>();
  }
  return lock;
}
#endif

void Ping::Submit(const nsACString& aReason) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
#else
  {
    auto lock = GetCallbackMapLock();
    auto callback = lock.ref()->Extract(mId);
    if (callback) {
      callback.extract()(aReason);
    }
  }
  fog_submit_ping_by_id(mId, &aReason);
#endif
}

void Ping::TestBeforeNextSubmit(PingTestCallback&& aCallback) const {
#ifdef MOZ_GLEAN_ANDROID
  return;
#else
  {
    auto lock = GetCallbackMapLock();
    lock.ref()->InsertOrUpdate(mId, aCallback);
  }
#endif
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanPing, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanPing, nsIGleanPing)

NS_IMETHODIMP
GleanPing::Submit(const nsACString& aReason) {
  mPing.Submit(aReason);
  return NS_OK;
}

NS_IMETHODIMP
GleanPing::TestBeforeNextSubmit(nsIGleanPingTestCallback* aCallback) {
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }
  // Throw the bare ptr into a COM ptr to keep it around in the lambda.
  nsCOMPtr<nsIGleanPingTestCallback> callback = aCallback;
  mPing.TestBeforeNextSubmit(
      [callback](const nsACString& aReason) { callback->Call(aReason); });
  return NS_OK;
}

}  // namespace mozilla::glean
