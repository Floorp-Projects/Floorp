/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset:  -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"

#include "InterceptedChannel.h"
#include "nsInputStreamPump.h"
#include "nsITimedChannel.h"
#include "nsHttpChannel.h"
#include "HttpChannelChild.h"
#include "nsHttpResponseHead.h"
#include "nsNetUtil.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/dom/ChannelInfo.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

extern nsresult DoUpdateExpirationTime(nsHttpChannel* aSelf,
                                       nsICacheEntry* aCacheEntry,
                                       nsHttpResponseHead* aResponseHead,
                                       uint32_t& aExpirationTime);
extern nsresult DoAddCacheEntryHeaders(nsHttpChannel* self,
                                       nsICacheEntry* entry,
                                       nsHttpRequestHead* requestHead,
                                       nsHttpResponseHead* responseHead,
                                       nsISupports* securityInfo);

NS_IMPL_ISUPPORTS(InterceptedChannelBase, nsIInterceptedChannel)

InterceptedChannelBase::InterceptedChannelBase(
    nsINetworkInterceptController* aController)
    : mController(aController),
      mReportCollector(new ConsoleReportCollector()),
      mClosed(false),
      mSynthesizedOrReset(Invalid) {}

void InterceptedChannelBase::EnsureSynthesizedResponse() {
  if (mSynthesizedResponseHead.isNothing()) {
    mSynthesizedResponseHead.emplace(new nsHttpResponseHead());
  }
}

void InterceptedChannelBase::DoNotifyController() {
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

void InterceptedChannelBase::DoSynthesizeStatus(uint16_t aStatus,
                                                const nsACString& aReason) {
  EnsureSynthesizedResponse();

  // Always assume HTTP 1.1 for synthesized responses.
  nsAutoCString statusLine;
  statusLine.AppendLiteral("HTTP/1.1 ");
  statusLine.AppendInt(aStatus);
  statusLine.AppendLiteral(" ");
  statusLine.Append(aReason);

  (*mSynthesizedResponseHead)->ParseStatusLine(statusLine);
}

nsresult InterceptedChannelBase::DoSynthesizeHeader(const nsACString& aName,
                                                    const nsACString& aValue) {
  EnsureSynthesizedResponse();

  nsAutoCString header = aName + ": "_ns + aValue;
  // Overwrite any existing header.
  return (*mSynthesizedResponseHead)->ParseHeaderLine(header);
}

NS_IMETHODIMP
InterceptedChannelBase::GetConsoleReportCollector(
    nsIConsoleReportCollector** aCollectorOut) {
  MOZ_ASSERT(aCollectorOut);
  nsCOMPtr<nsIConsoleReportCollector> ref = mReportCollector;
  ref.forget(aCollectorOut);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelBase::SetReleaseHandle(nsISupports* aHandle) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mReleaseHandle);
  MOZ_ASSERT(aHandle);

  // We need to keep it and mChannel alive until destructor clear it up.
  mReleaseHandle = aHandle;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelBase::SaveTimeStamps() {
  MOZ_ASSERT(NS_IsMainThread());

  // If we were not able to start the fetch event for some reason (like
  // corrupted scripts), then just do nothing here.
  if (mHandleFetchEventStart.IsNull()) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> underlyingChannel;
  nsresult rv = GetChannel(getter_AddRefs(underlyingChannel));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(underlyingChannel);
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

  bool isNonSubresourceRequest =
      nsContentUtils::IsNonSubresourceRequest(channel);
  nsCString navigationOrSubresource =
      isNonSubresourceRequest ? "navigation"_ns : "subresource"_ns;

  nsAutoCString subresourceKey(EmptyCString());
  GetSubresourceTimeStampKey(channel, subresourceKey);

  // We may have null timestamps if the fetch dispatch runnable was cancelled
  // and we defaulted to resuming the request.
  if (!mFinishResponseStart.IsNull() && !mFinishResponseEnd.IsNull()) {
    MOZ_ASSERT(mSynthesizedOrReset != Invalid);

    Telemetry::HistogramID id =
        (mSynthesizedOrReset == Synthesized)
            ? Telemetry::
                  SERVICE_WORKER_FETCH_EVENT_FINISH_SYNTHESIZED_RESPONSE_MS
            : Telemetry::SERVICE_WORKER_FETCH_EVENT_CHANNEL_RESET_MS;
    Telemetry::Accumulate(
        id, navigationOrSubresource,
        static_cast<uint32_t>(
            (mFinishResponseEnd - mFinishResponseStart).ToMilliseconds()));
    if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
      Telemetry::Accumulate(
          id, subresourceKey,
          static_cast<uint32_t>(
              (mFinishResponseEnd - mFinishResponseStart).ToMilliseconds()));
    }
  }

  Telemetry::Accumulate(
      Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS,
      navigationOrSubresource,
      static_cast<uint32_t>((mHandleFetchEventStart - mDispatchFetchEventStart)
                                .ToMilliseconds()));

  if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_FETCH_EVENT_DISPATCH_MS,
                          subresourceKey,
                          static_cast<uint32_t>((mHandleFetchEventStart -
                                                 mDispatchFetchEventStart)
                                                    .ToMilliseconds()));
  }

  if (!mFinishResponseEnd.IsNull()) {
    Telemetry::Accumulate(
        Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS,
        navigationOrSubresource,
        static_cast<uint32_t>(
            (mFinishResponseEnd - mDispatchFetchEventStart).ToMilliseconds()));
    if (!isNonSubresourceRequest && !subresourceKey.IsEmpty()) {
      Telemetry::Accumulate(
          Telemetry::SERVICE_WORKER_FETCH_INTERCEPTION_DURATION_MS,
          subresourceKey,
          static_cast<uint32_t>((mFinishResponseEnd - mDispatchFetchEventStart)
                                    .ToMilliseconds()));
    }
  }

  return rv;
}

/* static */
already_AddRefed<nsIURI> InterceptedChannelBase::SecureUpgradeChannelURI(
    nsIChannel* aChannel) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> upgradedURI;
  rv = NS_GetSecureUpgradedURI(uri, getter_AddRefs(upgradedURI));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return upgradedURI.forget();
}

}  // namespace net
}  // namespace mozilla
