/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutThirdParty.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {

static StaticRefPtr<AboutThirdParty> sSingleton;

NS_IMPL_ISUPPORTS(AboutThirdParty, nsIAboutThirdParty);

/*static*/
already_AddRefed<AboutThirdParty> AboutThirdParty::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new AboutThirdParty;
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

AboutThirdParty::AboutThirdParty()
    : mPromise(new BackgroundThreadPromise::Private(__func__)) {}

void AboutThirdParty::BackgroundThread() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mWorkerState == WorkerState::Running);

  mWorkerState = WorkerState::Done;
}

RefPtr<BackgroundThreadPromise> AboutThirdParty::CollectSystemInfoAsync() {
  MOZ_ASSERT(NS_IsMainThread());

  // Allow only the first call to start a background task.
  if (mWorkerState.compareExchange(WorkerState::Init, WorkerState::Running)) {
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "AboutThirdParty::BackgroundThread", [self = RefPtr{this}]() mutable {
          self->BackgroundThread();
          NS_DispatchToMainThread(NS_NewRunnableFunction(
              "AboutThirdParty::BackgroundThread Done",
              [self]() { self->mPromise->Resolve(true, __func__); }));
        });

    nsresult rv =
        NS_DispatchBackgroundTask(runnable.forget(), NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      mPromise->Reject(rv, __func__);
    }
  }

  return mPromise;
}

NS_IMETHODIMP
AboutThirdParty::CollectSystemInfo(JSContext* aCx, dom::Promise** aResult) {
  MOZ_ASSERT(NS_IsMainThread());

  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);
  MOZ_ASSERT(global);

  ErrorResult result;
  RefPtr<dom::Promise> promise(dom::Promise::Create(global, result));
  if (NS_WARN_IF(result.Failed())) {
    return result.StealNSResult();
  }

  CollectSystemInfoAsync()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](bool) { promise->MaybeResolve(JS::NullHandleValue); },
      [promise](nsresult aRv) { promise->MaybeReject(aRv); });

  promise.forget(aResult);
  return NS_OK;
}

}  // namespace mozilla
