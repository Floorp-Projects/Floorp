/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsRecentBadCerts.h"
#include "nsIX509Cert.h"
#include "nsSSLStatus.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsNSSCertificate.h"
#include "nsCRT.h"
#include "nsPromiseFlatString.h"
#include "nsStringBuffer.h"
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "nspr.h"
#include "pk11pub.h"
#include "certdb.h"
#include "sechash.h"

#include "nsNSSCleaner.h"
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRecentBadCertsService, 
                              nsIRecentBadCertsService)

nsRecentBadCertsService::nsRecentBadCertsService()
:mNextStorePosition(0)
{
  monitor = nsAutoMonitor::NewMonitor("security.recentBadCertsMonitor");
}

nsRecentBadCertsService::~nsRecentBadCertsService()
{
  if (monitor)
    nsAutoMonitor::DestroyMonitor(monitor);
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

  PRBool isDomainMismatch = PR_FALSE;
  PRBool isNotValidAtThisTime = PR_FALSE;
  PRBool isUntrusted = PR_FALSE;

  {
    nsAutoMonitor lock(monitor);
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
                                        PR_FALSE, // not perm
                                        PR_TRUE); // copy der

    SECITEM_FreeItem(&foundDER, PR_FALSE);

    if (!nssCert)
      return NS_ERROR_FAILURE;

    status->mServerCert = nsNSSCertificate::Create(nssCert);
    CERT_DestroyCertificate(nssCert);

    status->mHaveCertErrorBits = PR_TRUE;
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

  PRBool isDomainMismatch;
  PRBool isNotValidAtThisTime;
  PRBool isUntrusted;

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
    nsAutoMonitor lock(monitor);
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
