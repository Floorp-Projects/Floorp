/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertValidity.h"

#include "cert.h"
#include "mozilla/Assertions.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS(nsX509CertValidity, nsIX509CertValidity)

nsX509CertValidity::nsX509CertValidity(const mozilla::UniqueCERTCertificate& cert)
  : mTimesInitialized(false)
{
  MOZ_ASSERT(cert);
  if (!cert) {
    return;
  }

  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  if (CERT_GetCertTimes(cert.get(), &mNotBefore, &mNotAfter) == SECSuccess) {
    mTimesInitialized = true;
  }
}

nsX509CertValidity::~nsX509CertValidity()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }

  shutdown(ShutdownCalledFrom::Object);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotBefore(PRTime* aNotBefore)
{
  NS_ENSURE_ARG(aNotBefore);

  if (!mTimesInitialized) {
    return NS_ERROR_FAILURE;
  }

  *aNotBefore = mNotBefore;
  return NS_OK;
}

nsresult
nsX509CertValidity::FormatTime(const PRTime& aTimeDate,
                               PRTimeParamFn aParamFn,
                               const nsTimeFormatSelector aTimeFormatSelector,
                               nsAString& aFormattedTimeDate)
{
  if (!mTimesInitialized)
    return NS_ERROR_FAILURE;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(const_cast<PRTime&>(aTimeDate), aParamFn, &explodedTime);
  return mozilla::DateTimeFormat::FormatPRExplodedTime(kDateFormatLong,
					                                             aTimeFormatSelector,
					                                             &explodedTime, aFormattedTimeDate);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotBeforeLocalTime(nsAString& aNotBeforeLocalTime)
{
  return FormatTime(mNotBefore, PR_LocalTimeParameters,
                    kTimeFormatSeconds, aNotBeforeLocalTime);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotBeforeLocalDay(nsAString& aNotBeforeLocalDay)
{
  return FormatTime(mNotBefore, PR_LocalTimeParameters,
                    kTimeFormatNone, aNotBeforeLocalDay);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotBeforeGMT(nsAString& aNotBeforeGMT)
{
  return FormatTime(mNotBefore, PR_GMTParameters,
                    kTimeFormatSeconds, aNotBeforeGMT);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotAfter(PRTime* aNotAfter)
{
  NS_ENSURE_ARG(aNotAfter);

  if (!mTimesInitialized) {
    return NS_ERROR_FAILURE;
  }

  *aNotAfter = mNotAfter;
  return NS_OK;
}

NS_IMETHODIMP
nsX509CertValidity::GetNotAfterLocalTime(nsAString& aNotAfterLocaltime)
{
  return FormatTime(mNotAfter, PR_LocalTimeParameters,
                    kTimeFormatSeconds, aNotAfterLocaltime);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotAfterLocalDay(nsAString& aNotAfterLocalDay)
{
  return FormatTime(mNotAfter, PR_LocalTimeParameters,
                    kTimeFormatNone, aNotAfterLocalDay);
}

NS_IMETHODIMP
nsX509CertValidity::GetNotAfterGMT(nsAString& aNotAfterGMT)
{
  return FormatTime(mNotAfter, PR_GMTParameters,
                    kTimeFormatSeconds, aNotAfterGMT);
}
