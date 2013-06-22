/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prerror.h"
#include "prprf.h"

#include "nsCRLInfo.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSShutDown.h"

#include "nspr.h"
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"

NS_IMPL_ISUPPORTS1(nsCRLInfo, nsICRLInfo)

nsCRLInfo::nsCRLInfo()
{
  /* member initializers and constructor code */
}

nsCRLInfo::nsCRLInfo(CERTSignedCrl *signedCrl)
{
  nsNSSShutDownPreventionLock locker;
  CERTCrl *crl = &(signedCrl->crl);
  nsAutoString org;
  nsAutoString orgUnit;
  nsAutoString nameInDb;
  nsAutoString nextUpdateLocale;
  nsAutoString lastUpdateLocale;
  nsAutoCString lastFetchURL;
  PRTime lastUpdate = 0;
  PRTime nextUpdate = 0;
  SECStatus sec_rv;
  
  // Get the information we need here //
  char * o = CERT_GetOrgName(&(crl->name));
  if (o) {
    org = NS_ConvertASCIItoUTF16(o);
    PORT_Free(o);
  }

  char * ou = CERT_GetOrgUnitName(&(crl->name));
  if (ou) {
    orgUnit = NS_ConvertASCIItoUTF16(ou);
    //At present, the ou is being used as the unique key - but this
    //would change, one support for delta crls come in.
    nameInDb =  orgUnit;
    PORT_Free(ou);
  }
  
  nsCOMPtr<nsIDateTimeFormat> dateFormatter = do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID);
  
  // Last Update time
  if (crl->lastUpdate.len) {
    sec_rv = DER_UTCTimeToTime(&lastUpdate, &(crl->lastUpdate));
    if (sec_rv == SECSuccess && dateFormatter) {
      dateFormatter->FormatPRTime(nullptr, kDateFormatShort, kTimeFormatNone,
                            lastUpdate, lastUpdateLocale);
    }
  }

  if (crl->nextUpdate.len) {
    // Next update time
    sec_rv = DER_UTCTimeToTime(&nextUpdate, &(crl->nextUpdate));
    if (sec_rv == SECSuccess && dateFormatter) {
      dateFormatter->FormatPRTime(nullptr, kDateFormatShort, kTimeFormatNone,
                            nextUpdate, nextUpdateLocale);
    }
  }

  char * url = signedCrl->url;
  if(url) {
    lastFetchURL =  url;
  }

  mOrg.Assign(org.get());
  mOrgUnit.Assign(orgUnit.get());
  mLastUpdateLocale.Assign(lastUpdateLocale.get());
  mNextUpdateLocale.Assign(nextUpdateLocale.get());
  mLastUpdate = lastUpdate;
  mNextUpdate = nextUpdate;
  mNameInDb.Assign(nameInDb.get());
  mLastFetchURL = lastFetchURL;
}

nsCRLInfo::~nsCRLInfo()
{
  /* destructor code */
}

/* readonly attribute */
NS_IMETHODIMP nsCRLInfo::GetOrganization(nsAString & aOrg)
{
  aOrg = mOrg;
  return NS_OK;
}

/* readonly attribute */
NS_IMETHODIMP nsCRLInfo::GetOrganizationalUnit(nsAString & aOrgUnit)
{
  aOrgUnit = mOrgUnit;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetLastUpdateLocale(nsAString & aLastUpdateLocale)
{
  aLastUpdateLocale = mLastUpdateLocale;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetNextUpdateLocale(nsAString & aNextUpdateLocale)
{
  aNextUpdateLocale = mNextUpdateLocale;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetLastUpdate(PRTime* aLastUpdate)
{
  NS_ENSURE_ARG(aLastUpdate);
  *aLastUpdate = mLastUpdate;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetNextUpdate(PRTime* aNextUpdate)
{
  NS_ENSURE_ARG(aNextUpdate);
  *aNextUpdate = mNextUpdate;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetNameInDb(nsAString & aNameInDb)
{
  aNameInDb = mNameInDb;
  return NS_OK;
}

NS_IMETHODIMP nsCRLInfo::GetLastFetchURL(nsACString & aLastFetchURL)
{
  aLastFetchURL = mLastFetchURL;
  return NS_OK;
}
