/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NeckoChild_h
#define mozilla_net_NeckoChild_h

#include "mozilla/net/PNeckoChild.h"
#include "mozilla/net/NeckoCommon.h"

namespace mozilla {
namespace net {

// Header file contents
class NeckoChild : public PNeckoChild {
  friend class PNeckoChild;

 public:
  NeckoChild() = default;
  virtual ~NeckoChild();

  static void InitNeckoChild();

 protected:
  PStunAddrsRequestChild* AllocPStunAddrsRequestChild();
  bool DeallocPStunAddrsRequestChild(PStunAddrsRequestChild* aActor);

  PWebrtcTCPSocketChild* AllocPWebrtcTCPSocketChild(const Maybe<TabId>& tabId);
  bool DeallocPWebrtcTCPSocketChild(PWebrtcTCPSocketChild* aActor);

  PAltDataOutputStreamChild* AllocPAltDataOutputStreamChild(
      const nsCString& type, const int64_t& predictedSize,
      PHttpChannelChild* channel);
  bool DeallocPAltDataOutputStreamChild(PAltDataOutputStreamChild* aActor);

  already_AddRefed<PDocumentChannelChild> AllocPDocumentChannelChild(
      const PBrowserOrId& aBrowser, const SerializedLoadContext& aSerialized,
      const DocumentChannelCreationArgs& args);

  PCookieServiceChild* AllocPCookieServiceChild();
  bool DeallocPCookieServiceChild(PCookieServiceChild*);
  PFTPChannelChild* AllocPFTPChannelChild(
      const PBrowserOrId& aBrowser, const SerializedLoadContext& aSerialized,
      const FTPChannelCreationArgs& aOpenArgs);
  bool DeallocPFTPChannelChild(PFTPChannelChild*);
  PWebSocketChild* AllocPWebSocketChild(const PBrowserOrId&,
                                        const SerializedLoadContext&,
                                        const uint32_t&);
  bool DeallocPWebSocketChild(PWebSocketChild*);
  PTCPSocketChild* AllocPTCPSocketChild(const nsString& host,
                                        const uint16_t& port);
  bool DeallocPTCPSocketChild(PTCPSocketChild*);
  PTCPServerSocketChild* AllocPTCPServerSocketChild(
      const uint16_t& aLocalPort, const uint16_t& aBacklog,
      const bool& aUseArrayBuffers);
  bool DeallocPTCPServerSocketChild(PTCPServerSocketChild*);
  PUDPSocketChild* AllocPUDPSocketChild(nsIPrincipal* aPrincipal,
                                        const nsCString& aFilter);
  bool DeallocPUDPSocketChild(PUDPSocketChild*);
  PSimpleChannelChild* AllocPSimpleChannelChild(const uint32_t& channelId);
  bool DeallocPSimpleChannelChild(PSimpleChannelChild* child);
  PChannelDiverterChild* AllocPChannelDiverterChild(
      const ChannelDiverterArgs& channel);
  bool DeallocPChannelDiverterChild(PChannelDiverterChild* actor);
  PTransportProviderChild* AllocPTransportProviderChild();
  bool DeallocPTransportProviderChild(PTransportProviderChild* aActor);
  mozilla::ipc::IPCResult RecvAsyncAuthPromptForNestedFrame(
      const TabId& aNestedFrameId, const nsCString& aUri,
      const nsString& aRealm, const uint64_t& aCallbackId);
  PWebSocketEventListenerChild* AllocPWebSocketEventListenerChild(
      const uint64_t& aInnerWindowID);
  bool DeallocPWebSocketEventListenerChild(PWebSocketEventListenerChild*);

  /* Predictor Messsages */
  mozilla::ipc::IPCResult RecvPredOnPredictPrefetch(
      nsIURI* aURI, const uint32_t& aHttpStatus);
  mozilla::ipc::IPCResult RecvPredOnPredictPreconnect(nsIURI* aURI);
  mozilla::ipc::IPCResult RecvPredOnPredictDNS(nsIURI* aURI);

  mozilla::ipc::IPCResult RecvSpeculativeConnectRequest();
  mozilla::ipc::IPCResult RecvNetworkChangeNotification(nsCString const& type);

  PClassifierDummyChannelChild* AllocPClassifierDummyChannelChild(
      nsIURI* aURI, nsIURI* aTopWindowURI, const nsresult& aTopWindowURIResult,
      const Maybe<LoadInfoArgs>& aLoadInfo);

  bool DeallocPClassifierDummyChannelChild(
      PClassifierDummyChannelChild* aChannel);
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild* gNeckoChild;

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_NeckoChild_h
