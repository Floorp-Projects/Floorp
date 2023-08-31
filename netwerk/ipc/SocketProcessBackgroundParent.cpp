/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBackgroundParent.h"
#include "SocketProcessLogging.h"

#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/HttpConnectionMgrParent.h"
#include "mozilla/net/WebSocketConnectionParent.h"
#include "mozilla/psm/IPCClientCertsParent.h"
#include "mozilla/psm/VerifySSLServerCertParent.h"
#include "mozilla/psm/SelectTLSClientAuthCertParent.h"
#include "nsIHttpChannelInternal.h"

namespace mozilla::net {

SocketProcessBackgroundParent::SocketProcessBackgroundParent() {
  LOG(("SocketProcessBackgroundParent ctor"));
}

SocketProcessBackgroundParent::~SocketProcessBackgroundParent() {
  LOG(("SocketProcessBackgroundParent dtor"));
}

mozilla::ipc::IPCResult
SocketProcessBackgroundParent::RecvInitVerifySSLServerCert(
    Endpoint<PVerifySSLServerCertParent>&& aEndpoint,
    nsTArray<ByteArray>&& aPeerCertChain, const nsACString& aHostName,
    const int32_t& aPort, const OriginAttributes& aOriginAttributes,
    const Maybe<ByteArray>& aStapledOCSPResponse,
    const Maybe<ByteArray>& aSctsFromTLSExtension,
    const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
    const uint32_t& aProviderFlags, const uint32_t& aCertVerifierFlags) {
  LOG(("SocketProcessBackgroundParent::RecvInitVerifySSLServerCert\n"));
  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_FAILED(NS_CreateBackgroundTaskQueue("VerifySSLServerCert",
                                             getter_AddRefs(transportQueue)))) {
    return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
  }

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "InitVerifySSLServerCert",
      [endpoint = std::move(aEndpoint),
       peerCertChain = std::move(aPeerCertChain),
       hostName = nsCString(aHostName), port(aPort),
       originAttributes(aOriginAttributes),
       stapledOCSPResponse = std::move(aStapledOCSPResponse),
       sctsFromTLSExtension = std::move(aSctsFromTLSExtension),
       dcInfo = std::move(aDcInfo), providerFlags(aProviderFlags),
       certVerifierFlags(aCertVerifierFlags)]() mutable {
        RefPtr<psm::VerifySSLServerCertParent> parent =
            new psm::VerifySSLServerCertParent();
        if (!endpoint.Bind(parent)) {
          return;
        }

        parent->Dispatch(std::move(peerCertChain), hostName, port,
                         originAttributes, stapledOCSPResponse,
                         sctsFromTLSExtension, dcInfo, providerFlags,
                         certVerifierFlags);
      }));

  return IPC_OK();
}

mozilla::ipc::IPCResult
SocketProcessBackgroundParent::RecvInitSelectTLSClientAuthCert(
    Endpoint<PSelectTLSClientAuthCertParent>&& aEndpoint,
    const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
    const int32_t& aPort, const uint32_t& aProviderFlags,
    const uint32_t& aProviderTlsFlags, const ByteArray& aServerCertBytes,
    nsTArray<ByteArray>&& aCANames, const uint64_t& aBrowserId) {
  LOG(("SocketProcessBackgroundParent::RecvInitSelectTLSClientAuthCert\n"));
  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_FAILED(NS_CreateBackgroundTaskQueue("SelectTLSClientAuthCert",
                                             getter_AddRefs(transportQueue)))) {
    return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
  }

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "InitSelectTLSClientAuthCert",
      [endpoint = std::move(aEndpoint), hostName = nsCString(aHostName),
       originAttributes(aOriginAttributes), port(aPort),
       providerFlags(aProviderFlags), providerTlsFlags(aProviderTlsFlags),
       serverCertBytes(aServerCertBytes), CANAMEs(std::move(aCANames)),
       browserId(aBrowserId)]() mutable {
        RefPtr<psm::SelectTLSClientAuthCertParent> parent =
            new psm::SelectTLSClientAuthCertParent();
        if (!endpoint.Bind(parent)) {
          return;
        }

        parent->Dispatch(hostName, originAttributes, port, providerFlags,
                         providerTlsFlags, serverCertBytes, std::move(CANAMEs),
                         browserId);
      }));

  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessBackgroundParent::RecvInitIPCClientCerts(
    Endpoint<PIPCClientCertsParent>&& aEndpoint) {
  LOG(("SocketProcessBackgroundParent::RecvInitIPCClientCerts\n"));
  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_FAILED(NS_CreateBackgroundTaskQueue("IPCClientCerts",
                                             getter_AddRefs(transportQueue)))) {
    return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
  }

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "InitIPCClientCerts", [endpoint = std::move(aEndpoint)]() mutable {
        RefPtr<psm::IPCClientCertsParent> parent =
            new psm::IPCClientCertsParent();
        endpoint.Bind(parent);
      }));
  return IPC_OK();
}

mozilla::ipc::IPCResult
SocketProcessBackgroundParent::RecvInitWebSocketConnection(
    Endpoint<PWebSocketConnectionParent>&& aEndpoint,
    const uint32_t& aListenerId) {
  LOG(("SocketProcessBackgroundParent::RecvInitWebSocketConnection\n"));
  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_FAILED(NS_CreateBackgroundTaskQueue("WebSocketConnection",
                                             getter_AddRefs(transportQueue)))) {
    return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
  }

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "InitWebSocketConnection",
      [endpoint = std::move(aEndpoint), aListenerId]() mutable {
        Maybe<nsCOMPtr<nsIHttpUpgradeListener>> listener =
            net::HttpConnectionMgrParent::GetAndRemoveHttpUpgradeListener(
                aListenerId);
        if (!listener) {
          return;
        }

        RefPtr<WebSocketConnectionParent> actor =
            new WebSocketConnectionParent(*listener);
        endpoint.Bind(actor);
      }));
  return IPC_OK();
}

void SocketProcessBackgroundParent::ActorDestroy(ActorDestroyReason aReason) {
  LOG(("SocketProcessBackgroundParent::ActorDestroy"));
}

}  // namespace mozilla::net
