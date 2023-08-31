/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SocketProcessBackgroundParent_h
#define mozilla_net_SocketProcessBackgroundParent_h

#include "mozilla/net/PSocketProcessBackgroundParent.h"

namespace mozilla {
namespace net {

class SocketProcessBackgroundParent final
    : public PSocketProcessBackgroundParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SocketProcessBackgroundParent, final)

  SocketProcessBackgroundParent();

  mozilla::ipc::IPCResult RecvInitVerifySSLServerCert(
      Endpoint<PVerifySSLServerCertParent>&& aEndpoint,
      nsTArray<ByteArray>&& aPeerCertChain, const nsACString& aHostName,
      const int32_t& aPort, const OriginAttributes& aOriginAttributes,
      const Maybe<ByteArray>& aStapledOCSPResponse,
      const Maybe<ByteArray>& aSctsFromTLSExtension,
      const Maybe<DelegatedCredentialInfoArg>& aDcInfo,
      const uint32_t& aProviderFlags, const uint32_t& aCertVerifierFlags);

  mozilla::ipc::IPCResult RecvInitIPCClientCerts(
      Endpoint<PIPCClientCertsParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvInitSelectTLSClientAuthCert(
      Endpoint<PSelectTLSClientAuthCertParent>&& aEndpoint,
      const nsACString& aHostName, const OriginAttributes& aOriginAttributes,
      const int32_t& aPort, const uint32_t& aProviderFlags,
      const uint32_t& aProviderTlsFlags, const ByteArray& aServerCertBytes,
      nsTArray<ByteArray>&& aCANames, const uint64_t& aBrowserId);

  mozilla::ipc::IPCResult RecvInitWebSocketConnection(
      Endpoint<PWebSocketConnectionParent>&& aEndpoint,
      const uint32_t& aListenerId);

  void ActorDestroy(ActorDestroyReason aReason) override;

 private:
  ~SocketProcessBackgroundParent();
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_SocketProcessBackgroundParent_h
