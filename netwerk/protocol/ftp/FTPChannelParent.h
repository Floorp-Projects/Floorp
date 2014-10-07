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
#include "OfflineObserver.h"

class nsFtpChannel;
class nsILoadContext;

namespace mozilla {
namespace net {

class FTPChannelParent : public PFTPChannelParent
                       , public nsIParentChannel
                       , public nsIInterfaceRequestor
                       , public ADivertableParentChannel
                       , public nsIChannelEventSink
                       , public DisconnectableParent
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPARENTCHANNEL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK

  FTPChannelParent(nsILoadContext* aLoadContext, PBOverrideStatus aOverrideStatus);

  bool Init(const FTPChannelCreationArgs& aOpenArgs);

  // ADivertableParentChannel functions.
  void DivertTo(nsIStreamListener *aListener) MOZ_OVERRIDE;
  nsresult SuspendForDiversion() MOZ_OVERRIDE;

  // Calls OnStartRequest for "DivertTo" listener, then notifies child channel
  // that it should divert OnDataAvailable and OnStopRequest calls to this
  // parent channel.
  void StartDiversion();

  // Handles calling OnStart/Stop if there are errors during diversion.
  // Called asynchronously from FailDiversion.
  void NotifyDiversionFailed(nsresult aErrorCode, bool aSkipResume = true);

protected:
  virtual ~FTPChannelParent();

  // private, supporting function for ADivertableParentChannel.
  nsresult ResumeForDiversion();

  // Asynchronously calls NotifyDiversionFailed.
  void FailDiversion(nsresult aErrorCode, bool aSkipResume = true);

  bool DoAsyncOpen(const URIParams& aURI, const uint64_t& aStartPos,
                   const nsCString& aEntityID,
                   const OptionalInputStreamParams& aUploadStream,
                   const ipc::PrincipalInfo& aRequestingPrincipalInfo,
                   const uint32_t& aSecurityFlags,
                   const uint32_t& aContentPolicyType);

  // used to connect redirected-to channel in parent with just created
  // ChildChannel.  Used during HTTP->FTP redirects.
  bool ConnectChannel(const uint32_t& channelId);

  virtual bool RecvCancel(const nsresult& status) MOZ_OVERRIDE;
  virtual bool RecvSuspend() MOZ_OVERRIDE;
  virtual bool RecvResume() MOZ_OVERRIDE;
  virtual bool RecvDivertOnDataAvailable(const nsCString& data,
                                         const uint64_t& offset,
                                         const uint32_t& count) MOZ_OVERRIDE;
  virtual bool RecvDivertOnStopRequest(const nsresult& statusCode) MOZ_OVERRIDE;
  virtual bool RecvDivertComplete() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  void OfflineDisconnect() MOZ_OVERRIDE;
  uint32_t GetAppId() MOZ_OVERRIDE;

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
  nsRefPtr<OfflineObserver> mObserver;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelParent_h
