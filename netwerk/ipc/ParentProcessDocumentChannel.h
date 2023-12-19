/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ParentProcessDocumentChannel_h
#define mozilla_net_ParentProcessDocumentChannel_h

#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/net/DocumentChannel.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "nsIObserver.h"
#include "nsIAsyncVerifyRedirectCallback.h"

namespace mozilla {
namespace net {

class EarlyHintConnectArgs;

class ParentProcessDocumentChannel : public DocumentChannel,
                                     public nsIAsyncVerifyRedirectCallback,
                                     public nsIObserver {
 public:
  ParentProcessDocumentChannel(nsDocShellLoadState* aLoadState,
                               class LoadInfo* aLoadInfo,
                               nsLoadFlags aLoadFlags, uint32_t aCacheKey,
                               bool aUriModified,
                               bool aIsEmbeddingBlockedError);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK
  NS_DECL_NSIOBSERVER

  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;
  NS_IMETHOD Cancel(nsresult aStatusCode) override;
  NS_IMETHOD CancelWithReason(nsresult aStatusCode,
                              const nsACString& aReason) override;

  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
  RedirectToRealChannel(
      nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>&&
          aStreamFilterEndpoints,
      uint32_t aRedirectFlags, uint32_t aLoadFlags,
      const nsTArray<EarlyHintConnectArgs>& aEarlyHints);

 private:
  virtual ~ParentProcessDocumentChannel();
  void RemoveObserver();

  RefPtr<DocumentLoadListener> mDocumentLoadListener;
  nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>
      mStreamFilterEndpoints;
  MozPromiseHolder<PDocumentChannelParent::RedirectToRealChannelPromise>
      mPromise;
  bool mRequestObserversCalled = false;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ParentProcessDocumentChannel_h
