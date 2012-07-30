/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertValidity.h"
#include "nsNSSCertHeader.h"
#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSShutDown.h"

/* Implementation file */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsX509CertValidity, nsIX509CertValidity)

nsX509CertValidity::nsX509CertValidity() : mTimesInitialized(false)
{
  /* member initializers and constructor code */
}

nsX509CertValidity::nsX509CertValidity(CERTCertificate *cert) : 
                                           mTimesInitialized(false)
{
  nsNSSShutDownPreventionLock locker;
  if (cert) {
    SECStatus rv = CERT_GetCertTimes(cert, &mNotBefore, &mNotAfter);
    if (rv == SECSuccess)
      mTimesInitialized = true;
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatSecondsForce24Hour,
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatNone,
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotBefore, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatSecondsForce24Hour,
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatSecondsForce24Hour,
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_LocalTimeParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatNone,
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
     do_CreateInstance(NS_DATETIMEFORMAT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString date;
  PRExplodedTime explodedTime;
  PR_ExplodeTime(mNotAfter, PR_GMTParameters, &explodedTime);
  dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatShort, kTimeFormatSecondsForce24Hour,
                                      &explodedTime, date);
  aNotAfterGMT = date;
  return NS_OK;
}
