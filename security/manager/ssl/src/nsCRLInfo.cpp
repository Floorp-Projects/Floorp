/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "prmem.h"
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
extern "C" {
#include "pk11func.h"
#include "certdb.h"
#include "cert.h"
#include "secerr.h"
#include "nssb64.h"
#include "secasn1.h"
#include "secder.h"
}

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
  nsCAutoString lastFetchURL;
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
      dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
                            lastUpdate, lastUpdateLocale);
    }
  }

  if (crl->nextUpdate.len) {
    // Next update time
    sec_rv = DER_UTCTimeToTime(&nextUpdate, &(crl->nextUpdate));
    if (sec_rv == SECSuccess && dateFormatter) {
      dateFormatter->FormatPRTime(nsnull, kDateFormatShort, kTimeFormatNone,
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
