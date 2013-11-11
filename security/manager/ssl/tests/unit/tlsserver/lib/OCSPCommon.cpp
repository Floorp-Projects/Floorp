/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OCSPCommon.h"

#include <stdio.h>

#include "TLSServer.h"
#include "ScopedNSSTypes.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::test;


SECItemArray *
GetOCSPResponseForType(OCSPStapleResponseType aOSRT, CERTCertificate *aCert,
                       PLArenaPool *aArena)
{
  if (aOSRT == OSRTNone) {
    if (gDebugLevel >= DEBUG_WARNINGS) {
      fprintf(stderr, "GetOCSPResponseForType called with type OSRTNone, "
                      "which makes no sense.\n");
    }
    return nullptr;
  }

  PRTime now = PR_Now();
  ScopedCERTOCSPCertID id(CERT_CreateOCSPCertID(aCert, now));
  if (!id) {
    PrintPRError("CERT_CreateOCSPCertID failed");
    return nullptr;
  }
  PRTime nextUpdate = now + 10 * PR_USEC_PER_SEC;
  PRTime oneDay = 60*60*24 * (PRTime)PR_USEC_PER_SEC;
  PRTime expiredTime = now - oneDay;
  PRTime oldNow = now - (8 * oneDay);
  PRTime oldNextUpdate = oldNow + 10 * PR_USEC_PER_SEC;

  CERTOCSPSingleResponse *sr = nullptr;
  switch (aOSRT) {
    case OSRTGood:
    case OSRTGoodOtherCA:
      sr = CERT_CreateOCSPSingleResponseGood(aArena, id, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      id.forget(); // owned by sr now
      break;
    case OSRTRevoked:
      sr = CERT_CreateOCSPSingleResponseRevoked(aArena, id, now, &nextUpdate,
                                                expiredTime, nullptr);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseRevoked failed");
        return nullptr;
      }
      id.forget(); // owned by sr now
      break;
    case OSRTUnknown:
      sr = CERT_CreateOCSPSingleResponseUnknown(aArena, id, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseUnknown failed");
        return nullptr;
      }
      id.forget(); // owned by sr now
      break;
    case OSRTExpired:
    case OSRTExpiredFreshCA:
      sr = CERT_CreateOCSPSingleResponseGood(aArena, id, oldNow, &oldNextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      id.forget(); // owned by sr now
      break;
    case OSRTGoodOtherCert:
    {
      ScopedCERTCertificate otherCert(
        PK11_FindCertFromNickname("ocspOtherEndEntity", nullptr));
      if (!otherCert) {
        PrintPRError("PK11_FindCertFromNickname failed");
        return nullptr;
      }

      ScopedCERTOCSPCertID otherID(CERT_CreateOCSPCertID(otherCert, now));
      if (!otherID) {
        PrintPRError("CERT_CreateOCSPCertID failed");
        return nullptr;
      }
      sr = CERT_CreateOCSPSingleResponseGood(aArena, otherID, now, &nextUpdate);
      if (!sr) {
        PrintPRError("CERT_CreateOCSPSingleResponseGood failed");
        return nullptr;
      }
      otherID.forget(); // owned by sr now
      break;
    }
    case OSRTEmpty:
    case OSRTMalformed:
    case OSRTSrverr:
    case OSRTTryLater:
    case OSRTNeedsSig:
    case OSRTUnauthorized:
      break;
    default:
      if (gDebugLevel >= DEBUG_ERRORS) {
        fprintf(stderr, "bad ocsp response type: %d\n", aOSRT);
      }
      return nullptr;
  }

  ScopedCERTCertificate ca;
  if (aOSRT == OSRTGoodOtherCA) {
    ca = PK11_FindCertFromNickname("otherCA", nullptr);
    if (!ca) {
      PrintPRError("PK11_FindCertFromNickname failed");
      return nullptr;
    }
  } else {
    // XXX CERT_FindCertIssuer uses the old, deprecated path-building logic
    ca = CERT_FindCertIssuer(aCert, now, certUsageSSLCA);
    if (!ca) {
      PrintPRError("CERT_FindCertIssuer failed");
      return nullptr;
    }
  }

  PRTime signTime = now;
  if (aOSRT == OSRTExpired) {
    signTime = oldNow;
  }

  CERTOCSPSingleResponse **responses;
  SECItem *response = nullptr;
  switch (aOSRT) {
    case OSRTMalformed:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_MALFORMED_REQUEST);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTSrverr:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_SERVER_ERROR);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTTryLater:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_TRY_SERVER_LATER);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTNeedsSig:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_REQUEST_NEEDS_SIG);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTUnauthorized:
      response = CERT_CreateEncodedOCSPErrorResponse(
        aArena, SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPErrorResponse failed");
        return nullptr;
      }
      break;
    case OSRTEmpty:
      break;
    default:
      // responses is contained in aArena and will be freed when aArena is
      responses = PORT_ArenaNewArray(aArena, CERTOCSPSingleResponse *, 2);
      if (!responses) {
        PrintPRError("PORT_ArenaNewArray failed");
        return nullptr;
      }
      responses[0] = sr;
      responses[1] = nullptr;
      response = CERT_CreateEncodedOCSPSuccessResponse(
        aArena, ca, ocspResponderID_byName, signTime, responses, nullptr);
      if (!response) {
        PrintPRError("CERT_CreateEncodedOCSPSuccessResponse failed");
        return nullptr;
      }
      break;
  }

  SECItemArray *arr = SECITEM_AllocArray(aArena, nullptr, 1);
  arr->items[0].data = response ? response->data : nullptr;
  arr->items[0].len = response ? response->len : 0;

  return arr;
}
