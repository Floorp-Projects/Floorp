/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBridgeChild.h"
#include "SocketProcessLogging.h"

#include "mozilla/AppShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/NeckoChild.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"

namespace mozilla {

using dom::ContentChild;

namespace net {

StaticRefPtr<SocketProcessBridgeChild>
    SocketProcessBridgeChild::sSocketProcessBridgeChild;

NS_IMPL_ISUPPORTS(SocketProcessBridgeChild, nsIObserver)

// static
bool SocketProcessBridgeChild::Create(
    Endpoint<PSocketProcessBridgeChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  sSocketProcessBridgeChild = new SocketProcessBridgeChild();

  if (!aEndpoint.Bind(sSocketProcessBridgeChild)) {
    MOZ_ASSERT(false, "Bind failed!");
    sSocketProcessBridgeChild = nullptr;
    return false;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(sSocketProcessBridgeChild, "content-child-shutdown", false);
  }

  sSocketProcessBridgeChild->mSocketProcessPid = aEndpoint.OtherPid();
  return true;
}

// static
already_AddRefed<SocketProcessBridgeChild>
SocketProcessBridgeChild::GetSingleton() {
  RefPtr<SocketProcessBridgeChild> child = sSocketProcessBridgeChild;
  return child.forget();
}

// static
RefPtr<SocketProcessBridgeChild::GetPromise>
SocketProcessBridgeChild::GetSocketProcessBridge() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::network_process_enabled()) {
    return GetPromise::CreateAndReject(nsCString("Socket process disabled!"),
                                       __func__);
  }

  if (!gNeckoChild) {
    return GetPromise::CreateAndReject(nsCString("No NeckoChild!"), __func__);
  }

  // ContentChild is shutting down, we should not try to create
  // SocketProcessBridgeChild.
  ContentChild* content = ContentChild::GetSingleton();
  if (!content || content->IsShuttingDown()) {
    return GetPromise::CreateAndReject(
        nsCString("ContentChild is shutting down."), __func__);
  }

  if (sSocketProcessBridgeChild) {
    return GetPromise::CreateAndResolve(sSocketProcessBridgeChild, __func__);
  }

  return gNeckoChild->SendInitSocketProcessBridge()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [](NeckoChild::InitSocketProcessBridgePromise::ResolveOrRejectValue&&
             aResult) {
        ContentChild* content = ContentChild::GetSingleton();
        if (!content || content->IsShuttingDown()) {
          return GetPromise::CreateAndReject(
              nsCString("ContentChild is shutting down."), __func__);
        }
        if (!sSocketProcessBridgeChild) {
          if (aResult.IsReject()) {
            return GetPromise::CreateAndReject(
                nsCString("SendInitSocketProcessBridge failed"), __func__);
          }

          if (!aResult.ResolveValue().IsValid()) {
            return GetPromise::CreateAndReject(
                nsCString(
                    "SendInitSocketProcessBridge resolved with an invalid "
                    "endpoint!"),
                __func__);
          }

          if (!SocketProcessBridgeChild::Create(
                  std::move(aResult.ResolveValue()))) {
            return GetPromise::CreateAndReject(
                nsCString("SendInitSocketProcessBridge resolved with a valid "
                          "endpoint, "
                          "but SocketProcessBridgeChild::Create failed!"),
                __func__);
          }
        }

        return GetPromise::CreateAndResolve(sSocketProcessBridgeChild,
                                            __func__);
      });
}

SocketProcessBridgeChild::SocketProcessBridgeChild() : mShuttingDown(false) {
  LOG(("CONSTRUCT SocketProcessBridgeChild::SocketProcessBridgeChild\n"));
}

SocketProcessBridgeChild::~SocketProcessBridgeChild() {
  LOG(("DESTRUCT SocketProcessBridgeChild::SocketProcessBridgeChild\n"));
}

mozilla::ipc::IPCResult SocketProcessBridgeChild::RecvTest() {
  LOG(("SocketProcessBridgeChild::RecvTest\n"));
  return IPC_OK();
}

void SocketProcessBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("SocketProcessBridgeChild::ActorDestroy\n"));
  if (AbnormalShutdown == aWhy) {
    if (gNeckoChild &&
        !AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
      // Let NeckoParent know that the socket process connections must be
      // rebuilt.
      gNeckoChild->SendResetSocketProcessBridge();
    }

    nsresult res;
    nsCOMPtr<nsISerialEventTarget> mSTSThread =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res) && mSTSThread) {
      // This must be called off the main thread.  If we don't make this call
      // ipc::BackgroundChild::GetOrCreateSocketActorForCurrentThread() will
      // return the previous actor that is no longer able to send. This causes
      // rebuilding the socket process connections to fail.
      MOZ_ALWAYS_SUCCEEDS(mSTSThread->Dispatch(NS_NewRunnableFunction(
          "net::SocketProcessBridgeChild::ActorDestroy",
          []() { ipc::BackgroundChild::CloseForCurrentThread(); })));
    }
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, "content-child-shutdown");
  }
  GetCurrentSerialEventTarget()->Dispatch(
      NewRunnableMethod("net::SocketProcessBridgeChild::DeferredDestroy", this,
                        &SocketProcessBridgeChild::DeferredDestroy));
  mShuttingDown = true;
}

NS_IMETHODIMP
SocketProcessBridgeChild::Observe(nsISupports* aSubject, const char* aTopic,
                                  const char16_t* aData) {
  if (!strcmp(aTopic, "content-child-shutdown")) {
    PSocketProcessBridgeChild::Close();
  }
  return NS_OK;
}

void SocketProcessBridgeChild::DeferredDestroy() {
  MOZ_ASSERT(NS_IsMainThread());

  sSocketProcessBridgeChild = nullptr;
}

}  // namespace net
}  // namespace mozilla
