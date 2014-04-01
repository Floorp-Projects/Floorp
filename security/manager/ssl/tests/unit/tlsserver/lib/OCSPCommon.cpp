/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPCommon.h"

#include <stdio.h>

#include "ScopedNSSTypes.h"
#include "TLSServer.h"
#include "pkixtestutil.h"
#include "secder.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::test;
using namespace mozilla::pkix::test;


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

  OCSPResponseContext context(aArena, aCert, now);

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
      // already been initialized in the constructor.
      break;
  }
  if (aORT == ORTSkipResponseBytes) {
    context.skipResponseBytes = true;
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
  OCSPResponseExtension extension;
  if (aORT == ORTCriticalExtension || aORT == ORTNoncriticalExtension) {
    SECItem oidItem = {
      siBuffer,
      nullptr,
      0
    };
    // 1.3.6.1.4.1.13769.666.666.666 is the root of Mozilla's testing OID space
    static const char* testExtensionOID = "1.3.6.1.4.1.13769.666.666.666.1.500.9.2";
    if (SEC_StringToOID(aArena, &oidItem, testExtensionOID,
                        PL_strlen(testExtensionOID)) != SECSuccess) {
      return nullptr;
    }
    DERTemplate oidTemplate[2] = { { DER_OBJECT_ID, 0 }, { 0 } };
    extension.id.data = nullptr;
    extension.id.len = 0;
    if (DER_Encode(aArena, &extension.id, oidTemplate, &oidItem)
          != SECSuccess) {
      return nullptr;
    }
    extension.critical = (aORT == ORTCriticalExtension);
    static const uint8_t value[2] = { 0x05, 0x00 };
    extension.value.data = const_cast<uint8_t*>(value);
    extension.value.len = PR_ARRAY_SIZE(value);
    extension.next = nullptr;
    context.extensions = &extension;
  }

  if (!context.signerCert) {
    context.signerCert = CERT_DupCertificate(context.issuerCert.get());
  }

  SECItem* response = CreateEncodedOCSPResponse(context);
  if (!response) {
    PrintPRError("CreateEncodedOCSPResponse failed");
    return nullptr;
  }

  SECItemArray* arr = SECITEM_AllocArray(aArena, nullptr, 1);
  arr->items[0].data = response ? response->data : nullptr;
  arr->items[0].len = response ? response->len : 0;

  return arr;
}
