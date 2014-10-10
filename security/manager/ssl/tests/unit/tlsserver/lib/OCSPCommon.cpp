/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPCommon.h"

#include <stdio.h>

#include "pkixtestutil.h"
#include "ScopedNSSTypes.h"
#include "TLSServer.h"
#include "secder.h"
#include "secerr.h"

namespace mozilla { namespace pkix { namespace test {

// Ownership of privateKey is transfered.
TestKeyPair* CreateTestKeyPair(const ByteString& spki,
                               const ByteString& spk,
                               SECKEYPrivateKey* privateKey);

} } } // namespace mozilla::pkix::test

using namespace mozilla;
using namespace mozilla::pkix;
using namespace mozilla::pkix::test;
using namespace mozilla::test;

static TestKeyPair*
CreateTestKeyPairFromCert(CERTCertificate& cert)
{
  ScopedSECKEYPrivateKey privateKey(PK11_FindKeyByAnyCert(&cert, nullptr));
  if (!privateKey) {
    return nullptr;
  }
  ByteString subjectPublicKeyInfo(cert.derPublicKey.data,
                                  cert.derPublicKey.len);
  SECItem subjectPublicKeyItem = cert.subjectPublicKeyInfo.subjectPublicKey;
  DER_ConvertBitString(&subjectPublicKeyItem); // bits to bytes
  ByteString subjectPublicKey(subjectPublicKeyItem.data,
                              subjectPublicKeyItem.len);
  return CreateTestKeyPair(subjectPublicKeyInfo, subjectPublicKey,
                           privateKey.forget());
}

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

  time_t now = time(nullptr);
  time_t oldNow = now - (8 * Time::ONE_DAY_IN_SECONDS);

  mozilla::ScopedCERTCertificate cert(CERT_DupCertificate(aCert));

  if (aORT == ORTGoodOtherCert) {
    cert = PK11_FindCertFromNickname(aAdditionalCertName, nullptr);
    if (!cert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  }
  // XXX CERT_FindCertIssuer uses the old, deprecated path-building logic
  mozilla::ScopedCERTCertificate
    issuerCert(CERT_FindCertIssuer(aCert, PR_Now(), certUsageSSLCA));
  if (!issuerCert) {
    PrintPRError("CERT_FindCertIssuer failed");
    return nullptr;
  }
  Input issuer;
  if (issuer.Init(cert->derIssuer.data, cert->derIssuer.len) != Success) {
    return nullptr;
  }
  Input issuerPublicKey;
  if (issuerPublicKey.Init(issuerCert->derPublicKey.data,
                           issuerCert->derPublicKey.len) != Success) {
    return nullptr;
  }
  Input serialNumber;
  if (serialNumber.Init(cert->serialNumber.data,
                        cert->serialNumber.len) != Success) {
    return nullptr;
  }
  CertID certID(issuer, issuerPublicKey, serialNumber);
  OCSPResponseContext context(certID, now);

  mozilla::ScopedCERTCertificate signerCert;
  if (aORT == ORTGoodOtherCA || aORT == ORTDelegatedIncluded ||
      aORT == ORTDelegatedIncludedLast || aORT == ORTDelegatedMissing ||
      aORT == ORTDelegatedMissingMultiple) {
    signerCert = PK11_FindCertFromNickname(aAdditionalCertName, nullptr);
    if (!signerCert) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  }

  ByteString certs[5];

  if (aORT == ORTDelegatedIncluded) {
    certs[0].assign(signerCert->derCert.data, signerCert->derCert.len);
    context.certs = certs;
  }
  if (aORT == ORTDelegatedIncludedLast || aORT == ORTDelegatedMissingMultiple) {
    certs[0].assign(issuerCert->derCert.data, issuerCert->derCert.len);
    certs[1].assign(cert->derCert.data, cert->derCert.len);
    certs[2].assign(issuerCert->derCert.data, issuerCert->derCert.len);
    if (aORT != ORTDelegatedMissingMultiple) {
      certs[3].assign(signerCert->derCert.data, signerCert->derCert.len);
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
    context.nextUpdate = oldNow + 10;
  }
  if (aORT == ORTLongValidityAlmostExpired) {
    context.thisUpdate = now - (320 * Time::ONE_DAY_IN_SECONDS);
  }
  if (aORT == ORTAncientAlmostExpired) {
    context.thisUpdate = now - (640 * Time::ONE_DAY_IN_SECONDS);
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
    // python DottedOIDToCode.py --tlv some-Mozilla-OID \
    //        1.3.6.1.4.1.13769.666.666.666.1.500.9.2
    static const uint8_t tlv_some_Mozilla_OID[] = {
      0x06, 0x12, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xeb, 0x49, 0x85, 0x1a, 0x85,
      0x1a, 0x85, 0x1a, 0x01, 0x83, 0x74, 0x09, 0x02
    };

    extension.id.assign(tlv_some_Mozilla_OID, sizeof(tlv_some_Mozilla_OID));
    extension.critical = (aORT == ORTCriticalExtension);
    extension.value.push_back(0x05); // tag: NULL
    extension.value.push_back(0x00); // length: 0
    extension.next = nullptr;
    context.extensions = &extension;
  }
  if (aORT == ORTEmptyExtensions) {
    context.includeEmptyExtensions = true;
  }

  if (!signerCert) {
    signerCert = CERT_DupCertificate(issuerCert.get());
  }
  context.signerKeyPair = CreateTestKeyPairFromCert(*signerCert);
  if (!context.signerKeyPair) {
    PrintPRError("PK11_FindKeyByAnyCert failed");
    return nullptr;
  }

  ByteString response(CreateEncodedOCSPResponse(context));
  if (ENCODING_FAILED(response)) {
    PrintPRError("CreateEncodedOCSPResponse failed");
    return nullptr;
  }

  SECItem item = {
    siBuffer,
    const_cast<uint8_t*>(response.data()),
    static_cast<unsigned int>(response.length())
  };
  SECItemArray arr = { &item, 1 };
  return SECITEM_DupArray(aArena, &arr);
}
