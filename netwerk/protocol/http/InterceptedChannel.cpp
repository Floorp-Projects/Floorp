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

namespace mozilla {
namespace net {

extern nsresult
DoAddCacheEntryHeaders(nsHttpChannel *self,
                       nsICacheEntry *entry,
                       nsHttpRequestHead *requestHead,
                       nsHttpResponseHead *responseHead,
                       nsISupports *securityInfo);

NS_IMPL_ISUPPORTS(InterceptedChannelBase, nsIInterceptedChannel)

InterceptedChannelBase::InterceptedChannelBase(nsINetworkInterceptController* aController)
: mController(aController)
{
}

InterceptedChannelBase::~InterceptedChannelBase()
{
}

void
InterceptedChannelBase::EnsureSynthesizedResponse()
{
  if (mSynthesizedResponseHead.isNothing()) {
    mSynthesizedResponseHead.emplace();
  }
}

void
InterceptedChannelBase::DoNotifyController(nsIOutputStream* aOut)
{
    nsresult rv = mController->ChannelIntercepted(this, aOut);
    NS_ENSURE_SUCCESS_VOID(rv);
}

nsresult
InterceptedChannelBase::DoSynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
    EnsureSynthesizedResponse();

    nsAutoCString header = aName + NS_LITERAL_CSTRING(": ") + aValue;
    // Overwrite any existing header.
    nsresult rv = mSynthesizedResponseHead->ParseHeaderLine(header.get());
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
}

InterceptedChannelChrome::InterceptedChannelChrome(nsHttpChannel* aChannel,
                                                   nsINetworkInterceptController* aController,
                                                   nsICacheEntry* aEntry)
: InterceptedChannelBase(aController)
, mChannel(aChannel)
, mSynthesizedCacheEntry(aEntry)
{
}

void
InterceptedChannelChrome::NotifyController()
{
  nsCOMPtr<nsIOutputStream> out;

  nsresult rv = mSynthesizedCacheEntry->OpenOutputStream(0, getter_AddRefs(out));
  NS_ENSURE_SUCCESS_VOID(rv);

  DoNotifyController(out);
}

NS_IMETHODIMP
InterceptedChannelChrome::ResetInterception()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mSynthesizedCacheEntry->AsyncDoom(nullptr);
  mSynthesizedCacheEntry = nullptr;

  nsCOMPtr<nsIURI> uri;
  mChannel->GetURI(getter_AddRefs(uri));

  nsresult rv = mChannel->StartRedirectChannelToURI(uri, nsIChannelEventSink::REDIRECT_INTERNAL);
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel = nullptr;
  return NS_OK;
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
InterceptedChannelChrome::FinishSynthesizedResponse()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureSynthesizedResponse();

  mChannel->MarkIntercepted();

  // First we ensure the appropriate metadata is set on the synthesized cache entry
  // (i.e. the flattened response head)

  nsCOMPtr<nsISupports> securityInfo;
  nsresult rv = mChannel->GetSecurityInfo(getter_AddRefs(securityInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = DoAddCacheEntryHeaders(mChannel, mSynthesizedCacheEntry,
                              mChannel->GetRequestHead(),
                              mSynthesizedResponseHead.ptr(), securityInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  mChannel->GetURI(getter_AddRefs(uri));

  bool usingSSL = false;
  uri->SchemeIs("https", &usingSSL);

  // Then we open a real cache entry to read the synthesized response from.
  rv = mChannel->OpenCacheEntry(usingSSL);
  NS_ENSURE_SUCCESS(rv, rv);

  mSynthesizedCacheEntry = nullptr;

  if (!mChannel->AwaitingCacheCallbacks()) {
    rv = mChannel->ContinueConnect();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mChannel = nullptr;
  return NS_OK;
}

InterceptedChannelContent::InterceptedChannelContent(HttpChannelChild* aChannel,
                                                     nsINetworkInterceptController* aController,
                                                     nsIStreamListener* aListener)
: InterceptedChannelBase(aController)
, mChannel(aChannel)
, mStreamListener(aListener)
{
}

void
InterceptedChannelContent::NotifyController()
{
  nsresult rv = NS_NewPipe(getter_AddRefs(mSynthesizedInput),
                           getter_AddRefs(mSynthesizedOutput),
                           0, UINT32_MAX, true, true);
  NS_ENSURE_SUCCESS_VOID(rv);

  DoNotifyController(mSynthesizedOutput);
}

NS_IMETHODIMP
InterceptedChannelContent::ResetInterception()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mSynthesizedOutput = nullptr;
  mSynthesizedInput = nullptr;

  mChannel->ResetInterception();
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedChannelContent::SynthesizeHeader(const nsACString& aName, const nsACString& aValue)
{
  if (!mSynthesizedOutput) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return DoSynthesizeHeader(aName, aValue);
}

NS_IMETHODIMP
InterceptedChannelContent::FinishSynthesizedResponse()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  EnsureSynthesizedResponse();

  nsresult rv = nsInputStreamPump::Create(getter_AddRefs(mStoragePump), mSynthesizedInput,
                                          int64_t(-1), int64_t(-1), 0, 0, true);
  if (NS_FAILED(rv)) {
    mSynthesizedInput->Close();
    return rv;
  }

  mSynthesizedOutput = nullptr;

  rv = mStoragePump->AsyncRead(mStreamListener, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel->OverrideWithSynthesizedResponse(mSynthesizedResponseHead.ptr(), mStoragePump);

  mChannel = nullptr;
  return NS_OK;
}

} // namespace net
} // namespace mozilla
