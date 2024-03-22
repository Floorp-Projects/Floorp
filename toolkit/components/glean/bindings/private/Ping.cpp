/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Ping.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"
#include "nsTHashMap.h"
#include "nsString.h"

namespace mozilla::glean {

namespace impl {

using CallbackMapType = nsTHashMap<uint32_t, PingTestCallback>;
using MetricIdToCallbackMutex = StaticDataMutex<UniquePtr<CallbackMapType>>;
static Maybe<MetricIdToCallbackMutex::AutoLock> GetCallbackMapLock() {
  static MetricIdToCallbackMutex sCallbacks("sCallbacks");
  auto lock = sCallbacks.Lock();
  // Test callbacks will continue to work until the end of AppShutdownTelemetry
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown)) {
    return Nothing();
  }
  if (!*lock) {
    *lock = MakeUnique<CallbackMapType>();
    RunOnShutdown(
        [&] {
          auto lock = sCallbacks.Lock();
          *lock = nullptr;  // deletes, see UniquePtr.h
        },
        ShutdownPhase::XPCOMWillShutdown);
  }
  return Some(std::move(lock));
}

void Ping::Submit(const nsACString& aReason) const {
  {
    auto callback = Maybe<PingTestCallback>();
    GetCallbackMapLock().apply(
        [&](const auto& lock) { callback = lock.ref()->Extract(mId); });
    // Calling the callback outside of the lock allows it to register a new
    // callback itself.
    if (callback) {
      callback.extract()(aReason);
    }
  }
  fog_submit_ping_by_id(mId, &aReason);
}

void Ping::TestBeforeNextSubmit(PingTestCallback&& aCallback) const {
  {
    GetCallbackMapLock().apply(
        [&](const auto& lock) { lock.ref()->InsertOrUpdate(mId, aCallback); });
  }
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
