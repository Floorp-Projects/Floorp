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
    AllocPHttpChannelChild(PBrowserChild*, const SerializedLoadContext&,
                           const HttpChannelCreationArgs& aOpenArgs);
  virtual bool DeallocPHttpChannelChild(PHttpChannelChild*);
  virtual PCookieServiceChild* AllocPCookieServiceChild();
  virtual bool DeallocPCookieServiceChild(PCookieServiceChild*);
  virtual PWyciwygChannelChild* AllocPWyciwygChannelChild();
  virtual bool DeallocPWyciwygChannelChild(PWyciwygChannelChild*);
  virtual PFTPChannelChild*
    AllocPFTPChannelChild(PBrowserChild* aBrowser,
                          const SerializedLoadContext& aSerialized,
                          const FTPChannelCreationArgs& aOpenArgs);
  virtual bool DeallocPFTPChannelChild(PFTPChannelChild*);
  virtual PWebSocketChild* AllocPWebSocketChild(PBrowserChild*, const SerializedLoadContext&);
  virtual bool DeallocPWebSocketChild(PWebSocketChild*);
  virtual PTCPSocketChild* AllocPTCPSocketChild();
  virtual bool DeallocPTCPSocketChild(PTCPSocketChild*);
  virtual PTCPServerSocketChild* AllocPTCPServerSocketChild(const uint16_t& aLocalPort,
                                                       const uint16_t& aBacklog,
                                                       const nsString& aBinaryType);
  virtual bool DeallocPTCPServerSocketChild(PTCPServerSocketChild*);
  virtual PUDPSocketChild* AllocPUDPSocketChild(const nsCString& aHost,
                                                const uint16_t& aPort);
  virtual bool DeallocPUDPSocketChild(PUDPSocketChild*);
  virtual PRemoteOpenFileChild* AllocPRemoteOpenFileChild(const URIParams&,
                                                          const OptionalURIParams&);
  virtual bool DeallocPRemoteOpenFileChild(PRemoteOpenFileChild*);
  virtual PRtspControllerChild* AllocPRtspControllerChild();
  virtual bool DeallocPRtspControllerChild(PRtspControllerChild*);
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild *gNeckoChild;

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoChild_h
