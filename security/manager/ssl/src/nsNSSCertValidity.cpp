/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Ian McGreer <mcgreer@netscape.com>
 *  Javier Delgadillo <javi@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#include "nsNSSCertValidity.h"
#include "nsNSSCertHeader.h"
#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSShutDown.h"

static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsX509CertValidity, nsIX509CertValidity)

nsX509CertValidity::nsX509CertValidity() : mTimesInitialized(PR_FALSE)
{
  /* member initializers and constructor code */
}

nsX509CertValidity::nsX509CertValidity(CERTCertificate *cert) : 
                                           mTimesInitialized(PR_FALSE)
{
  nsNSSShutDownPreventionLock locker;
  if (cert) {
    SECStatus rv = CERT_GetCertTimes(cert, &mNotBefore, &mNotAfter);
    if (rv == SECSuccess)
      mTimesInitialized = PR_TRUE;
  }
}

nsX509CertValidity::~nsX509CertValidity()
{
  /* destructor code */
}

/* readonly attribute PRTime notBefore; */
NS_IMETHODIMP nsX509CertValidity::GetNotBefore(PRTime *aNotBefore)
{
  NS_ENSURE_ARG(aNotBefore);

  nsresult rv = NS_ERROR_FAILURE;
  if (mTimesInitialized) {
    *aNotBefore = mNotBefore;
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP nsX509CertValidity::GetNotBeforeLocalTime(nsAString &aNotBeforeLocalTime)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  aNotBeforeLocalTime = date;
  return NS_OK;
}

NS_IMETHODIMP nsX509CertValidity::GetNotBeforeLocalDay(nsAString &aNotBeforeLocalDay)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatNone,
                                      &explodedTime, date);
  aNotBeforeLocalDay = date;
  return NS_OK;
}


NS_IMETHODIMP nsX509CertValidity::GetNotBeforeGMT(nsAString &aNotBeforeGMT)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  aNotBeforeGMT = date;
  return NS_OK;
}

/* readonly attribute PRTime notAfter; */
NS_IMETHODIMP nsX509CertValidity::GetNotAfter(PRTime *aNotAfter)
{
  NS_ENSURE_ARG(aNotAfter);

  nsresult rv = NS_ERROR_FAILURE;
  if (mTimesInitialized) {
    *aNotAfter = mNotAfter;
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP nsX509CertValidity::GetNotAfterLocalTime(nsAString &aNotAfterLocaltime)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  aNotAfterLocaltime = date;
  return NS_OK;
}

NS_IMETHODIMP nsX509CertValidity::GetNotAfterLocalDay(nsAString &aNotAfterLocalDay)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatNone,
                                      &explodedTime, date);
  aNotAfterLocalDay = date;
  return NS_OK;
}

NS_IMETHODIMP nsX509CertValidity::GetNotAfterGMT(nsAString &aNotAfterGMT)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIDateTimeFormat> dateFormatter =
     do_CreateInstance(kDateTimeFormatCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nsnull, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  aNotAfterGMT = date;
  return NS_OK;
}
