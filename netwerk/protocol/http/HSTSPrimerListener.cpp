/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HttpLog.h"

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
#include "nsStreamListenerWrapper.h"
#include "nsHttpChannel.h"
#include "LoadInfo.h"
#include "mozilla/Unused.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

using namespace mozilla;

NS_IMPL_ISUPPORTS(HSTSPrimingListener, nsIStreamListener,
                  nsIRequestObserver, nsIInterfaceRequestor,
                  nsITimerCallback)

// default to 3000ms, same as the preference
uint32_t HSTSPrimingListener::sHSTSPrimingTimeout = 3000;


HSTSPrimingListener::HSTSPrimingListener(nsIHstsPrimingCallback* aCallback)
  : mCallback(aCallback)
{
  static nsresult rv =
    Preferences::AddUintVarCache(&sHSTSPrimingTimeout,
        "security.mixed_content.hsts_priming_request_timeout");
  Unused << rv;
}

NS_IMETHODIMP
HSTSPrimingListener::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

void
HSTSPrimingListener::ReportTiming(nsIHstsPrimingCallback* aCallback, nsresult aResult)
{
  nsCOMPtr<nsITimedChannel> timingChannel =
    do_QueryInterface(aCallback);
  if (!timingChannel) {
    LOG(("HSTS priming: mCallback is not an nsITimedChannel!"));
    return;
  }

  TimeStamp channelCreationTime;
  nsresult rv = timingChannel->GetChannelCreation(&channelCreationTime);
  if (NS_SUCCEEDED(rv) && !channelCreationTime.IsNull()) {
    PRUint32 interval =
      (PRUint32) (TimeStamp::Now() - channelCreationTime).ToMilliseconds();
    Telemetry::Accumulate(Telemetry::HSTS_PRIMING_REQUEST_DURATION,
        (NS_SUCCEEDED(aResult)) ? NS_LITERAL_CSTRING("success")
                                : NS_LITERAL_CSTRING("failure"),
        interval);
  }
}

NS_IMETHODIMP
HSTSPrimingListener::OnStartRequest(nsIRequest *aRequest,
                                    nsISupports *aContext)
{
  nsCOMPtr<nsIHstsPrimingCallback> callback;
  callback.swap(mCallback);

  if (mHSTSPrimingTimer) {
    Unused << mHSTSPrimingTimer->Cancel();
    mHSTSPrimingTimer = nullptr;
  }

  // if callback is null, we have already canceled this request and reported
  // the failure
  if (!callback) {
    return NS_OK;
  }

  nsresult primingResult = CheckHSTSPrimingRequestStatus(aRequest);
  ReportTiming(callback, primingResult);

  if (NS_FAILED(primingResult)) {
    LOG(("HSTS Priming Failed (request was not approved)"));
    return callback->OnHSTSPrimingFailed(primingResult, false);
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
  RefPtr<nsHttpChannel> rawHttpChannel = do_QueryObject(httpChannel);
  NS_ENSURE_STATE(rawHttpChannel);

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

  OriginAttributes originAttributes;
  NS_GetOriginAttributes(httpChannel, originAttributes);

  bool hsts;
  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, 0,
                        originAttributes, nullptr, &hsts);
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

/** nsITimerCallback **/
NS_IMETHODIMP
HSTSPrimingListener::Notify(nsITimer* timer)
{
  nsresult rv;
  nsCOMPtr<nsIHstsPrimingCallback> callback;
  callback.swap(mCallback);
  if (!callback) {
    // we already processed this channel
    return NS_OK;
  }

  ReportTiming(callback, NS_ERROR_HSTS_PRIMING_TIMEOUT);

  if (mPrimingChannel) {
    rv = mPrimingChannel->Cancel(NS_ERROR_HSTS_PRIMING_TIMEOUT);
    if (NS_FAILED(rv)) {
      NS_ERROR("HSTS Priming timed out, and we got an error canceling the priming channel.");
    }
  }

  rv = callback->OnHSTSPrimingFailed(NS_ERROR_HSTS_PRIMING_TIMEOUT, false);
  if (NS_FAILED(rv)) {
    NS_ERROR("HSTS Priming timed out, and we got an error reporting the failure.");
  }

  return NS_OK; // unused
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
  rv = NS_GetSecureUpgradedURI(finalChannelURI, getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv,rv);

  // check the HSTS cache
  bool hsts;
  bool cached;
  nsCOMPtr<nsISiteSecurityService> sss = do_GetService(NS_SSSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes originAttributes;
  NS_GetOriginAttributes(aRequestChannel, originAttributes);

  rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, uri, 0,
                        originAttributes, &cached, &hsts);
  NS_ENSURE_SUCCESS(rv, rv);

  if (hsts) {
    // already saw this host and will upgrade if allowed by preferences
    Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_REQUESTS,
                          HSTSPrimingRequest::eHSTS_PRIMING_REQUEST_CACHED_HSTS);
    return aCallback->OnHSTSPrimingSucceeded(true);
  }

  if (cached) {
    // there is a non-expired entry in the cache that doesn't allow us to
    // upgrade, so go ahead and fail early.
    Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_REQUESTS,
                          HSTSPrimingRequest::eHSTS_PRIMING_REQUEST_CACHED_NO_HSTS);
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
  loadInfo->SetIsHSTSPriming(true);

  // the LoadInfo must have a security flag set in order to pass through priming
  // if none of these security flags are set, go ahead and fail now instead of
  // crashing in nsContentSecurityManager::ValidateSecurityFlags
  nsSecurityFlags securityMode = loadInfo->GetSecurityMode();
  if (securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL &&
      securityMode != nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS) {
    return aCallback->OnHSTSPrimingFailed(NS_ERROR_CONTENT_BLOCKED, true);
  }

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
  nsCOMPtr<nsIHttpChannelInternal> internal = do_QueryInterface(primingChannel);
  NS_ENSURE_STATE(internal);

  // Since this is a perfomrance critical request (blocks the page load) we
  // want to get the response ASAP.
  nsCOMPtr<nsIClassOfService> classOfService(do_QueryInterface(primingChannel));
  if (classOfService) {
    classOfService->AddClassFlags(nsIClassOfService::UrgentStart);
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
    NS_ERROR("HSTSPrimingListener: httpChannel is not an nsIClassOfService");
    return NS_ERROR_FAILURE;
  }

  uint32_t classFlags = 0;
  rv = requestClass ->GetClassFlags(&classFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = primingClass->SetClassFlags(classFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // The priming channel should have highest priority so that it completes as
  // quickly as possible, allowing the load to proceed.
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(primingChannel);
  if (p) {
    uint32_t priority = nsISupportsPriority::PRIORITY_HIGHEST;

    p->SetPriority(priority);
  }

  // Set up listener which will start the original channel
  HSTSPrimingListener* listener = new HSTSPrimingListener(aCallback);
  // Start priming
  rv = primingChannel->AsyncOpen2(listener);
  NS_ENSURE_SUCCESS(rv, rv);
  listener->mPrimingChannel.swap(primingChannel);

  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  NS_ENSURE_STATE(timer);

  rv = timer->InitWithCallback(listener,
                               sHSTSPrimingTimeout,
                               nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    NS_ERROR("HSTS Priming failed to initialize channel cancellation timer");
  }

  listener->mHSTSPrimingTimer.swap(timer);

  Telemetry::Accumulate(Telemetry::MIXED_CONTENT_HSTS_PRIMING_REQUESTS,
                        HSTSPrimingRequest::eHSTS_PRIMING_REQUEST_SENT);

  return NS_OK;
}

} // namespace net
} // namespace mozilla
