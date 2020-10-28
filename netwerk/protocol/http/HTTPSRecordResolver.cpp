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

}  // namespace net
}  // namespace mozilla
