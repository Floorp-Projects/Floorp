/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertValidity_h
#define nsNSSCertValidity_h

#include "DateTimeFormat.h"
#include "ScopedNSSTypes.h"
#include "nsIX509CertValidity.h"
#include "nsNSSShutDown.h"

class nsX509CertValidity : public nsIX509CertValidity
                         , public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTVALIDITY

  explicit nsX509CertValidity(const mozilla::UniqueCERTCertificate& cert);

protected:
  virtual ~nsX509CertValidity();

  // Nothing to release.
  virtual void virtualDestroyNSSReference() override {}

private:
  nsresult FormatTime(const PRTime& aTime,
                      PRTimeParamFn aParamFn,
                      const nsTimeFormatSelector aTimeFormatSelector,
                      nsAString& aFormattedTimeDate);

  PRTime mNotBefore;
  PRTime mNotAfter;
  bool mTimesInitialized;

  nsX509CertValidity(const nsX509CertValidity& x) = delete;
  nsX509CertValidity& operator=(const nsX509CertValidity& x) = delete;
};

#endif // nsNSSCertValidity_h
