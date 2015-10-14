/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=2 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterceptedJARChannel.h"
#include "nsIPipe.h"
#include "mozilla/dom/ChannelInfo.h"

using namespace mozilla::net;

NS_IMPL_ISUPPORTS(InterceptedJARChannel, nsIInterceptedChannel)

InterceptedJARChannel::InterceptedJARChannel(nsJARChannel* aChannel,
                                             nsINetworkInterceptController* aController,
                                             bool aIsNavigation)
: mController(aController)
, mChannel(aChannel)
, mIsNavigation(aIsNavigation)
{
}

NS_IMETHODIMP
InterceptedJARChannel::GetResponseBody(nsIOutputStream** aStream)
{
  NS_IF_ADDREF(*aStream = mResponseBody);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::GetIsNavigation(bool* aIsNavigation)
{
  *aIsNavigation = mIsNavigation;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::GetChannel(nsIChannel** aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::ResetInterception()
{
  if (!mChannel) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mResponseBody = nullptr;
  mSynthesizedInput = nullptr;

  mChannel->ResetInterception();
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::SynthesizeStatus(uint16_t aStatus,
                                        const nsACString& aReason)
{
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::SynthesizeHeader(const nsACString& aName,
                                        const nsACString& aValue)
{
  if (aName.LowerCaseEqualsLiteral("content-type")) {
    mContentType = aValue;
  }
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::FinishSynthesizedResponse()
{
  if (NS_WARN_IF(!mChannel)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mChannel->OverrideWithSynthesizedResponse(mSynthesizedInput, mContentType);

  mResponseBody = nullptr;
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::Cancel(nsresult aStatus)
{
  MOZ_ASSERT(NS_FAILED(aStatus));

  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = mChannel->Cancel(aStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  mResponseBody = nullptr;
  mChannel = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
InterceptedJARChannel::SetChannelInfo(mozilla::dom::ChannelInfo* aChannelInfo)
{
  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  return aChannelInfo->ResurrectInfoOnChannel(mChannel);
}

void
InterceptedJARChannel::NotifyController()
{
  nsresult rv = NS_NewPipe(getter_AddRefs(mSynthesizedInput),
                           getter_AddRefs(mResponseBody),
                           0, UINT32_MAX, true, true);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = mController->ChannelIntercepted(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = ResetInterception();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
        "Failed to resume intercepted network request");
  }
  mController = nullptr;
}
