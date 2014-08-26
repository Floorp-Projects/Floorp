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
                           const HttpChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool DeallocPHttpChannelChild(PHttpChannelChild*) MOZ_OVERRIDE;
  virtual PCookieServiceChild* AllocPCookieServiceChild() MOZ_OVERRIDE;
  virtual bool DeallocPCookieServiceChild(PCookieServiceChild*) MOZ_OVERRIDE;
  virtual PWyciwygChannelChild* AllocPWyciwygChannelChild() MOZ_OVERRIDE;
  virtual bool DeallocPWyciwygChannelChild(PWyciwygChannelChild*) MOZ_OVERRIDE;
  virtual PFTPChannelChild*
    AllocPFTPChannelChild(const PBrowserOrId& aBrowser,
                          const SerializedLoadContext& aSerialized,
                          const FTPChannelCreationArgs& aOpenArgs) MOZ_OVERRIDE;
  virtual bool DeallocPFTPChannelChild(PFTPChannelChild*) MOZ_OVERRIDE;
  virtual PWebSocketChild*
    AllocPWebSocketChild(const PBrowserOrId&,
                         const SerializedLoadContext&) MOZ_OVERRIDE;
  virtual bool DeallocPWebSocketChild(PWebSocketChild*) MOZ_OVERRIDE;
  virtual PTCPSocketChild* AllocPTCPSocketChild(const nsString& host,
                                                const uint16_t& port) MOZ_OVERRIDE;
  virtual bool DeallocPTCPSocketChild(PTCPSocketChild*) MOZ_OVERRIDE;
  virtual PTCPServerSocketChild*
    AllocPTCPServerSocketChild(const uint16_t& aLocalPort,
                               const uint16_t& aBacklog,
                               const nsString& aBinaryType) MOZ_OVERRIDE;
  virtual bool DeallocPTCPServerSocketChild(PTCPServerSocketChild*) MOZ_OVERRIDE;
  virtual PUDPSocketChild* AllocPUDPSocketChild(const nsCString& aHost,
                                                const uint16_t& aPort,
                                                const nsCString& aFilter) MOZ_OVERRIDE;
  virtual bool DeallocPUDPSocketChild(PUDPSocketChild*) MOZ_OVERRIDE;
  virtual PDNSRequestChild* AllocPDNSRequestChild(const nsCString& aHost,
                                                  const uint32_t& aFlags) MOZ_OVERRIDE;
  virtual bool DeallocPDNSRequestChild(PDNSRequestChild*) MOZ_OVERRIDE;
  virtual PRemoteOpenFileChild*
    AllocPRemoteOpenFileChild(const SerializedLoadContext& aSerialized,
                              const URIParams&,
                              const OptionalURIParams&) MOZ_OVERRIDE;
  virtual bool DeallocPRemoteOpenFileChild(PRemoteOpenFileChild*) MOZ_OVERRIDE;
  virtual PRtspControllerChild* AllocPRtspControllerChild() MOZ_OVERRIDE;
  virtual bool DeallocPRtspControllerChild(PRtspControllerChild*) MOZ_OVERRIDE;
  virtual PRtspChannelChild*
    AllocPRtspChannelChild(const RtspChannelConnectArgs& aArgs)
                           MOZ_OVERRIDE;
  virtual bool DeallocPRtspChannelChild(PRtspChannelChild*) MOZ_OVERRIDE;
  virtual PChannelDiverterChild*
  AllocPChannelDiverterChild(const ChannelDiverterArgs& channel) MOZ_OVERRIDE;
  virtual bool
  DeallocPChannelDiverterChild(PChannelDiverterChild* actor) MOZ_OVERRIDE;
  virtual bool RecvAsyncAuthPromptForNestedFrame(const uint64_t& aNestedFrameId,
                                                 const nsCString& aUri,
                                                 const nsString& aRealm,
                                                 const uint64_t& aCallbackId) MOZ_OVERRIDE;
  virtual bool RecvAppOfflineStatus(const uint32_t& aId, const bool& aOffline) MOZ_OVERRIDE;
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild *gNeckoChild;

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoChild_h
