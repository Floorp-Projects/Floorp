/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset:  -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpLog.h"

#include "InterceptedChannel.h"
#include "nsInputStreamPump.h"
#include "nsIPipe.h"
#include "nsIStreamListener.h"
#include "nsHttpChannel.h"
#include "HttpChannelChild.h"
#include "nsHttpResponseHead.h"
#include "nsNetUtil.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/dom/ChannelInfo.h"
#include "nsIChannelEventSink.h"

namespace mozilla {
namespace net {

extern bool
WillRedirect(const nsHttpResponseHead * response);

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
{
}

InterceptedChannelBase::~InterceptedChannelBase()
{
}

NS_IMETHODIMP
InterceptedChannelBase::GetResponseBody(nsIOutputStream** aStream)
{
  NS_IF_ADDREF(*aStream = mResponseBody);
  return NS_OK;
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
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to resume intercepted network request");
      return;
    }

    rv = mController->ChannelIntercepted(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      rv = ResetInterception();
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to resume intercepted network request");
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

    (*mSynthesizedResponseHead)->ParseStatusLine(statusLine.get());
    return NS_OK;
}

nsresult
InterceptedChannelBase::DoSynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
    EnsureSynthesizedResponse();

    nsAutoCString header = aName + NS_LITERAL_CSTRING(": ") + aValue;
    // Overwrite any existing header.
    nsresult rv = (*mSynthesizedResponseHead)->ParseHeaderLine(header.get());
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
  mReleaseHandle = aHandle;
  return NS_OK;
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

InterceptedChannelChrome::InterceptedChannelChrome(nsHttpChannel* aChannel,
                                                   nsINetworkInterceptController* aController,
                                                   nsICacheEntry* aEntry)
: InterceptedChannelBase(aController)
, mChannel(aChannel)
, mSynthesizedCacheEntry(aEntry)
{
  nsresult rv = mChannel->GetApplyConversion(&mOldApplyConversion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mOldApplyConversion = false;
  }
}

void
InterceptedChannelChrome::NotifyController()
{
  nsCOMPtr<nsIOutputStream> out;

  // Intercepted responses should already be decoded.
  mChannel->SetApplyConversion(false);

  nsresult rv = mSynthesizedCacheEntry->OpenOutputStream(0, getter_AddRefs(mResponseBody));
  NS_ENSURE_SUCCESS_VOID(rv);

  DoNotifyController();
}

NS_IMETHODIMP
InterceptedChannelChrome::GetChannel(nsIChannel** aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelChrome::ResetInterception()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  mSynthesizedCacheEntry->AsyncDoom(nullptr);
  mSynthesizedCacheEntry = nullptr;

  mChannel->SetApplyConversion(mOldApplyConversion);

  nsCOMPtr<nsIURI> uri;
  mChannel->GetURI(getter_AddRefs(uri));

  nsresult rv = mChannel->StartRedirectChannelToURI(uri, nsIChannelEventSink::REDIRECT_INTERNAL);
  NS_ENSURE_SUCCESS(rv, rv);

  mResponseBody->Close();
  mResponseBody = nullptr;

  mReleaseHandle = nullptr;
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelChrome::SynthesizeStatus(uint16_t aStatus, const nsACString& aReason)
{
  if (!mSynthesizedCacheEntry) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeStatus(aStatus, aReason);
}

NS_IMETHODIMP
InterceptedChannelChrome::SynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
  if (!mSynthesizedCacheEntry) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeHeader(aName, aValue);
}

NS_IMETHODIMP
InterceptedChannelChrome::FinishSynthesizedResponse(const nsACString& aFinalURLSpec)
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Make sure the cache entry's output stream is always closed.  If the
  // channel was intercepted with a null-body response then its possible
  // the synthesis completed without a stream copy operation.
  mResponseBody->Close();

  mReportCollector->FlushConsoleReports(mChannel);

  EnsureSynthesizedResponse();

  // If the synthesized response is a redirect, then we want to respect
  // the encoding of whatever is loaded as a result.
  if (WillRedirect(mSynthesizedResponseHead.ref())) {
    nsresult rv = mChannel->SetApplyConversion(mOldApplyConversion);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mChannel->MarkIntercepted();

  // First we ensure the appropriate metadata is set on the synthesized cache entry
  // (i.e. the flattened response head)

  nsCOMPtr<nsISupports> securityInfo;
  nsresult rv = mChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t expirationTime = 0;
  rv = DoUpdateExpirationTime(mChannel, mSynthesizedCacheEntry,
                              mSynthesizedResponseHead.ref(),
                              expirationTime);

  rv = DoAddCacheEntryHeaders(mChannel, mSynthesizedCacheEntry,
                              mChannel->GetRequestHead(),
                              mSynthesizedResponseHead.ref(), securityInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> originalURI;
  mChannel->GetURI(getter_AddRefs(originalURI));

  nsCOMPtr<nsIURI> responseURI;
  if (!aFinalURLSpec.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(responseURI), aFinalURLSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    responseURI = originalURI;
  }

  bool equal = false;
  originalURI->Equals(responseURI, &equal);
  if (!equal) {
    rv =
        mChannel->StartRedirectChannelToURI(responseURI, nsIChannelEventSink::REDIRECT_INTERNAL);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    bool usingSSL = false;
    responseURI->SchemeIs("https", &usingSSL);

    // Then we open a real cache entry to read the synthesized response from.
    rv = mChannel->OpenCacheEntry(usingSSL);
    NS_ENSURE_SUCCESS(rv, rv);

    mSynthesizedCacheEntry = nullptr;

    if (!mChannel->AwaitingCacheCallbacks()) {
      rv = mChannel->ContinueConnect();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  mReleaseHandle = nullptr;
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelChrome::Cancel(nsresult aStatus)
{
  MOZ_ASSERT(NS_FAILED(aStatus));

  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  // we need to use AsyncAbort instead of Cancel since there's no active pump
  // to cancel which will provide OnStart/OnStopRequest to the channel.
  nsresult rv = mChannel->AsyncAbort(aStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  mReleaseHandle = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelChrome::SetChannelInfo(dom::ChannelInfo* aChannelInfo)
{
  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  return aChannelInfo->ResurrectInfoOnChannel(mChannel);
}

NS_IMETHODIMP
InterceptedChannelChrome::GetInternalContentPolicyType(nsContentPolicyType* aPolicyType)
{
  NS_ENSURE_ARG(aPolicyType);
  nsCOMPtr<nsILoadInfo> loadInfo;
  nsresult rv = mChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  *aPolicyType = loadInfo->InternalContentPolicyType();
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelChrome::GetSecureUpgradedChannelURI(nsIURI** aURI)
{
  return mChannel->GetURI(aURI);
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
  nsresult rv = NS_NewPipe(getter_AddRefs(mSynthesizedInput),
                           getter_AddRefs(mResponseBody),
                           0, UINT32_MAX, true, true);
  NS_ENSURE_SUCCESS_VOID(rv);

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
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  mResponseBody->Close();
  mResponseBody = nullptr;
  mSynthesizedInput = nullptr;

  mChannel->ResetInterception();
  mReleaseHandle = nullptr;
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::SynthesizeStatus(uint16_t aStatus, const nsACString& aReason)
{
  if (!mResponseBody) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeStatus(aStatus, aReason);
}

NS_IMETHODIMP
InterceptedChannelContent::SynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
  if (!mResponseBody) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeHeader(aName, aValue);
}

NS_IMETHODIMP
InterceptedChannelContent::FinishSynthesizedResponse(const nsACString& aFinalURLSpec)
{
  if (NS_WARN_IF(!mChannel)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Make sure the body output stream is always closed.  If the channel was
  // intercepted with a null-body response then its possible the synthesis
  // completed without a stream copy operation.
  mResponseBody->Close();

  mReportCollector->FlushConsoleReports(mChannel);

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
    mChannel->ForceIntercepted(mSynthesizedInput);
    mChannel->BeginNonIPCRedirect(responseURI, *mSynthesizedResponseHead.ptr());
  } else {
    mChannel->OverrideWithSynthesizedResponse(mSynthesizedResponseHead.ref(),
                                              mSynthesizedInput,
                                              mStreamListener);
  }

  mResponseBody = nullptr;
  mReleaseHandle = nullptr;
  mChannel = nullptr;
  mStreamListener = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::Cancel(nsresult aStatus)
{
  MOZ_ASSERT(NS_FAILED(aStatus));

  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  mReportCollector->FlushConsoleReports(mChannel);

  // we need to use AsyncAbort instead of Cancel since there's no active pump
  // to cancel which will provide OnStart/OnStopRequest to the channel.
  nsresult rv = mChannel->AsyncAbort(aStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  mReleaseHandle = nullptr;
  mChannel = nullptr;
  mStreamListener = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::SetChannelInfo(dom::ChannelInfo* aChannelInfo)
{
  if (!mChannel) {
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

  *aPolicyType = loadInfo->InternalContentPolicyType();
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
