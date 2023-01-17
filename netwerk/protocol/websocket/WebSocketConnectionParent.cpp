/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketConnectionParent.h"

#include "nsIHttpChannelInternal.h"
#include "nsITransportSecurityInfo.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "WebSocketConnectionListener.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS0(WebSocketConnectionParent)

WebSocketConnectionParent::WebSocketConnectionParent(
    nsIHttpUpgradeListener* aListener)
    : mUpgradeListener(aListener), mBackgroundThread(GetCurrentEventTarget()) {
  LOG(("WebSocketConnectionParent ctor %p\n", this));
  MOZ_ASSERT(mUpgradeListener);
}

WebSocketConnectionParent::~WebSocketConnectionParent() {
  LOG(("WebSocketConnectionParent dtor %p\n", this));
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnTransportAvailable(
    nsITransportSecurityInfo* aSecurityInfo) {
  LOG(("WebSocketConnectionParent::RecvOnTransportAvailable %p\n", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (aSecurityInfo) {
    MutexAutoLock lock(mMutex);
    mSecurityInfo = aSecurityInfo;
  }

  if (mUpgradeListener) {
    Unused << mUpgradeListener->OnWebSocketConnectionAvailable(this);
    mUpgradeListener = nullptr;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnError(
    const nsresult& aStatus) {
  LOG(("WebSocketConnectionParent::RecvOnError %p\n", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (mListener) {
    mListener->OnError(aStatus);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnUpgradeFailed(
    const nsresult& aReason) {
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (mUpgradeListener) {
    Unused << mUpgradeListener->OnUpgradeFailed(aReason);
    mUpgradeListener = nullptr;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnTCPClosed() {
  LOG(("WebSocketConnectionParent::RecvOnTCPClosed %p\n", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (mListener) {
    mListener->OnTCPClosed();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionParent::RecvOnDataReceived(
    nsTArray<uint8_t>&& aData) {
  LOG(("WebSocketConnectionParent::RecvOnDataReceived %p\n", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (mListener) {
    uint8_t* buffer = const_cast<uint8_t*>(aData.Elements());
    nsresult rv = mListener->OnDataReceived(buffer, aData.Length());
    if (NS_FAILED(rv)) {
      mListener->OnError(rv);
    }
  }
  return IPC_OK();
}

void WebSocketConnectionParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("WebSocketConnectionParent::ActorDestroy %p aWhy=%d\n", this, aWhy));
  if (!mClosed) {
    // Treat this as an error when IPC is closed before
    // WebSocketConnectionParent::Close() is called.
    RefPtr<WebSocketConnectionListener> listener;
    listener.swap(mListener);
    if (listener) {
      listener->OnError(NS_ERROR_FAILURE);
    }
  }
};

nsresult WebSocketConnectionParent::Init(
    WebSocketConnectionListener* aListener) {
  NS_ENSURE_ARG_POINTER(aListener);

  mListener = aListener;
  return NS_OK;
}

void WebSocketConnectionParent::GetIoTarget(nsIEventTarget** aTarget) {
  nsCOMPtr<nsIEventTarget> target = mBackgroundThread;
  return target.forget(aTarget);
}

void WebSocketConnectionParent::Close() {
  LOG(("WebSocketConnectionParent::Close %p\n", this));

  mClosed = true;

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    Unused << self->Send__delete__(self);
    self->mListener = nullptr;
  };

  if (mBackgroundThread->IsOnCurrentThread()) {
    task();
  } else {
    mBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "WebSocketConnectionParent::Close", std::move(task)));
  }
}

nsresult WebSocketConnectionParent::WriteOutputData(
    const uint8_t* aHdrBuf, uint32_t aHdrBufLength, const uint8_t* aPayloadBuf,
    uint32_t aPayloadBufLength) {
  LOG(("WebSocketConnectionParent::WriteOutputData %p", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<uint8_t> data;
  data.AppendElements(aHdrBuf, aHdrBufLength);
  data.AppendElements(aPayloadBuf, aPayloadBufLength);
  return SendWriteOutputData(data) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult WebSocketConnectionParent::StartReading() {
  LOG(("WebSocketConnectionParent::StartReading %p\n", this));

  RefPtr<WebSocketConnectionParent> self = this;
  auto task = [self{std::move(self)}]() {
    if (!self->CanSend()) {
      if (self->mListener) {
        self->mListener->OnError(NS_ERROR_NOT_AVAILABLE);
      }
      return;
    }

    Unused << self->SendStartReading();
  };

  if (mBackgroundThread->IsOnCurrentThread()) {
    task();
  } else {
    mBackgroundThread->Dispatch(NS_NewRunnableFunction(
        "WebSocketConnectionParent::SendStartReading", std::move(task)));
  }

  return NS_OK;
}

void WebSocketConnectionParent::DrainSocketData() {
  LOG(("WebSocketConnectionParent::DrainSocketData %p\n", this));
  MOZ_ASSERT(mBackgroundThread->IsOnCurrentThread());

  if (!CanSend()) {
    if (mListener) {
      mListener->OnError(NS_ERROR_NOT_AVAILABLE);
    }
    return;
  }

  Unused << SendDrainSocketData();
}

nsresult WebSocketConnectionParent::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  LOG(("WebSocketConnectionParent::GetSecurityInfo() %p\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_ARG_POINTER(aSecurityInfo);

  MutexAutoLock lock(mMutex);
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
