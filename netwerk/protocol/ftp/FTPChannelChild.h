/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_FTPChannelChild_h
#define mozilla_net_FTPChannelChild_h

#include "mozilla/UniquePtr.h"
#include "mozilla/net/PFTPChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsBaseChannel.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"
#include "nsIChildChannel.h"
#include "nsIDivertableChannel.h"
#include "nsIEventTarget.h"

#include "nsIStreamListener.h"
#include "mozilla/net/PrivateBrowsingChannel.h"

class nsIEventTarget;

namespace mozilla {

namespace net {

// This class inherits logic from nsBaseChannel that is not needed for an
// e10s child channel, but it works.  At some point we could slice up
// nsBaseChannel and have a new class that has only the common logic for
// nsFTPChannel/FTPChannelChild.

class FTPChannelChild final : public PFTPChannelChild,
                              public nsBaseChannel,
                              public nsIFTPChannel,
                              public nsIUploadChannel,
                              public nsIResumableChannel,
                              public nsIProxiedChannel,
                              public nsIChildChannel,
                              public nsIDivertableChannel {
 public:
  typedef ::nsIStreamListener nsIStreamListener;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFTPCHANNEL
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIRESUMABLECHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIDIVERTABLECHANNEL

  NS_IMETHOD Cancel(nsresult aStatus) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;

  explicit FTPChannelChild(nsIURI* aUri);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;

  // Note that we handle this ourselves, overriding the nsBaseChannel
  // default behavior, in order to be e10s-friendly.
  NS_IMETHOD IsPending(bool* aResult) override;

  nsresult OpenContentStream(bool aAsync, nsIInputStream** aStream,
                             nsIChannel** aChannel) override;

  bool IsSuspended() const;

  void FlushedForDiversion();

 protected:
  virtual ~FTPChannelChild();

  mozilla::ipc::IPCResult RecvOnStartRequest(const nsresult& aChannelStatus,
                                             const int64_t& aContentLength,
                                             const nsCString& aContentType,
                                             const PRTime& aLastModified,
                                             const nsCString& aEntityID,
                                             nsIURI* aURI) override;
  mozilla::ipc::IPCResult RecvOnDataAvailable(const nsresult& aChannelStatus,
                                              const nsCString& aData,
                                              const uint64_t& aOffset,
                                              const uint32_t& aCount) override;
  mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& aChannelStatus,
                                            const nsCString& aErrorMsg,
                                            const bool& aUseUTF8) override;
  mozilla::ipc::IPCResult RecvFailedAsyncOpen(
      const nsresult& aStatusCode) override;
  mozilla::ipc::IPCResult RecvFlushedForDiversion() override;
  mozilla::ipc::IPCResult RecvDivertMessages() override;
  mozilla::ipc::IPCResult RecvDeleteSelf() override;

  void DoOnStartRequest(const nsresult& aChannelStatus,
                        const int64_t& aContentLength,
                        const nsCString& aContentType,
                        const PRTime& aLastModified, const nsCString& aEntityID,
                        nsIURI* aURI);
  void DoOnDataAvailable(const nsresult& aChannelStatus, const nsCString& aData,
                         const uint64_t& aOffset, const uint32_t& aCount);
  void DoOnStopRequest(const nsresult& StatusCode, const nsCString& aErrorMsg,
                       bool aUseUTF8);
  void DoFailedAsyncOpen(const nsresult& aStatusCode);
  void DoDeleteSelf();

  void SetupNeckoTarget() override;

  friend class NeckoTargetChannelFunctionEvent;

 private:
  nsCOMPtr<nsIInputStream> mUploadStream;

  bool mIPCOpen;
  const RefPtr<ChannelEventQueue> mEventQ;

  // If nsUnknownDecoder is involved we queue onDataAvailable (and possibly
  // OnStopRequest) so that we can divert them if needed when the listener's
  // OnStartRequest is finally called
  nsTArray<UniquePtr<ChannelEvent>> mUnknownDecoderEventQ;
  bool mUnknownDecoderInvolved;

  bool mCanceled;
  uint32_t mSuspendCount;
  bool mIsPending;

  // This will only be true while DoOnStartRequest is in progress.
  // It is used to enforce that DivertToParent is only called during that time.
  bool mDuringOnStart = false;

  PRTime mLastModifiedTime;
  uint64_t mStartPos;
  nsCString mEntityID;

  // Once set, OnData and possibly OnStop will be diverted to the parent.
  bool mDivertingToParent;
  // Once set, no OnStart/OnData/OnStop callbacks should be received from the
  // parent channel, nor dequeued from the ChannelEventQueue.
  bool mFlushedForDiversion;
  // Set if SendSuspend is called. Determines if SendResume is needed when
  // diverting callbacks to parent.
  bool mSuspendSent;
};

inline bool FTPChannelChild::IsSuspended() const { return mSuspendCount != 0; }

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_FTPChannelChild_h
