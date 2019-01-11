/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBridgeChild.h"
#include "SocketProcessLogging.h"

#include "mozilla/net/NeckoChild.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

StaticRefPtr<SocketProcessBridgeChild>
    SocketProcessBridgeChild::sSocketProcessBridgeChild;

NS_IMPL_ISUPPORTS(SocketProcessBridgeChild, nsIObserver)

// static
bool SocketProcessBridgeChild::Create(
    Endpoint<PSocketProcessBridgeChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  sSocketProcessBridgeChild =
      new SocketProcessBridgeChild(std::move(aEndpoint));
  if (sSocketProcessBridgeChild->Inited()) {
    return true;
  }

  sSocketProcessBridgeChild = nullptr;
  return false;
}

// static
already_AddRefed<SocketProcessBridgeChild>
SocketProcessBridgeChild::GetSinglton() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSocketProcessBridgeChild) {
    return nullptr;
  }

  RefPtr<SocketProcessBridgeChild> child = sSocketProcessBridgeChild.get();
  return child.forget();
}

// static
void SocketProcessBridgeChild::EnsureSocketProcessBridge(
    std::function<void()>&& aOnSuccess, std::function<void()>&& aOnFailure) {
  MOZ_ASSERT(IsNeckoChild() && gNeckoChild);
  MOZ_ASSERT(NS_IsMainThread());

  if (!gNeckoChild) {
    aOnFailure();
    return;
  }

  if (sSocketProcessBridgeChild) {
    aOnSuccess();
    return;
  }

  gNeckoChild->SendInitSocketProcessBridge()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [onSuccess = std::move(aOnSuccess), onFailure = std::move(aOnFailure)](
          Endpoint<PSocketProcessBridgeChild>&& aEndpoint) {
        if (aEndpoint.IsValid()) {
          if (SocketProcessBridgeChild::Create(std::move(aEndpoint))) {
            onSuccess();
            return;
          }
        }
        onFailure();
      },
      [onFailure = std::move(aOnFailure)](
          const mozilla::ipc::ResponseRejectReason) { onFailure(); });
}

SocketProcessBridgeChild::SocketProcessBridgeChild(
    Endpoint<PSocketProcessBridgeChild>&& aEndpoint) {
  LOG(("CONSTRUCT SocketProcessBridgeChild::SocketProcessBridgeChild\n"));

  mInited = aEndpoint.Bind(this);
  if (!mInited) {
    MOZ_ASSERT(false, "Bind failed!");
    return;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "content-child-shutdown", false);
  }
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
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, "content-child-shutdown");
  }
  MessageLoop::current()->PostTask(
      NewRunnableMethod("net::SocketProcessBridgeChild::DeferredDestroy", this,
                        &SocketProcessBridgeChild::DeferredDestroy));
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
