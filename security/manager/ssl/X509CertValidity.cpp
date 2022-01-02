/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X509CertValidity.h"

#include "mozilla/Assertions.h"
#include "mozpkix/pkixder.h"
#include "mozpkix/pkixutil.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "seccomon.h"
#include "secder.h"

NS_IMPL_ISUPPORTS(X509CertValidity, nsIX509CertValidity)

using namespace mozilla;
using namespace mozilla::pkix;

X509CertValidity::X509CertValidity(Input certDER)
    : mNotBefore(0), mNotAfter(0), mTimesInitialized(false) {
  using namespace mozilla::pkix::der;

  // We're not building a verified certificate chain, so the EndEntityOrCA
  // parameter doesn't matter.
  BackCert cert(certDER, EndEntityOrCA::MustBeEndEntity, nullptr);
  pkix::Result rv = cert.Init();
  if (rv != Success) {
    return;
  }
  // Validity ::= SEQUENCE {
  //    notBefore      Time,
  //    notAfter       Time  }
  //
  // Time ::= CHOICE {
  //    utcTime        UTCTime,
  //    generalTime    GeneralizedTime }
  //
  // NB: BackCert::GetValidity returns the value of the Validity of the
  // certificate (i.e. notBefore and notAfter, without the enclosing SEQUENCE
  // and length)
  Reader reader(cert.GetValidity());
  uint8_t expectedTag = reader.Peek(UTCTime) ? UTCTime : GENERALIZED_TIME;
  Input notBefore;
  pkix::Result result = ExpectTagAndGetValue(reader, expectedTag, notBefore);
  if (result != Success) {
    return;
  }
  SECItemType notBeforeType =
      expectedTag == UTCTime ? siUTCTime : siGeneralizedTime;
  SECItem notBeforeItem = {
      notBeforeType, const_cast<unsigned char*>(notBefore.UnsafeGetData()),
      notBefore.GetLength()};
  SECStatus srv = DER_DecodeTimeChoice(&mNotBefore, &notBeforeItem);
  if (srv != SECSuccess) {
    return;
  }
  expectedTag = reader.Peek(UTCTime) ? UTCTime : GENERALIZED_TIME;
  Input notAfter;
  result = ExpectTagAndGetValue(reader, expectedTag, notAfter);
  if (result != Success) {
    return;
  }
  SECItemType notAfterType =
      expectedTag == UTCTime ? siUTCTime : siGeneralizedTime;
  SECItem notAfterItem = {notAfterType,
                          const_cast<unsigned char*>(notAfter.UnsafeGetData()),
                          notAfter.GetLength()};
  srv = DER_DecodeTimeChoice(&mNotAfter, &notAfterItem);
  if (srv != SECSuccess) {
    return;
  }

  mTimesInitialized = true;
}

NS_IMETHODIMP
X509CertValidity::GetNotBefore(PRTime* aNotBefore) {
  NS_ENSURE_ARG(aNotBefore);

  if (!mTimesInitialized) {
    return NS_ERROR_FAILURE;
  }

  *aNotBefore = mNotBefore;
  return NS_OK;
}

nsresult X509CertValidity::FormatTime(
    const PRTime& aTimeDate, PRTimeParamFn aParamFn,
    const Maybe<intl::DateTimeFormat::Style> aTimeStyle,
    nsAString& aFormattedTimeDate) {
  if (!mTimesInitialized) return NS_ERROR_FAILURE;

  PRExplodedTime explodedTime;
  PR_ExplodeTime(const_cast<PRTime&>(aTimeDate), aParamFn, &explodedTime);
  intl::DateTimeFormat::StyleBag style;
  style.time = aTimeStyle;
  return intl::AppDateTimeFormat::Format(style, &explodedTime,
                                         aFormattedTimeDate);
}

NS_IMETHODIMP
X509CertValidity::GetNotBeforeLocalTime(nsAString& aNotBeforeLocalTime) {
  return FormatTime(mNotBefore, PR_LocalTimeParameters,
                    Some(intl::DateTimeFormat::Style::Long),
                    aNotBeforeLocalTime);
}

NS_IMETHODIMP
X509CertValidity::GetNotBeforeLocalDay(nsAString& aNotBeforeLocalDay) {
  return FormatTime(mNotBefore, PR_LocalTimeParameters, Nothing(),
                    aNotBeforeLocalDay);
}

NS_IMETHODIMP
X509CertValidity::GetNotBeforeGMT(nsAString& aNotBeforeGMT) {
  return FormatTime(mNotBefore, PR_GMTParameters,
                    Some(intl::DateTimeFormat::Style::Long), aNotBeforeGMT);
}

NS_IMETHODIMP
X509CertValidity::GetNotAfter(PRTime* aNotAfter) {
  NS_ENSURE_ARG(aNotAfter);

  if (!mTimesInitialized) {
    return NS_ERROR_FAILURE;
  }

  *aNotAfter = mNotAfter;
  return NS_OK;
}

NS_IMETHODIMP
X509CertValidity::GetNotAfterLocalTime(nsAString& aNotAfterLocaltime) {
  return FormatTime(mNotAfter, PR_LocalTimeParameters,
                    Some(intl::DateTimeFormat::Style::Long),
                    aNotAfterLocaltime);
}

NS_IMETHODIMP
X509CertValidity::GetNotAfterLocalDay(nsAString& aNotAfterLocalDay) {
  return FormatTime(mNotAfter, PR_LocalTimeParameters, Nothing(),
                    aNotAfterLocalDay);
}

NS_IMETHODIMP
X509CertValidity::GetNotAfterGMT(nsAString& aNotAfterGMT) {
  return FormatTime(mNotAfter, PR_GMTParameters,
                    Some(intl::DateTimeFormat::Style::Long), aNotAfterGMT);
}
