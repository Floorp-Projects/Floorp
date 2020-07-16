/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketConnectionParent.h"

#include "nsIHttpChannelInternal.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(WebSocketConnectionParent)
NS_IMPL_RELEASE(WebSocketConnectionParent)
NS_INTERFACE_MAP_BEGIN(WebSocketConnectionParent)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketConnection)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(WebSocketConnectionParent)
NS_INTERFACE_MAP_END

WebSocketConnectionParent::WebSocketConnectionParent(
    nsIHttpUpgradeListener* aListener)
    : mUpgradeListener(aListener), mClosed(false) {
  LOG(("WebSocketConnectionParent ctor %p\n", this));
  MOZ_ASSERT(mUpgradeListener);
}

WebSocketConnectionParent::~WebSocketConnectionParent() {
  LOG(("WebSocketConnectionParent dtor %p\n", this));
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnTransportAvailable(
    const nsCString& aSecurityInfoSerialization) {
  LOG(("WebSocketConnectionParent::RecvOnTransportAvailable %p\n", this));
  if (!aSecurityInfoSerialization.IsEmpty()) {
    nsresult rv = NS_DeserializeObject(aSecurityInfoSerialization,
                                       getter_AddRefs(mSecurityInfo));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                          "Deserializing security info should not fail");
    Unused << rv;  // So we don't get an unused error in release builds.
  }

  if (mUpgradeListener) {
    Unused << mUpgradeListener->OnWebSocketConnectionAvailable(this);
    mUpgradeListener = nullptr;
  }
  return IPC_OK();
}

static inline void DispatchHelper(nsIEventTarget* aTarget, const char* aName,
                                  std::function<void()>&& aTask) {
  if (aTarget->IsOnCurrentThread()) {
    aTask();
  } else {
    aTarget->Dispatch(NS_NewRunnableFunction(aName, std::move(aTask)),
                      NS_DISPATCH_NORMAL);
  }
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnError(
    const nsresult& aStatus) {
  LOG(("WebSocketConnectionParent::RecvOnError %p\n", this));
  MOZ_ASSERT(mEventTarget);

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}, aStatus]() {
    if (self->mListener) {
      self->mListener->OnError(aStatus);
    }
  };

  DispatchHelper(mEventTarget, "WebSocketConnectionParent::RecvOnError",
                 std::move(task));
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnUpgradeFailed(
    const nsresult& aReason) {
  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}, aReason]() {
    if (self->mUpgradeListener) {
      Unused << self->mUpgradeListener->OnUpgradeFailed(aReason);
      self->mUpgradeListener = nullptr;
    }
  };

  DispatchHelper(mEventTarget, "WebSocketConnectionParent::RecvOnUpgradeFailed",
                 std::move(task));
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnTCPClosed() {
  LOG(("WebSocketConnectionParent::RecvOnTCPClosed %p\n", this));
  MOZ_ASSERT(mEventTarget);

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    if (self->mListener) {
      self->mListener->OnTCPClosed();
    }
  };

  DispatchHelper(mEventTarget, "WebSocketConnectionParent::RecvOnTCPClosed",
                 std::move(task));
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnDataReceived(
    nsTArray<uint8_t>&& aData) {
  LOG(("WebSocketConnectionParent::RecvOnDataReceived %p\n", this));
  MOZ_ASSERT(mEventTarget);

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)},
               data = CopyableTArray{std::move(aData)}]() {
    if (self->mListener) {
      uint8_t* buffer = const_cast<uint8_t*>(data.Elements());
      nsresult rv = self->mListener->OnDataReceived(buffer, data.Length());
      if (NS_FAILED(rv)) {
        self->mListener->OnError(rv);
      }
    }
  };

  DispatchHelper(mEventTarget, "WebSocketConnectionParent::RecvOnDataReceived",
                 std::move(task));
  return IPC_OK();
}

void WebSocketConnectionParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("WebSocketConnectionParent::ActorDestroy %p aWhy=%d\n", this, aWhy));
  if (!mClosed) {
    // Treat this as an error when IPC is closed before
    // WebSocketConnectionParent::Close() is called.
    nsCOMPtr<nsIWebSocketConnectionListener> listener;
    listener.swap(mListener);
    if (listener) {
      listener->OnError(NS_ERROR_FAILURE);
    }
  }
};

NS_IMETHODIMP
WebSocketConnectionParent::Init(nsIWebSocketConnectionListener* aListener,
                                nsIEventTarget* aEventTarget) {
  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_ARG_POINTER(aEventTarget);

  mListener = aListener;
  mEventTarget = aEventTarget;
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionParent::Close() {
  LOG(("WebSocketConnectionParent::Close %p\n", this));

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    self->mClosed = true;
    if (self->CanSend()) {
      Unused << self->Send__delete__(self);
      self->mListener = nullptr;
    }
  };

  DispatchHelper(GetMainThreadEventTarget(), "WebSocketConnectionParent::Close",
                 std::move(task));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionParent::EnqueueOutputData(const uint8_t* aHdrBuf,
                                             uint32_t aHdrBufLength,
                                             const uint8_t* aPayloadBuf,
                                             uint32_t aPayloadBufLength) {
  LOG(("WebSocketConnectionParent::EnqueueOutputData %p\n", this));

  RefPtr<WebSocketConnectionParent> self = this;
  nsTArray<uint8_t> header;
  header.AppendElements(aHdrBuf, aHdrBufLength);
  nsTArray<uint8_t> payload;
  payload.AppendElements(aPayloadBuf, aPayloadBufLength);
  auto task = [self{std::move(self)},
               header = CopyableTArray{std::move(header)},
               payload = CopyableTArray{std::move(payload)}]() mutable {
    if (self->CanSend()) {
      Unused << self->SendEnqueueOutgoingData(std::move(header),
                                              std::move(payload));
    }
  };

  DispatchHelper(GetMainThreadEventTarget(),
                 "WebSocketConnectionParent::EnqueOutputData", std::move(task));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionParent::StartReading() {
  LOG(("WebSocketConnectionParent::StartReading %p\n", this));

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    if (self->CanSend()) {
      Unused << self->SendStartReading();
    }
  };

  DispatchHelper(GetMainThreadEventTarget(),
                 "WebSocketConnectionParent::SendStartReading",
                 std::move(task));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionParent::DrainSocketData() {
  LOG(("WebSocketConnectionParent::DrainSocketData %p\n", this));

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    if (self->CanSend()) {
      Unused << self->SendDrainSocketData();
    }
  };

  DispatchHelper(GetMainThreadEventTarget(),
                 "WebSocketConnectionParent::DrainSocketData", std::move(task));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionParent::GetSecurityInfo(nsISupports** aSecurityInfo) {
  LOG(("WebSocketConnectionParent::GetSecurityInfo() %p\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG_POINTER(aSecurityInfo);
  nsCOMPtr<nsISupports> info = mSecurityInfo;
  info.forget(aSecurityInfo);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
