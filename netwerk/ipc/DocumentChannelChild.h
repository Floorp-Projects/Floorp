/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelChild_h
#define mozilla_net_DocumentChannelChild_h

#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/PDocumentChannelChild.h"
#include "nsBaseChannel.h"
#include "nsIChildChannel.h"
#include "nsIClassifiedChannel.h"

#define DOCUMENT_CHANNEL_CHILD_IID                   \
  {                                                  \
    0x6977bc44, 0xb1db, 0x41b7, {                    \
      0xb5, 0xc5, 0xe2, 0x13, 0x68, 0x22, 0xc9, 0x8f \
    }                                                \
  }

namespace mozilla {
namespace net {

class DocumentChannelChild final : public PDocumentChannelChild,
                                   public nsBaseChannel,
                                   public nsIClassifiedChannel {
 public:
  DocumentChannelChild(nsDocShellLoadState* aLoadState,
                       class LoadInfo* aLoadInfo,
                       const nsString* aInitiatorType, nsLoadFlags aLoadFlags,
                       uint32_t aLoadType, uint32_t aCacheKey, bool aIsActive,
                       bool aIsTopLevelDoc);

  NS_DECL_ISUPPORTS_INHERITED;
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSICLASSIFIEDCHANNEL

  NS_DECLARE_STATIC_IID_ACCESSOR(DOCUMENT_CHANNEL_CHILD_IID)

  // nsIRequest
  NS_IMETHOD Cancel(nsresult status) override;
  NS_IMETHOD Suspend() override;
  NS_IMETHOD Resume() override;
  // nsIChannel
  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;

  nsresult OpenContentStream(bool aAsync, nsIInputStream** aStream,
                             nsIChannel** aChannel) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& aStatusCode);

  mozilla::ipc::IPCResult RecvCancelForProcessSwitch();

  mozilla::ipc::IPCResult RecvDeleteSelf();

  mozilla::ipc::IPCResult RecvRedirectToRealChannel(
      const uint32_t& aRegistrarId, nsIURI* aURI, const uint32_t& aNewLoadFlags,
      const Maybe<ReplacementChannelConfigInit>& aInit,
      const Maybe<LoadInfoArgs>& aLoadInfo,
      nsTArray<DocumentChannelRedirect>&& aRedirects,
      const uint64_t& aChannelId, nsIURI* aOriginalURI,
      const uint32_t& aRedirectMode, const uint32_t& aRedirectFlags,
      const Maybe<uint32_t>& aContentDisposition,
      const Maybe<nsString>& aContentDispositionFilename,
      RedirectToRealChannelResolver&& aResolve);

  mozilla::ipc::IPCResult RecvNotifyChannelClassifierProtectionDisabled(
      const uint32_t& aAcceptedReason);
  mozilla::ipc::IPCResult RecvNotifyCookieAllowed();
  mozilla::ipc::IPCResult RecvNotifyCookieBlocked(
      const uint32_t& aRejectedReason);

  mozilla::ipc::IPCResult RecvSetClassifierMatchedInfo(
      const nsCString& aList, const nsCString& aProvider,
      const nsCString& aFullHash);
  mozilla::ipc::IPCResult RecvSetClassifierMatchedTrackingInfo(
      const nsCString& aLists, const nsCString& aFullHash);

  mozilla::ipc::IPCResult RecvConfirmRedirect(
      nsIURI* aNewUri, ConfirmRedirectResolver&& aResolve);

  void DoFailedAsyncOpen(const nsresult& aStatusCode);

  const nsTArray<DocumentChannelRedirect>& GetRedirectChain() const {
    return mRedirects;
  }

  friend class NeckoTargetChannelEvent<DocumentChannelChild>;

 private:
  void ShutdownListeners(nsresult aStatusCode);

  ~DocumentChannelChild() = default;

  RefPtr<ChannelEventQueue> mEventQueue;
  nsCOMPtr<nsIChildChannel> mRedirectChannel;
  nsTArray<DocumentChannelRedirect> mRedirects;

  // Classified channel's matched information
  nsCString mMatchedList;
  nsCString mMatchedProvider;
  nsCString mMatchedFullHash;
  nsTArray<nsCString> mMatchedTrackingLists;
  nsTArray<nsCString> mMatchedTrackingFullHashes;

  RedirectToRealChannelResolver mRedirectResolver;

  const RefPtr<nsDocShellLoadState> mLoadState;
  const Maybe<nsString> mInitiatorType;
  const uint32_t mLoadType;
  const uint32_t mCacheKey;
  const bool mIsActive;
  const bool mIsTopLevelDoc;

  bool mCanceled = false;
  uint32_t mSuspendCount = 0;
  bool mIsPending = false;
  bool mWasOpened = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DocumentChannelChild, DOCUMENT_CHANNEL_CHILD_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelChild_h
