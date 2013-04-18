/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSUSAGEARRAYHELPER_H_
#define _NSUSAGEARRAYHELPER_H_

#include "certt.h"

#include "nsNSSComponent.h"

class nsUsageArrayHelper
{
public:
  nsUsageArrayHelper(CERTCertificate *aCert);

  nsresult GetUsagesArray(const char *suffix,
               bool localOnly,
               uint32_t outArraySize,
               uint32_t *_verified,
               uint32_t *_count,
               PRUnichar **tmpUsages);

  enum { max_returned_out_array_size = 12 };

private:
  CERTCertificate *mCert;
  nsresult m_rv;
  CERTCertDBHandle *defaultcertdb;
  nsCOMPtr<nsINSSComponent> nssComponent;

  // XXX: old, non-libpkix version of check that will be removed after the
  // switch to libpkix is final.
  void check(const char *suffix,
             SECCertificateUsage aCertUsage,
             uint32_t &aCounter,
             PRUnichar **outUsages);

#ifndef NSS_NO_LIBPKIX
  uint32_t check(uint32_t previousCheckResult,
                 const char *suffix,
                 SECCertificateUsage aCertUsage,
                 nsCERTValInParamWrapper * aValInParams,
                 uint32_t &aCounter,
                 PRUnichar **outUsages);
#endif

  void verifyFailed(uint32_t *_verified, int err);
};

#endif
