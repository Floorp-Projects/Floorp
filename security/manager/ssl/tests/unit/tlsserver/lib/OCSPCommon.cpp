/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPCommon.h"

#include <stdio.h>

#include "ScopedNSSTypes.h"
#include "TLSServer.h"
#include "pkixtestutil.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::test;


SECItemArray *
GetOCSPResponseForType(OCSPResponseType aORT, CERTCertificate *aCert,
                       PLArenaPool *aArena, const char *aAdditionalCertName)
{
  if (aORT == ORTNone) {
    if (gDebugLevel >= DEBUG_WARNINGS) {
      fprintf(stderr, "GetOCSPResponseForType called with type ORTNone, "
                      "which makes no sense.\n");
    }
    return nullptr;
  }

  if (aORT == ORTEmpty) {
    SECItemArray* arr = SECITEM_AllocArray(aArena, nullptr, 1);
    arr->items[0].data = nullptr;
    arr->items[0].len = 0;
    return arr;
  }

  PRTime now = PR_Now();
  PRTime oneDay = 60*60*24 * (PRTime)PR_USEC_PER_SEC;
  PRTime oldNow = now - (8 * oneDay);

  insanity::test::OCSPResponseContext context;
  context.arena = aArena;
  context.cert = CERT_DupCertificate(aCert);
  context.issuerCert = nullptr;
  context.signerCert = nullptr;
  context.responseStatus = 0;

  context.producedAt = now;
  context.thisUpdate = now;
  context.nextUpdate = now + 10 * PR_USEC_PER_SEC;
  context.includeNextUpdate = true;
  context.certIDHashAlg = SEC_OID_SHA1;
  context.certStatus = 0;
  context.revocationTime = 0;
  context.badSignature = false;
  context.responderIDType = insanity::test::OCSPResponseContext::ByKeyHash;

  if (aORT == ORTGoodOtherCert) {
    context.cert = PK11_FindCertFromNickname(aAdditionalCertName, nullptr);
    if (!context.cert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  }
  // XXX CERT_FindCertIssuer uses the old, deprecated path-building logic
  context.issuerCert = CERT_FindCertIssuer(aCert, now, certUsageSSLCA);
  if (!context.issuerCert) {
    PrintPRError("CERT_FindCertIssuer failed");
    return nullptr;
  }
  if (aORT == ORTGoodOtherCA) {
    context.signerCert = PK11_FindCertFromNickname(aAdditionalCertName,
                                                   nullptr);
    if (!context.signerCert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  }
  switch (aORT) {
    case ORTMalformed:
      context.responseStatus = 1;
      break;
    case ORTSrverr:
      context.responseStatus = 2;
      break;
    case ORTTryLater:
      context.responseStatus = 3;
      break;
    case ORTNeedsSig:
      context.responseStatus = 5;
      break;
    case ORTUnauthorized:
      context.responseStatus = 6;
      break;
    default:
      // context.responseStatus is 0 in all other cases, and it has
      // already been initialized, above.
      break;
  }
  if (aORT == ORTExpired || aORT == ORTExpiredFreshCA) {
    context.thisUpdate = oldNow;
    context.nextUpdate = oldNow + 10 * PR_USEC_PER_SEC;
  }
  if (aORT == ORTRevoked) {
    context.certStatus = 1;
  }
  if (aORT == ORTUnknown) {
    context.certStatus = 2;
  }
  if (aORT == ORTBadSignature) {
    context.badSignature = true;
  }

  if (!context.signerCert) {
    context.signerCert = CERT_DupCertificate(context.issuerCert.get());
  }

  SECItem* response = insanity::test::CreateEncodedOCSPResponse(context);
  if (!response) {
    PrintPRError("CreateEncodedOCSPResponse failed");
    return nullptr;
  }

  SECItemArray* arr = SECITEM_AllocArray(aArena, nullptr, 1);
  arr->items[0].data = response ? response->data : nullptr;
  arr->items[0].len = response ? response->len : 0;

  return arr;
}
