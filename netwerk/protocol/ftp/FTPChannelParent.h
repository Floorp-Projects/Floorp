/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_FTPChannelParent_h
#define mozilla_net_FTPChannelParent_h

#include "mozilla/net/PFTPChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoParent.h"
#include "nsIParentChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"

class nsFtpChannel;

namespace mozilla {
namespace net {

class FTPChannelParent : public PFTPChannelParent
                       , public nsIParentChannel
                       , public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR

  FTPChannelParent(nsILoadContext* aLoadContext, PBOverrideStatus aOverrideStatus);
  virtual ~FTPChannelParent();

protected:
  virtual bool RecvAsyncOpen(const URIParams& uri,
                             const uint64_t& startPos,
                             const nsCString& entityID,
                             const OptionalInputStreamParams& uploadStream) MOZ_OVERRIDE;
  virtual bool RecvConnectChannel(const uint32_t& channelId) MOZ_OVERRIDE;
  virtual bool RecvCancel(const nsresult& status) MOZ_OVERRIDE;
  virtual bool RecvSuspend() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  nsRefPtr<nsFtpChannel> mChannel;

  bool mIPCClosed;

  nsCOMPtr<nsILoadContext> mLoadContext;

  PBOverrideStatus mPBOverride;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelParent_h
