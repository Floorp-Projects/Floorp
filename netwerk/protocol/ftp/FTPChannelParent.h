/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_FTPChannelParent_h
#define mozilla_net_FTPChannelParent_h

#include "ADivertableParentChannel.h"
#include "mozilla/net/PFTPChannelParent.h"
#include "mozilla/net/NeckoParent.h"
#include "nsIParentChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIFTPChannelParentInternal.h"

class nsILoadContext;

namespace mozilla {

namespace dom {
class BrowserParent;
}  // namespace dom

namespace net {
class ChannelEventQueue;

class FTPChannelParent final : public PFTPChannelParent,
                               public nsIParentChannel,
                               public nsIInterfaceRequestor,
                               public ADivertableParentChannel,
                               public nsIChannelEventSink,
                               public nsIFTPChannelParentInternal {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  FTPChannelParent(dom::BrowserParent* aIframeEmbedding,
                   nsILoadContext* aLoadContext,
                   PBOverrideStatus aOverrideStatus);

  bool Init(const FTPChannelCreationArgs& aOpenArgs);

  // ADivertableParentChannel functions.
  void DivertTo(nsIStreamListener* aListener) override;
  nsresult SuspendForDiversion() override;
  nsresult SuspendMessageDiversion() override;
  nsresult ResumeMessageDiversion() override;
  nsresult CancelDiversion() override;

  // Calls OnStartRequest for "DivertTo" listener, then notifies child channel
  // that it should divert OnDataAvailable and OnStopRequest calls to this
  // parent channel.
  void StartDiversion();

  // Handles calling OnStart/Stop if there are errors during diversion.
  // Called asynchronously from FailDiversion.
  void NotifyDiversionFailed(nsresult aErrorCode);

  NS_IMETHOD SetErrorMsg(const char* aMsg, bool aUseUTF8) override;

 protected:
  virtual ~FTPChannelParent();

  // private, supporting function for ADivertableParentChannel.
  nsresult ResumeForDiversion();

  // Asynchronously calls NotifyDiversionFailed.
  void FailDiversion(nsresult aErrorCode);

  bool DoAsyncOpen(const URIParams& aURI, const uint64_t& aStartPos,
                   const nsCString& aEntityID,
                   const Maybe<IPCStream>& aUploadStream,
                   const Maybe<LoadInfoArgs>& aLoadInfoArgs,
                   const uint32_t& aLoadFlags);

  // used to connect redirected-to channel in parent with just created
  // ChildChannel.  Used during HTTP->FTP redirects.
  bool ConnectChannel(const uint32_t& channelId);

  void DivertOnDataAvailable(const nsCString& data, const uint64_t& offset,
                             const uint32_t& count);
  void DivertOnStopRequest(const nsresult& statusCode);
  void DivertComplete();

  friend class FTPDivertDataAvailableEvent;
  friend class FTPDivertStopRequestEvent;
  friend class FTPDivertCompleteEvent;

  virtual mozilla::ipc::IPCResult RecvCancel(const nsresult& status) override;
  virtual mozilla::ipc::IPCResult RecvSuspend() override;
  virtual mozilla::ipc::IPCResult RecvResume() override;
  virtual mozilla::ipc::IPCResult RecvDivertOnDataAvailable(
      const nsCString& data, const uint64_t& offset,
      const uint32_t& count) override;
  virtual mozilla::ipc::IPCResult RecvDivertOnStopRequest(
      const nsresult& statusCode) override;
  virtual mozilla::ipc::IPCResult RecvDivertComplete() override;

  nsresult ResumeChannelInternalIfPossible();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  // if configured to use HTTP proxy for FTP, this can an an HTTP channel.
  nsCOMPtr<nsIChannel> mChannel;

  bool mIPCClosed;

  nsCOMPtr<nsILoadContext> mLoadContext;

  PBOverrideStatus mPBOverride;

  // If OnStart/OnData/OnStop have been diverted from the child, forward them to
  // this listener.
  nsCOMPtr<nsIStreamListener> mDivertToListener;
  // Set to the canceled status value if the main channel was canceled.
  nsresult mStatus;
  // Once set, no OnStart/OnData/OnStop calls should be accepted; conversely, it
  // must be set when RecvDivertOnData/~DivertOnStop/~DivertComplete are
  // received from the child channel.
  bool mDivertingFromChild;
  // Set if OnStart|StopRequest was called during a diversion from the child.
  bool mDivertedOnStartRequest;

  // Set if we successfully suspended the nsHttpChannel for diversion. Unset
  // when we call ResumeForDiversion.
  bool mSuspendedForDiversion;
  RefPtr<mozilla::dom::BrowserParent> mBrowserParent;

  RefPtr<ChannelEventQueue> mEventQ;

  nsCString mErrorMsg;
  bool mUseUTF8;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_FTPChannelParent_h
