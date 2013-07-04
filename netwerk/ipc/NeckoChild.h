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
    AllocPHttpChannel(PBrowserChild*, const SerializedLoadContext&,
                      const HttpChannelCreationArgs& aOpenArgs);
  virtual bool DeallocPHttpChannel(PHttpChannelChild*);
  virtual PCookieServiceChild* AllocPCookieService();
  virtual bool DeallocPCookieService(PCookieServiceChild*);
  virtual PWyciwygChannelChild* AllocPWyciwygChannel();
  virtual bool DeallocPWyciwygChannel(PWyciwygChannelChild*);
  virtual PFTPChannelChild*
    AllocPFTPChannel(PBrowserChild* aBrowser,
                     const SerializedLoadContext& aSerialized,
                     const FTPChannelCreationArgs& aOpenArgs);
  virtual bool DeallocPFTPChannel(PFTPChannelChild*);
  virtual PWebSocketChild* AllocPWebSocket(PBrowserChild*, const SerializedLoadContext&);
  virtual bool DeallocPWebSocket(PWebSocketChild*);
  virtual PTCPSocketChild* AllocPTCPSocket(const nsString& aHost,
                                           const uint16_t& aPort,
                                           const bool& useSSL,
                                           const nsString& aBinaryType,
                                           PBrowserChild* aBrowser);
  virtual bool DeallocPTCPSocket(PTCPSocketChild*);
  virtual PRemoteOpenFileChild* AllocPRemoteOpenFile(const URIParams&,
                                                     PBrowserChild*);
  virtual bool DeallocPRemoteOpenFile(PRemoteOpenFileChild*);
};

/**
 * Reference to the PNecko Child protocol.
 * Null if this is not a content process.
 */
extern PNeckoChild *gNeckoChild;

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoChild_h
