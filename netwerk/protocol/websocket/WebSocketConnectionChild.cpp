/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketConnectionChild.h"

#include "nsISerializable.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "nsWebSocketConnection.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(WebSocketConnectionChild, nsIWebSocketConnectionListener,
                  nsIHttpUpgradeListener)

WebSocketConnectionChild::WebSocketConnectionChild() {
  LOG(("WebSocketConnectionChild ctor %p\n", this));
}

WebSocketConnectionChild::~WebSocketConnectionChild() {
  LOG(("WebSocketConnectionChild dtor %p\n", this));
}

// nsIHttpUpgradeListener
NS_IMETHODIMP
WebSocketConnectionChild::OnTransportAvailable(
    nsISocketTransport* aTransport, nsIAsyncInputStream* aSocketIn,
    nsIAsyncOutputStream* aSocketOut) {
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsISocketTransport> transport = aTransport;
    nsCOMPtr<nsIAsyncInputStream> inputStream = aSocketIn;
    nsCOMPtr<nsIAsyncOutputStream> outputStream = aSocketOut;
    RefPtr<WebSocketConnectionChild> self = this;
    return NS_DispatchToMainThread(
        NS_NewRunnableFunction("WebSocketConnectionChild::OnTransportAvailable",
                               [self, transport, inputStream, outputStream]() {
                                 Unused << self->OnTransportAvailable(
                                     transport, inputStream, outputStream);
                               }),
        NS_DISPATCH_NORMAL);
  }

  LOG(("WebSocketConnectionChild::OnTransportAvailable %p\n", this));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mConnection, "already called");
  MOZ_ASSERT(aTransport);

  nsAutoCString serializedSecurityInfo;
  nsCOMPtr<nsISupports> secInfoSupp;
  aTransport->GetSecurityInfo(getter_AddRefs(secInfoSupp));
  if (secInfoSupp) {
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer) {
      NS_SerializeToString(secInfoSer, serializedSecurityInfo);
    }
  }

  mConnection = new nsWebSocketConnection(aTransport, aSocketIn, aSocketOut);
  nsresult rv = mConnection->Init(this, GetCurrentEventTarget());
  if (NS_FAILED(rv)) {
    Unused << SendOnError(rv);
    return NS_OK;
  }

  Unused << SendOnTransportAvailable(serializedSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnWebSocketConnectionAvailable(
    nsIWebSocketConnection* aConnection) {
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnUpgradeFailed(nsresult aReason) {
  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(NewRunnableMethod<nsresult>(
        "WebSocketConnectionChild::OnUpgradeFailed", this,
        &WebSocketConnectionChild::OnUpgradeFailed, aReason));
  }

  if (CanSend()) {
    Unused << SendOnUpgradeFailed(aReason);
  }
  return NS_OK;
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvEnqueueOutgoingData(
    nsTArray<uint8_t>&& aHeader, nsTArray<uint8_t>&& aPayload) {
  LOG(("WebSocketConnectionChild::RecvEnqueueOutgoingData %p\n", this));

  if (!mConnection) {
    MOZ_ASSERT(false);
    return IPC_FAIL(this, "Connection is not available");
  }

  mConnection->EnqueueOutputData(std::move(aHeader), std::move(aPayload));
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvStartReading() {
  LOG(("WebSocketConnectionChild::RecvStartReading %p\n", this));

  if (!mConnection) {
    MOZ_ASSERT(false);
    return IPC_FAIL(this, "Connection is not available");
  }

  mConnection->StartReading();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvDrainSocketData() {
  LOG(("WebSocketConnectionChild::RecvDrainSocketData %p\n", this));

  if (!mConnection) {
    MOZ_ASSERT(false);
    return IPC_FAIL(this, "Connection is not available");
  }

  mConnection->DrainSocketData();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::Recv__delete__() {
  LOG(("WebSocketConnectionChild::Recv__delete__ %p\n", this));

  if (!mConnection) {
    return IPC_OK();
  }

  mConnection->Close();
  mConnection = nullptr;
  return IPC_OK();
}

NS_IMETHODIMP
WebSocketConnectionChild::OnError(nsresult aStatus) {
  LOG(("WebSocketConnectionChild::OnError %p\n", this));

  if (CanSend()) {
    Unused << SendOnError(aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnTCPClosed() {
  LOG(("WebSocketConnectionChild::OnTCPClosed %p\n", this));

  if (CanSend()) {
    Unused << SendOnTCPClosed();
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnDataReceived(uint8_t* aData, uint32_t aCount) {
  LOG(("WebSocketConnectionChild::OnDataReceived %p\n", this));

  if (CanSend()) {
    nsTArray<uint8_t> data;
    data.AppendElements(aData, aCount);
    Unused << SendOnDataReceived(std::move(data));
  }
  return NS_OK;
}

void WebSocketConnectionChild::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("WebSocketConnectionChild::ActorDestroy %p\n", this));
  if (mConnection) {
    mConnection->Close();
    mConnection = nullptr;
  }
}

}  // namespace net
}  // namespace mozilla
