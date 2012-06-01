/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRecentBadCerts.h"
#include "nsIX509Cert.h"
#include "nsSSLStatus.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsNSSCertificate.h"
#include "nsCRT.h"
#include "nsPromiseFlatString.h"
#include "nsStringBuffer.h"
#include "nsAutoPtr.h"
#include "nspr.h"
#include "pk11pub.h"
#include "certdb.h"
#include "sechash.h"

#include "nsNSSCleaner.h"

using namespace mozilla;

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRecentBadCertsService, 
                              nsIRecentBadCertsService)

nsRecentBadCertsService::nsRecentBadCertsService()
:monitor("nsRecentBadCertsService.monitor")
,mNextStorePosition(0)
{
}

nsRecentBadCertsService::~nsRecentBadCertsService()
{
}

nsresult
nsRecentBadCertsService::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsRecentBadCertsService::GetRecentBadCert(const nsAString & aHostNameWithPort, 
                                          nsISSLStatus **aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  if (!aHostNameWithPort.Length())
    return NS_ERROR_INVALID_ARG;

  *aStatus = nsnull;
  nsRefPtr<nsSSLStatus> status = new nsSSLStatus();
  if (!status)
    return NS_ERROR_OUT_OF_MEMORY;

  SECItem foundDER;
  foundDER.len = 0;
  foundDER.data = nsnull;

  bool isDomainMismatch = false;
  bool isNotValidAtThisTime = false;
  bool isUntrusted = false;

  {
    ReentrantMonitorAutoEnter lock(monitor);
    for (size_t i=0; i<const_recently_seen_list_size; ++i) {
      if (mCerts[i].mHostWithPort.Equals(aHostNameWithPort)) {
        SECStatus srv = SECITEM_CopyItem(nsnull, &foundDER, &mCerts[i].mDERCert);
        if (srv != SECSuccess)
          return NS_ERROR_OUT_OF_MEMORY;

        isDomainMismatch = mCerts[i].isDomainMismatch;
        isNotValidAtThisTime = mCerts[i].isNotValidAtThisTime;
        isUntrusted = mCerts[i].isUntrusted;
      }
    }
  }

  if (foundDER.len) {
    CERTCertificate *nssCert;
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    nssCert = CERT_FindCertByDERCert(certdb, &foundDER);
    if (!nssCert) 
      nssCert = CERT_NewTempCertificate(certdb, &foundDER,
                                        nsnull, // no nickname
                                        false, // not perm
                                        true); // copy der

    SECITEM_FreeItem(&foundDER, false);

    if (!nssCert)
      return NS_ERROR_FAILURE;

    status->mServerCert = nsNSSCertificate::Create(nssCert);
    CERT_DestroyCertificate(nssCert);

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
nsRecentBadCertsService::AddBadCert(const nsAString &hostWithPort, 
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
  rv = cert->GetRawDER(&tempItem.len, (PRUint8 **)&tempItem.data);
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
