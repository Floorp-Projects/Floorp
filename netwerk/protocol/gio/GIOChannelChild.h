/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_GIOCHANNELCHILD_H
#define NS_GIOCHANNELCHILD_H

#include "mozilla/net/PGIOChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsBaseChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"
#include "nsIChildChannel.h"
#include "nsIEventTarget.h"
#include "nsIStreamListener.h"

class nsIEventTarget;

namespace mozilla {
namespace net {

class GIOChannelChild final : public PGIOChannelChild,
                              public nsBaseChannel,
                              public nsIChildChannel {
 public:
  using nsIStreamListener = ::nsIStreamListener;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICHILDCHANNEL

  NS_IMETHOD Cancel(nsresult aStatus) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;

  explicit GIOChannelChild(nsIURI* uri);

  void AddIPDLReference();
  void ReleaseIPDLReference();

  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;

  // Note that we handle this ourselves, overriding the nsBaseChannel
  // default behavior, in order to be e10s-friendly.
  NS_IMETHOD IsPending(bool* aResult) override;

  nsresult OpenContentStream(bool aAsync, nsIInputStream** aStream,
                             nsIChannel** aChannel) override;

  bool IsSuspended() const;

 protected:
  virtual ~GIOChannelChild() = default;

  mozilla::ipc::IPCResult RecvOnStartRequest(const nsresult& aChannelStatus,
                                             const int64_t& aContentLength,
                                             const nsACString& aContentType,
                                             const nsACString& aEntityID,
                                             const URIParams& aURI) override;
  mozilla::ipc::IPCResult RecvOnDataAvailable(const nsresult& aChannelStatus,
                                              const nsACString& aData,
                                              const uint64_t& aOffset,
                                              const uint32_t& aCount) override;
  mozilla::ipc::IPCResult RecvOnStopRequest(
      const nsresult& aChannelStatus) override;
  mozilla::ipc::IPCResult RecvFailedAsyncOpen(
      const nsresult& aStatusCode) override;
  mozilla::ipc::IPCResult RecvDeleteSelf() override;

  void DoOnStartRequest(const nsresult& aChannelStatus,
                        const int64_t& aContentLength,
                        const nsACString& aContentType,
                        const nsACString& aEntityID, const URIParams& aURI);
  void DoOnDataAvailable(const nsresult& aChannelStatus,
                         const nsACString& aData, const uint64_t& aOffset,
                         const uint32_t& aCount);
  void DoOnStopRequest(const nsresult& aChannelStatus);
  void DoFailedAsyncOpen(const nsresult& aStatusCode);
  void DoDeleteSelf();

  void SetupNeckoTarget() override;

  friend class NeckoTargetChannelFunctionEvent;

 private:
  nsCOMPtr<nsIInputStream> mUploadStream;

  bool mIPCOpen = false;
  const RefPtr<ChannelEventQueue> mEventQ;

  bool mCanceled = false;
  uint32_t mSuspendCount = 0;
  ;
  bool mIsPending = false;

  uint64_t mStartPos = 0;
  nsCString mEntityID;

  // Set if SendSuspend is called. Determines if SendResume is needed when
  // diverting callbacks to parent.
  bool mSuspendSent = false;
};

inline bool GIOChannelChild::IsSuspended() const { return mSuspendCount != 0; }

}  // namespace net
}  // namespace mozilla

#endif /* NS_GIOCHANNELCHILD_H */
