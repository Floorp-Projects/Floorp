/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentProcessDocumentChannel.h"

#include "nsIObserverService.h"

extern mozilla::LazyLogModule gDocumentChannelLog;
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED(ParentProcessDocumentChannel, DocumentChannel,
                            nsIAsyncVerifyRedirectCallback, nsIObserver)

ParentProcessDocumentChannel::ParentProcessDocumentChannel(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    nsLoadFlags aLoadFlags, uint32_t aCacheKey, bool aUriModified,
    bool aIsXFOError)
    : DocumentChannel(aLoadState, aLoadInfo, aLoadFlags, aCacheKey,
                      aUriModified, aIsXFOError) {
  LOG(("ParentProcessDocumentChannel ctor [this=%p]", this));
}

ParentProcessDocumentChannel::~ParentProcessDocumentChannel() {
  LOG(("ParentProcessDocumentChannel dtor [this=%p]", this));
}

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
ParentProcessDocumentChannel::RedirectToRealChannel(
    nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>&&
        aStreamFilterEndpoints,
    uint32_t aRedirectFlags, uint32_t aLoadFlags) {
  LOG(("ParentProcessDocumentChannel RedirectToRealChannel [this=%p]", this));
  nsCOMPtr<nsIChannel> channel = mDocumentLoadListener->GetChannel();
  channel->SetLoadFlags(aLoadFlags);
  channel->SetNotificationCallbacks(mCallbacks);

  if (mLoadGroup) {
    channel->SetLoadGroup(mLoadGroup);
  }

  mStreamFilterEndpoints = std::move(aStreamFilterEndpoints);

  RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise> p =
      mPromise.Ensure(__func__);

  nsresult rv =
      gHttpHandler->AsyncOnChannelRedirect(this, channel, aRedirectFlags);
  if (NS_FAILED(rv)) {
    OnRedirectVerifyCallback(rv);
  }

  return p;
}

NS_IMETHODIMP
ParentProcessDocumentChannel::OnRedirectVerifyCallback(nsresult aResult) {
  LOG(
      ("ParentProcessDocumentChannel OnRedirectVerifyCallback [this=%p "
       "aResult=%d]",
       this, int(aResult)));

  MOZ_ASSERT(mCanceled || mDocumentLoadListener);

  if (NS_FAILED(aResult)) {
    Cancel(aResult);
  } else if (mCanceled && NS_SUCCEEDED(aResult)) {
    aResult = NS_BINDING_ABORTED;
  } else {
    const nsCOMPtr<nsIChannel> channel = mDocumentLoadListener->GetChannel();
    mLoadGroup->AddRequest(channel, nullptr);
    // Adding the channel to the loadgroup could have triggered a status
    // change with an observer being called destroying the docShell, resulting
    // in the PPDC to be canceled.
    if (!mCanceled) {
      mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_REDIRECTED);
      for (auto& endpoint : mStreamFilterEndpoints) {
        extensions::StreamFilterParent::Attach(channel, std::move(endpoint));
      }
      if (!mDocumentLoadListener->ResumeSuspendedChannel(mListener)) {
        // We added ourselves to the load group, but attempting
        // to resume has notified us that the channel is already
        // finished. Better remove ourselves from the loadgroup
        // again.
        nsresult status = NS_OK;
        channel->GetStatus(&status);
        mLoadGroup->RemoveRequest(channel, nullptr, status);
      }
    }
  }

  mLoadGroup = nullptr;
  mListener = nullptr;
  mCallbacks = nullptr;
  DisconnectDocumentLoadListener();

  mPromise.ResolveIfExists(aResult, __func__);

  return NS_OK;
}

NS_IMETHODIMP ParentProcessDocumentChannel::AsyncOpen(
    nsIStreamListener* aListener) {
  LOG(("ParentProcessDocumentChannel AsyncOpen [this=%p]", this));
  mDocumentLoadListener = new DocumentLoadListener(
      GetDocShell()->GetBrowsingContext()->Canonical(), this);
  LOG(("Created PPDocumentChannel with listener=%p",
       mDocumentLoadListener.get()));

  // Add observers.
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    MOZ_ALWAYS_SUCCEEDS(observerService->AddObserver(
        this, NS_HTTP_ON_MODIFY_REQUEST_TOPIC, false));
  }

  gHttpHandler->OnOpeningDocumentRequest(this);

  nsresult rv = NS_OK;
  Maybe<dom::ClientInfo> initialClientInfo = mInitialClientInfo;
  if (!mDocumentLoadListener->Open(
          mLoadState, mCacheKey, Some(mChannelId), mAsyncOpenTime, mTiming,
          std::move(initialClientInfo), GetDocShell()->GetOuterWindowID(),
          GetDocShell()
              ->GetBrowsingContext()
              ->HasValidTransientUserGestureActivation(),
          Some(mUriModified), Some(mIsXFOError), &rv)) {
    MOZ_ASSERT(NS_FAILED(rv));
    DisconnectDocumentLoadListener();
    return rv;
  }

  mListener = aListener;
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP ParentProcessDocumentChannel::Cancel(nsresult aStatus) {
  LOG(("ParentProcessDocumentChannel Cancel [this=%p]", this));
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mDocumentLoadListener->Cancel(aStatus);

  ShutdownListeners(aStatus);

  return NS_OK;
}

void ParentProcessDocumentChannel::DisconnectDocumentLoadListener() {
  if (!mDocumentLoadListener) {
    return;
  }
  mDocumentLoadListener->DocumentChannelBridgeDisconnected();
  mDocumentLoadListener = nullptr;
  RemoveObserver();
}

void ParentProcessDocumentChannel::RemoveObserver() {
  if (nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService()) {
    observerService->RemoveObserver(this, NS_HTTP_ON_MODIFY_REQUEST_TOPIC);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
ParentProcessDocumentChannel::Observe(nsISupports* aSubject, const char* aTopic,
                                      const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mRequestObserversCalled) {
    // We have already emitted the event, we don't want to emit it again.
    // We only care about forwarding the first NS_HTTP_ON_MODIFY_REQUEST_TOPIC
    // encountered.
    return NS_OK;
  }
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aSubject);
  if (!channel || mDocumentLoadListener->GetChannel() != channel) {
    // Not a channel we are interested with.
    return NS_OK;
  }
  LOG(("DocumentChannelParent Observe [this=%p aChannel=%p]", this,
       channel.get()));
  if (!nsCRT::strcmp(aTopic, NS_HTTP_ON_MODIFY_REQUEST_TOPIC)) {
    mRequestObserversCalled = true;
    gHttpHandler->OnModifyDocumentRequest(this);
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
