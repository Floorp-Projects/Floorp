/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_DocumentChannelChild_h
#define mozilla_net_DocumentChannelChild_h

#include "mozilla/net/PDocumentChannelChild.h"
#include "mozilla/net/DocumentChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/dom/nsCSPContext.h"

namespace mozilla {
namespace net {

/**
 * DocumentChannelChild is an implementation of DocumentChannel for nsDocShells
 * in the content process, that uses PDocumentChannel to serialize everything
 * across IPDL to the parent process.
 */
class DocumentChannelChild final : public DocumentChannel,
                                   public nsIAsyncVerifyRedirectCallback,
                                   public PDocumentChannelChild {
 public:
  DocumentChannelChild(nsDocShellLoadState* aLoadState,
                       class LoadInfo* aLoadInfo, nsLoadFlags aLoadFlags,
                       uint32_t aCacheKey, bool aUriModified, bool aIsXFOError);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

  NS_IMETHOD AsyncOpen(nsIStreamListener* aListener) override;
  NS_IMETHOD Cancel(nsresult aStatusCode) override;

  mozilla::ipc::IPCResult RecvFailedAsyncOpen(const nsresult& aStatusCode);

  mozilla::ipc::IPCResult RecvDisconnectChildListeners(
      const nsresult& aStatus, const nsresult& aLoadGroupStatus,
      bool aSwitchedProcess);

  mozilla::ipc::IPCResult RecvDeleteSelf();

  mozilla::ipc::IPCResult RecvRedirectToRealChannel(
      RedirectToRealChannelArgs&& aArgs,
      nsTArray<Endpoint<extensions::PStreamFilterParent>>&& aEndpoints,
      RedirectToRealChannelResolver&& aResolve);

  mozilla::ipc::IPCResult RecvUpgradeObjectLoad(
      UpgradeObjectLoadResolver&& aResolve);

 private:
  void DeleteIPDL() override {
    if (CanSend()) {
      Send__delete__(this);
    }
  }

  ~DocumentChannelChild();

  nsCOMPtr<nsIChannel> mRedirectChannel;

  RedirectToRealChannelResolver mRedirectResolver;
  nsTArray<Endpoint<extensions::PStreamFilterParent>> mStreamFilterEndpoints;
  BrowsingContext* mLoadingContext;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_DocumentChannelChild_h
