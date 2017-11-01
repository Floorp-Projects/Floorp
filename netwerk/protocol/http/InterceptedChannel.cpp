/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset:  -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"

#include "InterceptedChannel.h"
#include "nsICancelable.h"
#include "nsInputStreamPump.h"
#include "nsIPipe.h"
#include "nsIStreamListener.h"
#include "nsITimedChannel.h"
#include "nsHttpChannel.h"
#include "HttpChannelChild.h"
#include "nsHttpResponseHead.h"
#include "nsNetUtil.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/dom/ChannelInfo.h"
#include "nsIChannelEventSink.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

extern nsresult
DoUpdateExpirationTime(nsHttpChannel* aSelf,
                       nsICacheEntry* aCacheEntry,
                       nsHttpResponseHead* aResponseHead,
                       uint32_t& aExpirationTime);
extern nsresult
DoAddCacheEntryHeaders(nsHttpChannel *self,
                       nsICacheEntry *entry,
                       nsHttpRequestHead *requestHead,
                       nsHttpResponseHead *responseHead,
                       nsISupports *securityInfo);

NS_IMPL_ISUPPORTS(InterceptedChannelBase, nsIInterceptedChannel)

InterceptedChannelBase::InterceptedChannelBase(nsINetworkInterceptController* aController)
  : mController(aController)
  , mReportCollector(new ConsoleReportCollector())
  , mClosed(false)
  , mSynthesizedOrReset(Invalid)
{
}

InterceptedChannelBase::~InterceptedChannelBase()
{
}

void
InterceptedChannelBase::EnsureSynthesizedResponse()
{
  if (mSynthesizedResponseHead.isNothing()) {
    mSynthesizedResponseHead.emplace(new nsHttpResponseHead());
  }
}

void
InterceptedChannelBase::DoNotifyController()
{
    nsresult rv = NS_OK;

    if (NS_WARN_IF(!mController)) {
      rv = ResetInterception();
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to resume intercepted network request");
        CancelInterception(rv);
      }
      return;
    }

    rv = mController->ChannelIntercepted(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      rv = ResetInterception();
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to resume intercepted network request");
        CancelInterception(rv);
      }
    }
    mController = nullptr;
}

nsresult
InterceptedChannelBase::DoSynthesizeStatus(uint16_t aStatus, const nsACString& aReason)
{
    EnsureSynthesizedResponse();

    // Always assume HTTP 1.1 for synthesized responses.
    nsAutoCString statusLine;
    statusLine.AppendLiteral("HTTP/1.1 ");
    statusLine.AppendInt(aStatus);
    statusLine.AppendLiteral(" ");
    statusLine.Append(aReason);

    (*mSynthesizedResponseHead)->ParseStatusLine(statusLine);
    return NS_OK;
}

nsresult
InterceptedChannelBase::DoSynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
    EnsureSynthesizedResponse();

    nsAutoCString header = aName + NS_LITERAL_CSTRING(": ") + aValue;
    // Overwrite any existing header.
    nsresult rv = (*mSynthesizedResponseHead)->ParseHeaderLine(header);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelBase::GetConsoleReportCollector(nsIConsoleReportCollector** aCollectorOut)
{
  MOZ_ASSERT(aCollectorOut);
  nsCOMPtr<nsIConsoleReportCollector> ref = mReportCollector;
  ref.forget(aCollectorOut);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelBase::SetReleaseHandle(nsISupports* aHandle)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mReleaseHandle);
  MOZ_ASSERT(aHandle);

  // We need to keep it and mChannel alive until destructor clear it up.
  mReleaseHandle = aHandle;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelBase::SaveTimeStamps()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIChannel> underlyingChannel;
  nsresult rv = GetChannel(getter_AddRefs(underlyingChannel));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsITimedChannel> timedChannel =
    do_QueryInterface(underlyingChannel);
  MOZ_ASSERT(timedChannel);

  rv = timedChannel->SetLaunchServiceWorkerStart(mLaunchServiceWorkerStart);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = timedChannel->SetLaunchServiceWorkerEnd(mLaunchServiceWorkerEnd);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = timedChannel->SetDispatchFetchEventStart(mDispatchFetchEventStart);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = timedChannel->SetDispatchFetchEventEnd(mDispatchFetchEventEnd);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = timedChannel->SetHandleFetchEventStart(mHandleFetchEventStart);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = timedChannel->SetHandleFetchEventEnd(mHandleFetchEventEnd);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIChannel> channel;
  GetChannel(getter_AddRefs(channel));
  if (NS_WARN_IF(!channel)) {
    return NS_ERROR_FAILURE;
  }

  nsCString navigationOrSubresource = nsContentUtils::IsNonSubresourceRequest(channel) ?
    NS_LITERAL_CSTRING("navigation") : NS_LITERAL_CSTRING("subresource");

  // We may have null timestamps if the fetch dispatch runnable was cancelled
  // and we defaulted to resuming the request.
  if (!mFinishResponseStart.IsNull() && !mFinishResponseEnd.IsNull()) {
    MOZ_ASSERT(mSynthesizedOrReset != Invalid);

    Telemetry::HistogramID id = (mSynthesizedOrReset == Synthesized) ?
      Telemetry::SERVICE_WORKER_FETCH_EVENT_FINISH_SYNTHESIZED_RESPONSE_MS :
      Telemetry::SERVICE_WORKER_FETCH_EVENT_CHANNEL_RESET_MS;
    Telemetry::Accumulate(id, navigationOrSubresource,
      static_cast<uint32_t>((mFinishResponseEnd - mFinishResponseStart).ToMilliseconds()));
  }

  Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS,
    navigationOrSubresource,
    static_cast<uint32_t>((mHandleFetchEventStart - mDispatchFetchEventStart).ToMilliseconds()));

  if (!mFinishResponseEnd.IsNull()) {
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS,
      navigationOrSubresource,
      static_cast<uint32_t>((mFinishResponseEnd - mDispatchFetchEventStart).ToMilliseconds()));
  }

  return rv;
}

/* static */
already_AddRefed<nsIURI>
InterceptedChannelBase::SecureUpgradeChannelURI(nsIChannel* aChannel)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> upgradedURI;
  rv = NS_GetSecureUpgradedURI(uri, getter_AddRefs(upgradedURI));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return upgradedURI.forget();
}

InterceptedChannelContent::InterceptedChannelContent(HttpChannelChild* aChannel,
                                                     nsINetworkInterceptController* aController,
                                                     InterceptStreamListener* aListener,
                                                     bool aSecureUpgrade)
: InterceptedChannelBase(aController)
, mChannel(aChannel)
, mStreamListener(aListener)
, mSecureUpgrade(aSecureUpgrade)
{
}

void
InterceptedChannelContent::NotifyController()
{
  DoNotifyController();
}

NS_IMETHODIMP
InterceptedChannelContent::GetChannel(nsIChannel** aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::ResetInterception()
{
  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  mChannel->ResetInterception();

  mClosed = true;

  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::SynthesizeStatus(uint16_t aStatus, const nsACString& aReason)
{
  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeStatus(aStatus, aReason);
}

NS_IMETHODIMP
InterceptedChannelContent::SynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeHeader(aName, aValue);
}

NS_IMETHODIMP
InterceptedChannelContent::StartSynthesizedResponse(nsIInputStream* aBody,
                                                    nsIInterceptedBodyCallback* aBodyCallback,
                                                    const nsACString& aFinalURLSpec,
                                                    bool aResponseRedirected)
{
  if (NS_WARN_IF(mClosed)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureSynthesizedResponse();

  nsCOMPtr<nsIURI> originalURI;
  mChannel->GetURI(getter_AddRefs(originalURI));

  nsCOMPtr<nsIURI> responseURI;
  if (!aFinalURLSpec.IsEmpty()) {
    nsresult rv = NS_NewURI(getter_AddRefs(responseURI), aFinalURLSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (mSecureUpgrade) {
    nsresult rv = NS_GetSecureUpgradedURI(originalURI,
                                          getter_AddRefs(responseURI));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    responseURI = originalURI;
  }

  bool equal = false;
  originalURI->Equals(responseURI, &equal);
  if (!equal) {
    mChannel->ForceIntercepted(aBody, aBodyCallback);
    mChannel->BeginNonIPCRedirect(responseURI, *mSynthesizedResponseHead.ptr(),
                                  aResponseRedirected);
  } else {
    mChannel->OverrideWithSynthesizedResponse(mSynthesizedResponseHead.ref(),
                                              aBody, aBodyCallback,
                                              mStreamListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::FinishSynthesizedResponse()
{
  if (NS_WARN_IF(mClosed)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  mStreamListener = nullptr;
  mClosed = true;

  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::CancelInterception(nsresult aStatus)
{
  MOZ_ASSERT(NS_FAILED(aStatus));

  if (mClosed) {
    return NS_ERROR_FAILURE;
  }
  mClosed = true;

  mReportCollector->FlushConsoleReports(mChannel);

  Unused << mChannel->Cancel(aStatus);
  mStreamListener = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::SetChannelInfo(dom::ChannelInfo* aChannelInfo)
{
  if (mClosed) {
    return NS_ERROR_FAILURE;
  }

  return aChannelInfo->ResurrectInfoOnChannel(mChannel);
}

NS_IMETHODIMP
InterceptedChannelContent::GetInternalContentPolicyType(nsContentPolicyType* aPolicyType)
{
  NS_ENSURE_ARG(aPolicyType);

  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv = mChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (loadInfo) {
    *aPolicyType = loadInfo->InternalContentPolicyType();
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::GetSecureUpgradedChannelURI(nsIURI** aURI)
{
  nsCOMPtr<nsIURI> uri;
  if (mSecureUpgrade) {
    uri = SecureUpgradeChannelURI(mChannel);
  } else {
    nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (uri) {
    uri.forget(aURI);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

} // namespace net
} // namespace mozilla
