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
  ScopedCERTCertificate issuerCert(CERT_FindCertIssuer(aCert, now,
                                                       certUsageSSLCA));
  if (!issuerCert) {
    PrintPRError("CERT_FindCertIssuer failed");
    return nullptr;
  }
  context.issuerNameDER = &issuerCert->derSubject;
  context.issuerSPKI = &issuerCert->subjectPublicKeyInfo;
  ScopedCERTCertificate signerCert;
  if (aORT == ORTGoodOtherCA || aORT == ORTDelegatedIncluded ||
      aORT == ORTDelegatedIncludedLast || aORT == ORTDelegatedMissing ||
      aORT == ORTDelegatedMissingMultiple) {
    signerCert = PK11_FindCertFromNickname(aAdditionalCertName, nullptr);
    if (!signerCert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  }

  const SECItem* certs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };

  if (aORT == ORTDelegatedIncluded) {
    certs[0] = &signerCert->derCert;
    context.certs = certs;
  }
  if (aORT == ORTDelegatedIncludedLast || aORT == ORTDelegatedMissingMultiple) {
    certs[0] = &issuerCert->derCert;
    certs[1] = &context.cert->derCert;
    certs[2] = &issuerCert->derCert;
    if (aORT != ORTDelegatedMissingMultiple) {
      certs[3] = &signerCert->derCert;
    }
    context.certs = certs;
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
  if (aORT == ORTExpired || aORT == ORTExpiredFreshCA ||
      aORT == ORTRevokedOld || aORT == ORTUnknownOld) {
    context.thisUpdate = oldNow;
    context.nextUpdate = oldNow + 10 * PR_USEC_PER_SEC;
  }
  if (aORT == ORTLongValidityAlmostExpired) {
    context.thisUpdate = now - (320 * oneDay);
  }
  if (aORT == ORTAncientAlmostExpired) {
    context.thisUpdate = now - (640 * oneDay);
  }
  if (aORT == ORTRevoked || aORT == ORTRevokedOld) {
    context.certStatus = 1;
  }
  if (aORT == ORTUnknown || aORT == ORTUnknownOld) {
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
  if (aORT == ORTEmptyExtensions) {
    context.includeEmptyExtensions = true;
  }

  if (!signerCert) {
    signerCert = CERT_DupCertificate(issuerCert.get());
  }
  context.signerPrivateKey = PK11_FindKeyByAnyCert(signerCert.get(), nullptr);
  if (!context.signerPrivateKey) {
    PrintPRError("PK11_FindKeyByAnyCert failed");
    return nullptr;
  }

  SECItem* response = CreateEncodedOCSPResponse(context);
  if (!response) {
    PrintPRError("CreateEncodedOCSPResponse failed");
    return nullptr;
  }

  SECItemArray* arr = SECITEM_AllocArray(aArena, nullptr, 1);
  if (!arr) {
    PrintPRError("SECITEM_AllocArray failed");
    return nullptr;
  }
  arr->items[0].data = response->data;
  arr->items[0].len = response->len;

  return arr;
}
