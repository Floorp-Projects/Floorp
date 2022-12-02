/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchPreloader.h"

#include "mozilla/CORSMode.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/Document.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "nsContentPolicyUtils.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIClassOfService.h"
#include "nsIHttpChannel.h"
#include "nsITimedChannel.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsIDocShell.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(FetchPreloader, nsIStreamListener, nsIRequestObserver)

FetchPreloader::FetchPreloader()
    : FetchPreloader(nsIContentPolicy::TYPE_INTERNAL_FETCH_PRELOAD) {}

FetchPreloader::FetchPreloader(nsContentPolicyType aContentPolicyType)
    : mContentPolicyType(aContentPolicyType) {}

nsresult FetchPreloader::OpenChannel(const PreloadHashKey& aKey, nsIURI* aURI,
                                     const CORSMode aCORSMode,
                                     const dom::ReferrerPolicy& aReferrerPolicy,
                                     dom::Document* aDocument) {
  nsresult rv;
  nsCOMPtr<nsIChannel> channel;

  auto notify = MakeScopeExit([&]() {
    if (NS_FAILED(rv)) {
      // Make sure we notify any <link preload> elements when opening fails
      // because of various technical or security reasons.
      NotifyStart(channel);
      // Using the non-channel overload of this method to make it work even
      // before NotifyOpen has been called on this preload.  We are not
      // switching between channels, so it's safe to do so.
      NotifyStop(rv);
    }
  });

  nsCOMPtr<nsILoadGroup> loadGroup = aDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsPIDOMWindowOuter> window = aDocument->GetWindow();
  nsCOMPtr<nsIInterfaceRequestor> prompter;
  if (window) {
    nsIDocShell* docshell = window->GetDocShell();
    prompter = do_QueryInterface(docshell);
  }

  rv = CreateChannel(getter_AddRefs(channel), aURI, aCORSMode, aReferrerPolicy,
                     aDocument, loadGroup, prompter);
  NS_ENSURE_SUCCESS(rv, rv);

  // Doing this now so that we have the channel and tainting set on it properly
  // to notify the proper event (load or error) on the associated preload tags
  // when the CSP check fails.
  rv = CheckContentPolicy(aURI, aDocument);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PrioritizeAsPreload(channel);
  AddLoadBackgroundFlag(channel);

  NotifyOpen(aKey, channel, aDocument, true);

  return mAsyncConsumeResult = rv = channel->AsyncOpen(this);
}

nsresult FetchPreloader::CreateChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy, dom::Document* aDocument,
    nsILoadGroup* aLoadGroup, nsIInterfaceRequestor* aCallbacks) {
  nsresult rv;

  nsSecurityFlags securityFlags =
      nsContentSecurityManager::ComputeSecurityFlags(
          aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                         CORS_NONE_MAPS_TO_DISABLED_CORS_CHECKS);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aURI, aDocument, aDocument->NodePrincipal(),
      securityFlags, nsIContentPolicy::TYPE_FETCH, nullptr, aLoadGroup,
      aCallbacks);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel)) {
    nsCOMPtr<nsIReferrerInfo> referrerInfo = new dom::ReferrerInfo(
        aDocument->GetDocumentURIAsReferrer(), aReferrerPolicy);
    rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(channel)) {
    timedChannel->SetInitiatorType(u"link"_ns);
  }

  channel.forget(aChannel);
  return NS_OK;
}

nsresult FetchPreloader::CheckContentPolicy(nsIURI* aURI,
                                            dom::Document* aDocument) {
  if (!aDocument) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
      aDocument->NodePrincipal(), aDocument->NodePrincipal(), aDocument,
      nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK, mContentPolicyType);

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv =
      NS_CheckContentLoadPolicy(aURI, secCheckLoadInfo, ""_ns, &shouldLoad,
                                nsContentUtils::GetContentPolicy());
  if (NS_SUCCEEDED(rv) && NS_CP_ACCEPTED(shouldLoad)) {
    return NS_OK;
  }

  return NS_ERROR_CONTENT_BLOCKED;
}

// PreloaderBase

nsresult FetchPreloader::AsyncConsume(nsIStreamListener* aListener) {
  if (NS_FAILED(mAsyncConsumeResult)) {
    // Already being consumed or failed to open.
    return mAsyncConsumeResult;
  }

  // Prevent duplicate calls.
  mAsyncConsumeResult = NS_ERROR_NOT_AVAILABLE;

  if (!mConsumeListener) {
    // Called before we are getting response from the channel.
    mConsumeListener = aListener;
  } else {
    // Channel already started, push cached calls to this listener.
    // Can't be anything else than the `Cache`, hence a safe static_cast.
    Cache* cache = static_cast<Cache*>(mConsumeListener.get());
    cache->AsyncConsume(aListener);
  }

  return NS_OK;
}

// static
void FetchPreloader::PrioritizeAsPreload(nsIChannel* aChannel) {
  if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }
}

void FetchPreloader::PrioritizeAsPreload() { PrioritizeAsPreload(Channel()); }

// nsIRequestObserver + nsIStreamListener

NS_IMETHODIMP FetchPreloader::OnStartRequest(nsIRequest* request) {
  NotifyStart(request);

  if (!mConsumeListener) {
    // AsyncConsume not called yet.
    mConsumeListener = new Cache();
  }

  return mConsumeListener->OnStartRequest(request);
}

NS_IMETHODIMP FetchPreloader::OnDataAvailable(nsIRequest* request,
                                              nsIInputStream* input,
                                              uint64_t offset, uint32_t count) {
  return mConsumeListener->OnDataAvailable(request, input, offset, count);
}

NS_IMETHODIMP FetchPreloader::OnStopRequest(nsIRequest* request,
                                            nsresult status) {
  mConsumeListener->OnStopRequest(request, status);

  // We want 404 or other types of server responses to be treated as 'error'.
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request)) {
    uint32_t responseStatus = 0;
    Unused << httpChannel->GetResponseStatus(&responseStatus);
    if (responseStatus / 100 != 2) {
      status = NS_ERROR_FAILURE;
    }
  }

  // Fetch preloader wants to keep the channel around so that consumers like XHR
  // can access it even after the preload is done.
  nsCOMPtr<nsIChannel> channel = mChannel;
  NotifyStop(request, status);
  mChannel.swap(channel);
  return NS_OK;
}

// FetchPreloader::Cache

NS_IMPL_ISUPPORTS(FetchPreloader::Cache, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP FetchPreloader::Cache::OnStartRequest(nsIRequest* request) {
  mRequest = request;

  if (mFinalListener) {
    return mFinalListener->OnStartRequest(mRequest);
  }

  mCalls.AppendElement(Call{VariantIndex<0>{}, StartRequest{}});
  return NS_OK;
}

NS_IMETHODIMP FetchPreloader::Cache::OnDataAvailable(nsIRequest* request,
                                                     nsIInputStream* input,
                                                     uint64_t offset,
                                                     uint32_t count) {
  if (mFinalListener) {
    return mFinalListener->OnDataAvailable(mRequest, input, offset, count);
  }

  DataAvailable data;
  if (!data.mData.SetLength(count, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t read;
  nsresult rv = input->Read(data.mData.BeginWriting(), count, &read);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mCalls.AppendElement(Call{VariantIndex<1>{}, std::move(data)});
  return NS_OK;
}

NS_IMETHODIMP FetchPreloader::Cache::OnStopRequest(nsIRequest* request,
                                                   nsresult status) {
  if (mFinalListener) {
    return mFinalListener->OnStopRequest(mRequest, status);
  }

  mCalls.AppendElement(Call{VariantIndex<2>{}, StopRequest{status}});
  return NS_OK;
}

void FetchPreloader::Cache::AsyncConsume(nsIStreamListener* aListener) {
  // Must dispatch for two reasons:
  // 1. This is called directly from FetchLoader::AsyncConsume, which must
  // behave the same way as nsIChannel::AsyncOpen.
  // 2. In case there are already stream listener events scheduled on the main
  // thread we preserve the order - those will still end up in Cache.

  // * The `Cache` object is fully main thread only for now, doesn't support
  // retargeting, but it can be improved to allow it.

  nsCOMPtr<nsIStreamListener> listener(aListener);
  NS_DispatchToMainThread(NewRunnableMethod<nsCOMPtr<nsIStreamListener>>(
      "FetchPreloader::Cache::Consume", this, &FetchPreloader::Cache::Consume,
      listener));
}

void FetchPreloader::Cache::Consume(nsCOMPtr<nsIStreamListener> aListener) {
  MOZ_ASSERT(!mFinalListener, "Duplicate call");

  mFinalListener = std::move(aListener);

  // Status of the channel read after each call.
  nsresult status = NS_OK;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(mRequest));

  RefPtr<Cache> self(this);
  for (auto& call : mCalls) {
    nsresult rv = call.match(
        [&](const StartRequest& startRequest) mutable {
          return self->mFinalListener->OnStartRequest(self->mRequest);
        },
        [&](const DataAvailable& dataAvailable) mutable {
          if (NS_FAILED(status)) {
            // Channel has been cancelled during this mCalls loop.
            return NS_OK;
          }

          nsCOMPtr<nsIInputStream> input;
          rv = NS_NewCStringInputStream(getter_AddRefs(input),
                                        dataAvailable.mData);
          if (NS_FAILED(rv)) {
            return rv;
          }

          return self->mFinalListener->OnDataAvailable(
              self->mRequest, input, 0, dataAvailable.mData.Length());
        },
        [&](const StopRequest& stopRequest) {
          // First cancellation overrides mStatus in nsHttpChannel.
          nsresult stopStatus =
              NS_FAILED(status) ? status : stopRequest.mStatus;
          self->mFinalListener->OnStopRequest(self->mRequest, stopStatus);
          self->mFinalListener = nullptr;
          self->mRequest = nullptr;
          return NS_OK;
        });

    if (!mRequest) {
      // We are done!
      break;
    }

    bool isCancelled = false;
    Unused << channel->GetCanceled(&isCancelled);
    if (isCancelled) {
      mRequest->GetStatus(&status);
    } else if (NS_FAILED(rv)) {
      status = rv;
      mRequest->Cancel(status);
    }
  }

  mCalls.Clear();
}

}  // namespace mozilla
