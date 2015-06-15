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
  NeckoChild();
  virtual ~NeckoChild();

  static void InitNeckoChild();
  static void DestroyNeckoChild();

protected:
  virtual PHttpChannelChild*
    AllocPHttpChannelChild(const PBrowserOrId&, const SerializedLoadContext&,
                           const HttpChannelCreationArgs& aOpenArgs) override;
  virtual bool DeallocPHttpChannelChild(PHttpChannelChild*) override;
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
                         const SerializedLoadContext&) override;
  virtual bool DeallocPWebSocketChild(PWebSocketChild*) override;
  virtual PTCPSocketChild* AllocPTCPSocketChild(const nsString& host,
                                                const uint16_t& port) override;
  virtual bool DeallocPTCPSocketChild(PTCPSocketChild*) override;
  virtual PTCPServerSocketChild*
    AllocPTCPServerSocketChild(const uint16_t& aLocalPort,
                               const uint16_t& aBacklog,
                               const nsString& aBinaryType) override;
  virtual bool DeallocPTCPServerSocketChild(PTCPServerSocketChild*) override;
  virtual PUDPSocketChild* AllocPUDPSocketChild(const Principal& aPrincipal,
                                                const nsCString& aFilter) override;
  virtual bool DeallocPUDPSocketChild(PUDPSocketChild*) override;
  virtual PDNSRequestChild* AllocPDNSRequestChild(const nsCString& aHost,
                                                  const uint32_t& aFlags,
                                                  const nsCString& aNetworkInterface) override;
  virtual bool DeallocPDNSRequestChild(PDNSRequestChild*) override;
  virtual PRemoteOpenFileChild*
    AllocPRemoteOpenFileChild(const SerializedLoadContext& aSerialized,
                              const URIParams&,
                              const OptionalURIParams&) override;
  virtual bool DeallocPRemoteOpenFileChild(PRemoteOpenFileChild*) override;
  virtual PDataChannelChild* AllocPDataChannelChild(const uint32_t& channelId) override;
  virtual bool DeallocPDataChannelChild(PDataChannelChild* child) override;
  virtual PRtspControllerChild* AllocPRtspControllerChild() override;
  virtual bool DeallocPRtspControllerChild(PRtspControllerChild*) override;
  virtual PRtspChannelChild*
    AllocPRtspChannelChild(const RtspChannelConnectArgs& aArgs)
                           override;
  virtual bool DeallocPRtspChannelChild(PRtspChannelChild*) override;
  virtual PChannelDiverterChild*
  AllocPChannelDiverterChild(const ChannelDiverterArgs& channel) override;
  virtual bool
  DeallocPChannelDiverterChild(PChannelDiverterChild* actor) override;
  virtual bool RecvAsyncAuthPromptForNestedFrame(const TabId& aNestedFrameId,
                                                 const nsCString& aUri,
                                                 const nsString& aRealm,
                                                 const uint64_t& aCallbackId) override;
  virtual bool RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline) override;
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild *gNeckoChild;

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoChild_h
