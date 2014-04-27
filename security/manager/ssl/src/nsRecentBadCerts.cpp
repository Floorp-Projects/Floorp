/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRecentBadCerts.h"

#include "pkix/pkixtypes.h"
#include "nsIX509Cert.h"
#include "nsIObserverService.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "nsSSLStatus.h"
#include "nsCOMPtr.h"
#include "nsNSSCertificate.h"
#include "nsCRT.h"
#include "nsPromiseFlatString.h"
#include "nsStringBuffer.h"
#include "nspr.h"
#include "pk11pub.h"
#include "certdb.h"
#include "sechash.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsRecentBadCerts, nsIRecentBadCerts)

nsRecentBadCerts::nsRecentBadCerts()
:monitor("nsRecentBadCerts.monitor")
,mNextStorePosition(0)
{
}

nsRecentBadCerts::~nsRecentBadCerts()
{
}

NS_IMETHODIMP
nsRecentBadCerts::GetRecentBadCert(const nsAString & aHostNameWithPort, 
                                   nsISSLStatus **aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  if (!aHostNameWithPort.Length())
    return NS_ERROR_INVALID_ARG;

  *aStatus = nullptr;
  RefPtr<nsSSLStatus> status(new nsSSLStatus());

  SECItem foundDER;
  foundDER.len = 0;
  foundDER.data = nullptr;

  bool isDomainMismatch = false;
  bool isNotValidAtThisTime = false;
  bool isUntrusted = false;

  {
    ReentrantMonitorAutoEnter lock(monitor);
    for (size_t i=0; i<const_recently_seen_list_size; ++i) {
      if (mCerts[i].mHostWithPort.Equals(aHostNameWithPort)) {
        SECStatus srv = SECITEM_CopyItem(nullptr, &foundDER, &mCerts[i].mDERCert);
        if (srv != SECSuccess)
          return NS_ERROR_OUT_OF_MEMORY;

        isDomainMismatch = mCerts[i].isDomainMismatch;
        isNotValidAtThisTime = mCerts[i].isNotValidAtThisTime;
        isUntrusted = mCerts[i].isUntrusted;
      }
    }
  }

  if (foundDER.len) {
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    mozilla::pkix::ScopedCERTCertificate nssCert(
      CERT_FindCertByDERCert(certdb, &foundDER));
    if (!nssCert) 
      nssCert = CERT_NewTempCertificate(certdb, &foundDER,
                                        nullptr, // no nickname
                                        false, // not perm
                                        true); // copy der

    SECITEM_FreeItem(&foundDER, false);

    if (!nssCert)
      return NS_ERROR_FAILURE;

    status->mServerCert = nsNSSCertificate::Create(nssCert.get());
    status->mHaveCertErrorBits = true;
    status->mIsDomainMismatch = isDomainMismatch;
    status->mIsNotValidAtThisTime = isNotValidAtThisTime;
    status->mIsUntrusted = isUntrusted;

    *aStatus = status;
    NS_IF_ADDREF(*aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRecentBadCerts::AddBadCert(const nsAString &hostWithPort, 
                                    nsISSLStatus *aStatus)
{
  NS_ENSURE_ARG(aStatus);

  nsCOMPtr<nsIX509Cert> cert;
  nsresult rv;
  rv = aStatus->GetServerCert(getter_AddRefs(cert));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDomainMismatch;
  bool isNotValidAtThisTime;
  bool isUntrusted;

  rv = aStatus->GetIsDomainMismatch(&isDomainMismatch);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStatus->GetIsNotValidAtThisTime(&isNotValidAtThisTime);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStatus->GetIsUntrusted(&isUntrusted);
  NS_ENSURE_SUCCESS(rv, rv);

  SECItem tempItem;
  rv = cert->GetRawDER(&tempItem.len, (uint8_t **)&tempItem.data);
  NS_ENSURE_SUCCESS(rv, rv);

  {
    ReentrantMonitorAutoEnter lock(monitor);
    RecentBadCert &updatedEntry = mCerts[mNextStorePosition];

    ++mNextStorePosition;
    if (mNextStorePosition == const_recently_seen_list_size)
      mNextStorePosition = 0;

    updatedEntry.Clear();
    updatedEntry.mHostWithPort = hostWithPort;
    updatedEntry.mDERCert = tempItem; // consume
    updatedEntry.isDomainMismatch = isDomainMismatch;
    updatedEntry.isNotValidAtThisTime = isNotValidAtThisTime;
    updatedEntry.isUntrusted = isUntrusted;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRecentBadCerts::ResetStoredCerts()
{
  for (size_t i = 0; i < const_recently_seen_list_size; ++i) {
    RecentBadCert &entry = mCerts[i];
    entry.Clear();
  }
  return NS_OK;
}
