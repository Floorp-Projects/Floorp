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

  FTPChannelParent();
  virtual ~FTPChannelParent();

protected:
  NS_OVERRIDE virtual bool RecvAsyncOpen(const IPC::URI& uri,
                                         const PRUint64& startPos,
                                         const nsCString& entityID,
                                         const IPC::InputStream& uploadStream,
                                         const bool& aUsePrivateBrowsing);
  NS_OVERRIDE virtual bool RecvConnectChannel(const PRUint32& channelId);
  NS_OVERRIDE virtual bool RecvCancel(const nsresult& status);
  NS_OVERRIDE virtual bool RecvSuspend();
  NS_OVERRIDE virtual bool RecvResume();

  NS_OVERRIDE virtual void ActorDestroy(ActorDestroyReason why);

  nsRefPtr<nsFtpChannel> mChannel;

  bool mIPCClosed;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelParent_h
