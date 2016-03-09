/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertValidity.h"
#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSShutDown.h"
#include "cert.h"

/* Implementation file */
NS_IMPL_ISUPPORTS(nsX509CertValidity, nsIX509CertValidity)

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

nsresult
nsX509CertValidity::FormatTime(const PRTime& aTimeDate,
                               PRTimeParamFn aParamFn,
                               const nsTimeFormatSelector aTimeFormatSelector,
                               nsAString& aFormattedTimeDate)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDateTimeFormat> dateFormatter = nsIDateTimeFormat::Create();
  if (!dateFormatter) {
    return NS_ERROR_FAILURE;
  }

  PRExplodedTime explodedTime;
  PR_ExplodeTime(const_cast<PRTime&>(aTimeDate), aParamFn, &explodedTime);
  return dateFormatter->FormatPRExplodedTime(nullptr, kDateFormatLong,
					     aTimeFormatSelector,
					     &explodedTime, aFormattedTimeDate);
}

NS_IMETHODIMP nsX509CertValidity::GetNotBeforeLocalTime(nsAString &aNotBeforeLocalTime)
{
  return FormatTime(mNotBefore, PR_LocalTimeParameters,
                    kTimeFormatSeconds, aNotBeforeLocalTime);
}

NS_IMETHODIMP nsX509CertValidity::GetNotBeforeLocalDay(nsAString &aNotBeforeLocalDay)
{
  return FormatTime(mNotBefore, PR_LocalTimeParameters,
                    kTimeFormatNone, aNotBeforeLocalDay);
}


NS_IMETHODIMP nsX509CertValidity::GetNotBeforeGMT(nsAString &aNotBeforeGMT)
{
  return FormatTime(mNotBefore, PR_GMTParameters,
                    kTimeFormatSeconds, aNotBeforeGMT);
}

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
  return FormatTime(mNotAfter, PR_LocalTimeParameters,
                    kTimeFormatSeconds, aNotAfterLocaltime);
}

NS_IMETHODIMP nsX509CertValidity::GetNotAfterLocalDay(nsAString &aNotAfterLocalDay)
{
  return FormatTime(mNotAfter, PR_LocalTimeParameters,
                    kTimeFormatNone, aNotAfterLocalDay);
}

NS_IMETHODIMP nsX509CertValidity::GetNotAfterGMT(nsAString &aNotAfterGMT)
{
  return FormatTime(mNotAfter, PR_GMTParameters,
                    kTimeFormatSeconds, aNotAfterGMT);
}
