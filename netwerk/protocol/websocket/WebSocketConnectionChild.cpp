/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketLog.h"
#include "WebSocketConnectionChild.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsISerializable.h"
#include "nsISSLSocketControl.h"
#include "nsITransportSecurityInfo.h"
#include "nsSerializationHelper.h"
#include "nsThreadUtils.h"
#include "WebSocketConnection.h"
#include "nsNetCID.h"
#include "nsSocketTransportService2.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(WebSocketConnectionChild, nsIHttpUpgradeListener)

WebSocketConnectionChild::WebSocketConnectionChild() {
  LOG(("WebSocketConnectionChild ctor %p\n", this));
}

WebSocketConnectionChild::~WebSocketConnectionChild() {
  LOG(("WebSocketConnectionChild dtor %p\n", this));
}

void WebSocketConnectionChild::Init(uint32_t aListenerId) {
  nsresult rv;
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (!mSocketThread) {
    return;
  }

  RefPtr<WebSocketConnectionChild> self = this;
  mSocketThread->Dispatch(NS_NewRunnableFunction(
      "WebSocketConnectionChild::Init", [self, aListenerId]() {
        mozilla::ipc::PBackgroundChild* actorChild = mozilla::ipc::
            BackgroundChild::GetOrCreateForSocketParentBridgeForCurrentThread();
        if (!actorChild) {
          return;
        }

        Unused << actorChild->SendPWebSocketConnectionConstructor(self,
                                                                  aListenerId);
      }));
}

// nsIHttpUpgradeListener
NS_IMETHODIMP
WebSocketConnectionChild::OnTransportAvailable(
    nsISocketTransport* aTransport, nsIAsyncInputStream* aSocketIn,
    nsIAsyncOutputStream* aSocketOut) {
  LOG(("WebSocketConnectionChild::OnTransportAvailable %p\n", this));
  if (!OnSocketThread()) {
    nsCOMPtr<nsISocketTransport> transport = aTransport;
    nsCOMPtr<nsIAsyncInputStream> inputStream = aSocketIn;
    nsCOMPtr<nsIAsyncOutputStream> outputStream = aSocketOut;
    RefPtr<WebSocketConnectionChild> self = this;
    return mSocketThread->Dispatch(
        NS_NewRunnableFunction("WebSocketConnectionChild::OnTransportAvailable",
                               [self, transport, inputStream, outputStream]() {
                                 Unused << self->OnTransportAvailable(
                                     transport, inputStream, outputStream);
                               }),
        NS_DISPATCH_NORMAL);
  }

  LOG(("WebSocketConnectionChild::OnTransportAvailable %p\n", this));
  MOZ_ASSERT(OnSocketThread());
  MOZ_ASSERT(!mConnection, "already called");
  MOZ_ASSERT(aTransport);

  if (!CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISSLSocketControl> tlsSocketControl;
  aTransport->GetTlsSocketControl(getter_AddRefs(tlsSocketControl));
  nsCOMPtr<nsITransportSecurityInfo> securityInfo(
      do_QueryInterface(tlsSocketControl));

  RefPtr<WebSocketConnection> connection =
      new WebSocketConnection(aTransport, aSocketIn, aSocketOut);
  nsresult rv = connection->Init(this);
  if (NS_FAILED(rv)) {
    Unused << OnUpgradeFailed(rv);
    return NS_OK;
  }

  mConnection = std::move(connection);

  Unused << SendOnTransportAvailable(securityInfo);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnUpgradeFailed(nsresult aReason) {
  if (!OnSocketThread()) {
    return mSocketThread->Dispatch(NewRunnableMethod<nsresult>(
        "WebSocketConnectionChild::OnUpgradeFailed", this,
        &WebSocketConnectionChild::OnUpgradeFailed, aReason));
  }

  if (CanSend()) {
    Unused << SendOnUpgradeFailed(aReason);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebSocketConnectionChild::OnWebSocketConnectionAvailable(
    WebSocketConnectionBase* aConnection) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvWriteOutputData(
    nsTArray<uint8_t>&& aData) {
  LOG(("WebSocketConnectionChild::RecvWriteOutputData %p\n", this));

  if (!mConnection) {
    OnError(NS_ERROR_NOT_AVAILABLE);
    return IPC_OK();
  }

  mConnection->WriteOutputData(std::move(aData));
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvStartReading() {
  LOG(("WebSocketConnectionChild::RecvStartReading %p\n", this));

  if (!mConnection) {
    OnError(NS_ERROR_NOT_AVAILABLE);
    return IPC_OK();
  }

  mConnection->StartReading();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::RecvDrainSocketData() {
  LOG(("WebSocketConnectionChild::RecvDrainSocketData %p\n", this));

  if (!mConnection) {
    OnError(NS_ERROR_NOT_AVAILABLE);
    return IPC_OK();
  }

  mConnection->DrainSocketData();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketConnectionChild::Recv__delete__() {
  LOG(("WebSocketConnectionChild::Recv__delete__ %p\n", this));

  if (!mConnection) {
    OnError(NS_ERROR_NOT_AVAILABLE);
    return IPC_OK();
  }

  mConnection->Close();
  mConnection = nullptr;
  return IPC_OK();
}

void WebSocketConnectionChild::OnError(nsresult aStatus) {
  LOG(("WebSocketConnectionChild::OnError %p\n", this));

  if (CanSend()) {
    Unused << SendOnError(aStatus);
  }
}

void WebSocketConnectionChild::OnTCPClosed() {
  LOG(("WebSocketConnectionChild::OnTCPClosed %p\n", this));

  if (CanSend()) {
    Unused << SendOnTCPClosed();
  }
}

nsresult WebSocketConnectionChild::OnDataReceived(uint8_t* aData,
                                                  uint32_t aCount) {
  LOG(("WebSocketConnectionChild::OnDataReceived %p\n", this));

  if (CanSend()) {
    nsTArray<uint8_t> data;
    data.AppendElements(aData, aCount);
    Unused << SendOnDataReceived(data);
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
