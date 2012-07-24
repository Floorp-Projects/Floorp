/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_FTPChannelParent_h
#define mozilla_net_FTPChannelParent_h

#include "mozilla/net/PFTPChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIParentChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"

class nsFtpChannel;

namespace mozilla {
namespace net {

class FTPChannelParent : public PFTPChannelParent
                       , public nsIParentChannel
                       , public nsIInterfaceRequestor
                       , public nsILoadContext
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSILOADCONTEXT

  FTPChannelParent();
  virtual ~FTPChannelParent();

protected:
  virtual bool RecvAsyncOpen(const IPC::URI& uri,
                             const PRUint64& startPos,
                             const nsCString& entityID,
                             const IPC::InputStream& uploadStream,
                             const bool& haveLoadContext,
                             const bool& isContent,
                             const bool& usingPrivateBrowsing,
                             const bool& isInBrowserElement,
                             const PRUint32& appId,
                             const nsCString& extendedOrigin) MOZ_OVERRIDE;
  virtual bool RecvConnectChannel(const PRUint32& channelId) MOZ_OVERRIDE;
  virtual bool RecvCancel(const nsresult& status) MOZ_OVERRIDE;
  virtual bool RecvSuspend() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  nsRefPtr<nsFtpChannel> mChannel;

  bool mIPCClosed;

  // fields for impersonating nsILoadContext
  bool mHaveLoadContext       : 1;
  bool mIsContent             : 1;
  bool mUsePrivateBrowsing    : 1;
  bool mIsInBrowserElement    : 1;

  PRUint32 mAppId;
  nsCString mExtendedOrigin;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelParent_h
