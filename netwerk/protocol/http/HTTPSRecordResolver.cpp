/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "HTTPSRecordResolver.h"
#include "nsIDNSByTypeRecord.h"
#include "nsIDNSService.h"
#include "nsHttpConnectionInfo.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(HTTPSRecordResolver, nsIDNSListener)

HTTPSRecordResolver::HTTPSRecordResolver(nsAHttpTransaction* aTransaction)
    : mTransaction(aTransaction),
      mConnInfo(aTransaction->ConnectionInfo()),
      mCaps(aTransaction->Caps()) {}

HTTPSRecordResolver::~HTTPSRecordResolver() = default;

nsresult HTTPSRecordResolver::FetchHTTPSRRInternal(
    nsIEventTarget* aTarget, nsICancelable** aDNSRequest) {
  NS_ENSURE_ARG_POINTER(aTarget);

  // Only fetch HTTPS RR for https.
  if (!mConnInfo->FirstHopSSL()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t flags = nsIDNSService::GetFlagsFromTRRMode(mConnInfo->GetTRRMode());
  if (mCaps & NS_HTTP_REFRESH_DNS) {
    flags |= nsIDNSService::RESOLVE_BYPASS_CACHE;
  }

  return dns->AsyncResolveNative(
      mConnInfo->GetOrigin(), nsIDNSService::RESOLVE_TYPE_HTTPSSVC, flags,
      nullptr, this, aTarget, mConnInfo->GetOriginAttributes(), aDNSRequest);
}

NS_IMETHODIMP HTTPSRecordResolver::OnLookupComplete(nsICancelable* aRequest,
                                                    nsIDNSRecord* aRecord,
                                                    nsresult aStatus) {
  nsCOMPtr<nsIDNSAddrRecord> addrRecord = do_QueryInterface(aRecord);
  // This will be called again when receving the result of speculatively loading
  // the addr records of the target name. In this case, just return NS_OK.
  if (addrRecord) {
    return NS_OK;
  }

  if (!mTransaction) {
    // The connecttion is not interesed in a response anymore.
    return NS_OK;
  }

  nsCOMPtr<nsIDNSHTTPSSVCRecord> record = do_QueryInterface(aRecord);
  if (!record || NS_FAILED(aStatus)) {
    return mTransaction->OnHTTPSRRAvailable(nullptr, nullptr);
  }

  nsCOMPtr<nsISVCBRecord> svcbRecord;
  if (NS_FAILED(record->GetServiceModeRecord(mCaps & NS_HTTP_DISALLOW_SPDY,
                                             mCaps & NS_HTTP_DISALLOW_HTTP3,
                                             getter_AddRefs(svcbRecord)))) {
    return mTransaction->OnHTTPSRRAvailable(record, nullptr);
  }

  return mTransaction->OnHTTPSRRAvailable(record, svcbRecord);
}

void HTTPSRecordResolver::PrefetchAddrRecord(const nsACString& aTargetName,
                                             bool aRefreshDNS) {
  MOZ_ASSERT(mTransaction);
  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns) {
    return;
  }

  uint32_t flags = nsIDNSService::GetFlagsFromTRRMode(
      mTransaction->ConnectionInfo()->GetTRRMode());
  if (aRefreshDNS) {
    flags |= nsIDNSService::RESOLVE_BYPASS_CACHE;
  }

  nsCOMPtr<nsICancelable> tmpOutstanding;

  Unused << dns->AsyncResolveNative(
      aTargetName, nsIDNSService::RESOLVE_TYPE_DEFAULT,
      flags | nsIDNSService::RESOLVE_SPECULATE, nullptr, this,
      GetCurrentEventTarget(),
      mTransaction->ConnectionInfo()->GetOriginAttributes(),
      getter_AddRefs(tmpOutstanding));
}

void HTTPSRecordResolver::Close() { mTransaction = nullptr; }

}  // namespace net
}  // namespace mozilla
