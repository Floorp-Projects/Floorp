/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttp.h"

#include "HSTSPrimerListener.h"
#include "nsIHstsPrimingCallback.h"
#include "nsIPrincipal.h"
#include "nsSecurityHeaderParser.h"
#include "nsISiteSecurityService.h"
#include "nsISocketProvider.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsStreamUtils.h"
#include "nsHttpChannel.h"
#include "LoadInfo.h"

namespace mozilla {
namespace net {

using namespace mozilla;

NS_IMPL_ISUPPORTS(HSTSPrimingListener, nsIStreamListener,
                  nsIRequestObserver, nsIInterfaceRequestor)

NS_IMETHODIMP
HSTSPrimingListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
HSTSPrimingListener::OnStartRequest(nsIRequest *aRequest,
                                   nsISupports *aContext)
{
  nsresult rv = CheckHSTSPrimingRequestStatus(aRequest);
  nsCOMPtr<nsIHstsPrimingCallback> callback(mCallback);
  mCallback = nullptr;

  if (NS_FAILED(rv)) {
    LOG(("HSTS Priming Failed (request was not approved)"));
    return callback->OnHSTSPrimingFailed(rv, false);
  }

  LOG(("HSTS Priming Succeeded (request was approved)"));
  return callback->OnHSTSPrimingSucceeded(false);
}

NS_IMETHODIMP
HSTSPrimingListener::OnStopRequest(nsIRequest *aRequest,
                                   nsISupports *aContext,
                                   nsresult aStatus)
{
  return NS_OK;
}

nsresult
HSTSPrimingListener::CheckHSTSPrimingRequestStatus(nsIRequest* aRequest)
{
  nsresult status;
  nsresult rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(status)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // Test that things worked on a HTTP level
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  NS_ENSURE_STATE(httpChannel);
  nsCOMPtr<nsIHttpChannelInternal> internal = do_QueryInterface(aRequest);
  NS_ENSURE_STATE(internal);

  bool succeedded;
  rv = httpChannel->GetRequestSucceeded(&succeedded);
  if (NS_FAILED(rv) || !succeedded) {
    // If the request did not return a 2XX response, don't process it
    return NS_ERROR_CONTENT_BLOCKED;
  }

  bool synthesized = false;
  nsHttpChannel* rawHttpChannel = static_cast<nsHttpChannel*>(httpChannel.get());
  rv = rawHttpChannel->GetResponseSynthesized(&synthesized);
  NS_ENSURE_SUCCESS(rv, rv);
  if (synthesized) {
    // Don't consider synthesized responses
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // check to see if the HSTS cache was updated
  nsCOMPtr<nsISiteSecurityService> sss = do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = httpChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(uri, NS_ERROR_CONTENT_BLOCKED);

  bool hsts;
  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, 0, nullptr, &hsts);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hsts) {
    // An HSTS upgrade was found
    return NS_OK;
  }

  // There is no HSTS upgrade available
  return NS_ERROR_CONTENT_BLOCKED;
}

/** nsIStreamListener methods **/

NS_IMETHODIMP
HSTSPrimingListener::OnDataAvailable(nsIRequest *aRequest,
                                     nsISupports *ctxt,
                                     nsIInputStream *inStr,
                                     uint64_t sourceOffset,
                                     uint32_t count)
{
  uint32_t totalRead;
  return inStr->ReadSegments(NS_DiscardSegment, nullptr, count, &totalRead);
}

// static
nsresult
HSTSPrimingListener::StartHSTSPriming(nsIChannel* aRequestChannel,
                                      nsIHstsPrimingCallback* aCallback)
{

  nsCOMPtr<nsIURI> finalChannelURI;
  nsresult rv = NS_GetFinalChannelURI(aRequestChannel, getter_AddRefs(finalChannelURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  rv = finalChannelURI->Clone(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = uri->SetScheme(NS_LITERAL_CSTRING("https"));
  NS_ENSURE_SUCCESS(rv, rv);

  // check the HSTS cache
  bool hsts;
  bool cached;
  nsCOMPtr<nsISiteSecurityService> sss = do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, 0, &cached, &hsts);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hsts) {
    // already saw this host and will upgrade if allowed by preferences
    return aCallback->OnHSTSPrimingSucceeded(true);
  }

  if (cached) {
    // there is a non-expired entry in the cache that doesn't allow us to
    // upgrade, so go ahead and fail early.
    return aCallback->OnHSTSPrimingFailed(NS_ERROR_CONTENT_BLOCKED, true);
  }

  // Either it wasn't cached or the cached result has expired. Build a
  // channel for the HEAD request.

  nsCOMPtr<nsILoadInfo> originalLoadInfo = aRequestChannel->GetLoadInfo();
  MOZ_ASSERT(originalLoadInfo, "can not perform HSTS priming without a loadInfo");
  if (!originalLoadInfo) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = static_cast<mozilla::LoadInfo*>
    (originalLoadInfo.get())->CloneForNewRequest();

  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = aRequestChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  nsLoadFlags loadFlags;
  rv = aRequestChannel->GetLoadFlags(&loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  loadFlags &= HttpBaseChannel::INHIBIT_CACHING |
               HttpBaseChannel::INHIBIT_PERSISTENT_CACHING |
               HttpBaseChannel::LOAD_BYPASS_CACHE |
               HttpBaseChannel::LOAD_FROM_CACHE |
               HttpBaseChannel::VALIDATE_ALWAYS;
  // Priming requests should never be intercepted by service workers and
  // are always anonymous.
  loadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER |
               nsIRequest::LOAD_ANONYMOUS;

  // Create a new channel to send the priming request
  nsCOMPtr<nsIChannel> primingChannel;
  rv = NS_NewChannelInternal(getter_AddRefs(primingChannel),
                             uri,
                             loadInfo,
                             loadGroup,
                             nullptr,   // aCallbacks are set later
                             loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set method and headers
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(primingChannel);
  if (!httpChannel) {
    NS_ERROR("HSTSPrimingListener: Failed to QI to nsIHttpChannel!");
    return NS_ERROR_FAILURE;
  }

  // Currently using HEAD per the draft, but under discussion to change to GET
  // with credentials so if the upgrade is approved the result is already cached.
  rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("HEAD"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = httpChannel->
    SetRequestHeader(NS_LITERAL_CSTRING("Upgrade-Insecure-Requests"),
                     NS_LITERAL_CSTRING("1"), false);
  NS_ENSURE_SUCCESS(rv, rv);

  // attempt to set the class of service flags on the new channel
  nsCOMPtr<nsIClassOfService> requestClass = do_QueryInterface(aRequestChannel);
  if (!requestClass) {
    NS_ERROR("HSTSPrimingListener: aRequestChannel is not an nsIClassOfService");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIClassOfService> primingClass = do_QueryInterface(httpChannel);
  if (!primingClass) {
    NS_ERROR("HSTSPrimingListener: aRequestChannel is not an nsIClassOfService");
    return NS_ERROR_FAILURE;
  }
  
  uint32_t classFlags = 0;
  rv = requestClass ->GetClassFlags(&classFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = primingClass->SetClassFlags(classFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up listener which will start the original channel
  RefPtr<HSTSPrimingListener> primingListener =
    new HSTSPrimingListener(aCallback);

  // Start priming
  rv = primingChannel->AsyncOpen2(primingListener);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

} // namespace net
} // namespace mozilla
