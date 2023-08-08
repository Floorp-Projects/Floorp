/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_GIOCHANNELPARENT_H
#define NS_GIOCHANNELPARENT_H

#include "mozilla/net/PGIOChannelParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsIParentChannel.h"
#include "nsIInterfaceRequestor.h"

class nsILoadContext;

namespace mozilla {

namespace dom {
class BrowserParent;
}  // namespace dom

namespace net {
class ChannelEventQueue;

class GIOChannelParent final : public PGIOChannelParent,
                               public nsIParentChannel,
                               public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR

  GIOChannelParent(dom::BrowserParent* aIframeEmbedding,
                   nsILoadContext* aLoadContext,
                   PBOverrideStatus aOverrideStatus);

  bool Init(const GIOChannelCreationArgs& aOpenArgs);

 protected:
  virtual ~GIOChannelParent() = default;

  bool DoAsyncOpen(const URIParams& aURI, const uint64_t& aStartPos,
                   const nsCString& aEntityID,
                   const Maybe<mozilla::ipc::IPCStream>& aUploadStream,
                   const LoadInfoArgs& aLoadInfoArgs,
                   const uint32_t& aLoadFlags);

  // used to connect redirected-to channel in parent with just created
  // ChildChannel.  Used during HTTP->FTP redirects.
  bool ConnectChannel(const uint64_t& channelId);

  virtual mozilla::ipc::IPCResult RecvCancel(const nsresult& status) override;
  virtual mozilla::ipc::IPCResult RecvSuspend() override;
  virtual mozilla::ipc::IPCResult RecvResume() override;

  virtual void ActorDestroy(ActorDestroyReason why) override;

  nsCOMPtr<nsIChannel> mChannel;

  bool mIPCClosed = false;

  nsCOMPtr<nsILoadContext> mLoadContext;

  PBOverrideStatus mPBOverride;

  // Set to the canceled status value if the main channel was canceled.
  nsresult mStatus = NS_OK;

  RefPtr<mozilla::dom::BrowserParent> mBrowserParent;

  RefPtr<ChannelEventQueue> mEventQ;
};

}  // namespace net
}  // namespace mozilla

#endif /* NS_GIOCHANNELPARENT_H */
