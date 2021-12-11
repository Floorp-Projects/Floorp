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
    rv = ResetInterception(false);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to resume intercepted network request");
      CancelInterception(rv);
    }
    return;
  }

  rv = mController->ChannelIntercepted(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = ResetInterception(false);
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
