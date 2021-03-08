/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FOG.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/FOGIPC.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/MozPromise.h"
#include "nsContentUtils.h"
#include "nsIFOG.h"
#include "nsIUserIdleService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

static StaticRefPtr<FOG> gFOG;

// We wait for 5s of idle before dumping IPC.
// This number hasn't been tuned, so if you have a reason to change it,
// please by all means do.
const uint32_t kIdleSecs = 5;

// static
already_AddRefed<FOG> FOG::GetSingleton() {
  if (gFOG) {
    return do_AddRef(gFOG);
  }

  gFOG = new FOG();

  nsresult rv;
  nsCOMPtr<nsIUserIdleService> idleService =
      do_GetService("@mozilla.org/widget/useridleservice;1", &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);
  MOZ_ASSERT(idleService);
  if (NS_WARN_IF(NS_FAILED(idleService->AddIdleObserver(gFOG, kIdleSecs)))) {
    glean::fog::failed_idle_registration.Set(true);
  }

  RunOnShutdown([&] {
    gFOG->Shutdown();
    gFOG = nullptr;
  });
  return do_AddRef(gFOG);
}

void FOG::Shutdown() {
#ifndef MOZ_GLEAN_ANDROID
  glean::impl::fog_shutdown();
#endif
}

NS_IMETHODIMP
FOG::InitializeFOG(const nsACString& aDataPathOverride) {
#ifdef MOZ_GLEAN_ANDROID
  return NS_OK;
#else
  return glean::impl::fog_init(&aDataPathOverride);
#endif
}

NS_IMETHODIMP
FOG::SetLogPings(bool aEnableLogPings) {
#ifdef MOZ_GLEAN_ANDROID
  return NS_OK;
#else
  return glean::impl::fog_set_log_pings(aEnableLogPings);
#endif
}

NS_IMETHODIMP
FOG::SetTagPings(const nsACString& aDebugTag) {
#ifdef MOZ_GLEAN_ANDROID
  return NS_OK;
#else
  return glean::impl::fog_set_debug_view_tag(&aDebugTag);
#endif
}

NS_IMETHODIMP
FOG::SendPing(const nsACString& aPingName) {
#ifdef MOZ_GLEAN_ANDROID
  return NS_OK;
#else
  return glean::impl::fog_submit_ping(&aPingName);
#endif
}

NS_IMETHODIMP
FOG::TestFlushAllChildren(JSContext* aCx, mozilla::dom::Promise** aOutPromise) {
  NS_ENSURE_ARG(aOutPromise);
  *aOutPromise = nullptr;
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult erv;
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, erv);
  if (NS_WARN_IF(erv.Failed())) {
    return erv.StealNSResult();
  }

  glean::FlushAndUseFOGData()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [promise]() { promise->MaybeResolveWithUndefined(); });

  promise.forget(aOutPromise);
  return NS_OK;
}

NS_IMETHODIMP
FOG::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // On idle, opportunistically flush child process data to the parent.
  if (!strcmp(aTopic, OBSERVER_TOPIC_IDLE)) {
    glean::FlushAndUseFOGData();
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(FOG, nsIFOG, nsIObserver)

}  //  namespace mozilla
