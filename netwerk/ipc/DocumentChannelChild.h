/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelChild_h
#define mozilla_net_DocumentChannelChild_h

#include "mozilla/net/PDocumentChannelChild.h"
#include "nsHashPropertyBag.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIChannel.h"
#include "nsIChildChannel.h"
#include "nsITraceableChannel.h"

#define DOCUMENT_CHANNEL_CHILD_IID                   \
  {                                                  \
    0x6977bc44, 0xb1db, 0x41b7, {                    \
      0xb5, 0xc5, 0xe2, 0x13, 0x68, 0x22, 0xc9, 0x8f \
    }                                                \
  }

namespace mozilla {
namespace net {

class DocumentChannelChild final : public nsHashPropertyBag,
                                   public PDocumentChannelChild,
                                   public nsIIdentChannel,
                                   public nsIAsyncVerifyRedirectCallback,
                                   public nsITraceableChannel {
 public:
  DocumentChannelChild(nsDocShellLoadState* aLoadState,
                       class LoadInfo* aLoadInfo,
                       const nsString* aInitiatorType, nsLoadFlags aLoadFlags,
                       uint32_t aLoadType, uint32_t aCacheKey, bool aIsActive,
                       bool aIsTopLevelDoc, bool aHasNonEmptySandboxingFlags);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIIDENTCHANNEL
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSITRACEABLECHANNEL

  NS_DECLARE_STATIC_IID_ACCESSOR(DOCUMENT_CHANNEL_CHILD_IID)

  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& aStatusCode);

  mozilla::ipc::IPCResult RecvDisconnectChildListeners(const nsresult& aStatus);

  mozilla::ipc::IPCResult RecvDeleteSelf();

  mozilla::ipc::IPCResult RecvRedirectToRealChannel(
      const RedirectToRealChannelArgs& aArgs,
      RedirectToRealChannelResolver&& aResolve);

  mozilla::ipc::IPCResult RecvAttachStreamFilter(
      Endpoint<extensions::PStreamFilterParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvConfirmRedirect(
      const LoadInfoArgs& aLoadInfo, nsIURI* aNewUri,
      ConfirmRedirectResolver&& aResolve);

  const nsTArray<DocumentChannelRedirect>& GetRedirectChain() const {
    return mRedirects;
  }

 private:
  void ShutdownListeners(nsresult aStatusCode);

  ~DocumentChannelChild() = default;

  nsCOMPtr<nsIChannel> mRedirectChannel;
  nsTArray<DocumentChannelRedirect> mRedirects;

  RedirectToRealChannelResolver mRedirectResolver;

  const TimeStamp mAsyncOpenTime;
  const RefPtr<nsDocShellLoadState> mLoadState;
  const Maybe<nsString> mInitiatorType;
  const uint32_t mLoadType;
  const uint32_t mCacheKey;
  const bool mIsActive;
  const bool mIsTopLevelDoc;
  const bool mHasNonEmptySandboxingFlags;

  nsresult mStatus = NS_OK;
  bool mCanceled = false;
  bool mIsPending = false;
  bool mWasOpened = false;
  uint64_t mChannelId;
  uint32_t mLoadFlags = LOAD_NORMAL;
  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsISupports> mOwner;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DocumentChannelChild, DOCUMENT_CHANNEL_CHILD_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelChild_h
