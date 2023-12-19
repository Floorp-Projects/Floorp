/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannel_h
#define mozilla_net_DocumentChannel_h

#include "mozilla/dom/ClientInfo.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsDOMNavigationTiming.h"
#include "nsIChannel.h"
#include "nsIChildChannel.h"

class nsDocShell;

#define DOCUMENT_CHANNEL_IID                         \
  {                                                  \
    0x6977bc44, 0xb1db, 0x41b7, {                    \
      0xb5, 0xc5, 0xe2, 0x13, 0x68, 0x22, 0xc9, 0x8f \
    }                                                \
  }

namespace mozilla {
namespace net {

uint64_t InnerWindowIDForExtantDoc(nsDocShell* docShell);

/**
 * DocumentChannel is a protocol agnostic placeholder nsIChannel implementation
 * that we use so that nsDocShell knows about a connecting load. It transfers
 * all data into a DocumentLoadListener (running in the parent process), which
 * will create the real channel for the connection, and decide which process to
 * load the resulting document in. If the document is to be loaded in the
 * current process, then we'll synthesize a redirect replacing this placeholder
 * channel with the real one, otherwise the originating docshell will be removed
 * during the process switch.
 */
class DocumentChannel : public nsIIdentChannel {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIIDENTCHANNEL

  NS_DECLARE_STATIC_IID_ACCESSOR(DOCUMENT_CHANNEL_IID)

  void SetNavigationTiming(nsDOMNavigationTiming* aTiming) {
    mTiming = aTiming;
  }

  void SetInitialClientInfo(const Maybe<dom::ClientInfo>& aInfo) {
    mInitialClientInfo = aInfo;
  }

  void DisconnectChildListeners(const nsresult& aStatus,
                                const nsresult& aLoadGroupStatus);

  /**
   * Will create the appropriate document channel:
   * Either a DocumentChannelChild if called from the content process or
   * a ParentProcessDocumentChannel if called from the parent process.
   * This operation is infallible.
   */
  static already_AddRefed<DocumentChannel> CreateForDocument(
      nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
      nsLoadFlags aLoadFlags, nsIInterfaceRequestor* aNotificationCallbacks,
      uint32_t aCacheKey, bool aUriModified, bool aIsEmbeddingBlockedError);
  static already_AddRefed<DocumentChannel> CreateForObject(
      nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
      nsLoadFlags aLoadFlags, nsIInterfaceRequestor* aNotificationCallbacks);

  static bool CanUseDocumentChannel(nsIURI* aURI);

 protected:
  DocumentChannel(nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
                  nsLoadFlags aLoadFlags, uint32_t aCacheKey, bool aUriModified,
                  bool aIsEmbeddingBlockedError);

  void ShutdownListeners(nsresult aStatusCode);
  virtual void DeleteIPDL() {}

  nsDocShell* GetDocShell();

  virtual ~DocumentChannel() = default;

  const RefPtr<nsDocShellLoadState> mLoadState;
  const uint32_t mCacheKey;

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
  RefPtr<nsDOMNavigationTiming> mTiming;
  Maybe<dom::ClientInfo> mInitialClientInfo;
  // mUriModified is true if we're doing a history load and the URI of the
  // session history had been modified by pushState/replaceState.
  bool mUriModified = false;
  // mIsEmbeddingBlockedError is true if we're handling a load error and the
  // status of the failed channel is NS_ERROR_XFO_VIOLATION or
  // NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION.
  bool mIsEmbeddingBlockedError = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DocumentChannel, DOCUMENT_CHANNEL_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannel_h
