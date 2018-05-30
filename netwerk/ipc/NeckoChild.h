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
class NeckoChild :
  public PNeckoChild
{
public:
  NeckoChild() = default;
  virtual ~NeckoChild();

  static void InitNeckoChild();

protected:
  virtual PHttpChannelChild*
    AllocPHttpChannelChild(const PBrowserOrId&, const SerializedLoadContext&,
                           const HttpChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPHttpChannelChild(PHttpChannelChild*) override;

  virtual PStunAddrsRequestChild* AllocPStunAddrsRequestChild() override;
  virtual bool
    DeallocPStunAddrsRequestChild(PStunAddrsRequestChild* aActor) override;

  virtual PAltDataOutputStreamChild* AllocPAltDataOutputStreamChild(const nsCString& type, const int64_t& predictedSize, PHttpChannelChild* channel) override;
  virtual bool DeallocPAltDataOutputStreamChild(PAltDataOutputStreamChild* aActor) override;

  virtual PCookieServiceChild* AllocPCookieServiceChild() override;
  virtual bool DeallocPCookieServiceChild(PCookieServiceChild*) override;
  virtual PWyciwygChannelChild* AllocPWyciwygChannelChild() override;
  virtual bool DeallocPWyciwygChannelChild(PWyciwygChannelChild*) override;
  virtual PFTPChannelChild*
    AllocPFTPChannelChild(const PBrowserOrId& aBrowser,
                          const SerializedLoadContext& aSerialized,
                          const FTPChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPFTPChannelChild(PFTPChannelChild*) override;
  virtual PWebSocketChild*
    AllocPWebSocketChild(const PBrowserOrId&,
                         const SerializedLoadContext&,
                         const uint32_t&) override;
  virtual bool DeallocPWebSocketChild(PWebSocketChild*) override;
  virtual PTCPSocketChild* AllocPTCPSocketChild(const nsString& host,
                                                const uint16_t& port) override;
  virtual bool DeallocPTCPSocketChild(PTCPSocketChild*) override;
  virtual PTCPServerSocketChild*
    AllocPTCPServerSocketChild(const uint16_t& aLocalPort,
                               const uint16_t& aBacklog,
                               const bool& aUseArrayBuffers) override;
  virtual bool DeallocPTCPServerSocketChild(PTCPServerSocketChild*) override;
  virtual PUDPSocketChild* AllocPUDPSocketChild(const Principal& aPrincipal,
                                                const nsCString& aFilter) override;
  virtual bool DeallocPUDPSocketChild(PUDPSocketChild*) override;
  virtual PDNSRequestChild* AllocPDNSRequestChild(const nsCString& aHost,
                                                  const OriginAttributes& aOriginAttributes,
                                                  const uint32_t& aFlags) override;
  virtual bool DeallocPDNSRequestChild(PDNSRequestChild*) override;
  virtual PDataChannelChild* AllocPDataChannelChild(const uint32_t& channelId) override;
  virtual bool DeallocPDataChannelChild(PDataChannelChild* child) override;
  virtual PFileChannelChild* AllocPFileChannelChild(const uint32_t& channelId) override;
  virtual bool DeallocPFileChannelChild(PFileChannelChild* child) override;
  virtual PSimpleChannelChild* AllocPSimpleChannelChild(const uint32_t& channelId) override;
  virtual bool DeallocPSimpleChannelChild(PSimpleChannelChild* child) override;
  virtual PChannelDiverterChild*
  AllocPChannelDiverterChild(const ChannelDiverterArgs& channel) override;
  virtual bool
  DeallocPChannelDiverterChild(PChannelDiverterChild* actor) override;
  virtual PTransportProviderChild*
  AllocPTransportProviderChild() override;
  virtual bool
  DeallocPTransportProviderChild(PTransportProviderChild* aActor) override;
  virtual mozilla::ipc::IPCResult RecvAsyncAuthPromptForNestedFrame(const TabId& aNestedFrameId,
                                                                    const nsCString& aUri,
                                                                    const nsString& aRealm,
                                                                    const uint64_t& aCallbackId) override;
  virtual PWebSocketEventListenerChild*
    AllocPWebSocketEventListenerChild(const uint64_t& aInnerWindowID) override;
  virtual bool DeallocPWebSocketEventListenerChild(PWebSocketEventListenerChild*) override;

  /* Predictor Messsages */
  virtual mozilla::ipc::IPCResult RecvPredOnPredictPrefetch(const URIParams& aURI,
                                                            const uint32_t& aHttpStatus) override;
  virtual mozilla::ipc::IPCResult RecvPredOnPredictPreconnect(const URIParams& aURI) override;
  virtual mozilla::ipc::IPCResult RecvPredOnPredictDNS(const URIParams& aURI) override;

  virtual mozilla::ipc::IPCResult RecvSpeculativeConnectRequest() override;
  virtual mozilla::ipc::IPCResult RecvNetworkChangeNotification(nsCString const& type) override;
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild *gNeckoChild;

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoChild_h
