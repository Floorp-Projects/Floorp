/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a standalone server that delivers various stapled OCSP responses.
// The client is expected to connect, initiate an SSL handshake (with SNI
// to indicate which "server" to connect to), and verify the OCSP response.
// If all is good, the client then sends one encrypted byte and receives that
// same byte back.
// This server also has the ability to "call back" another process waiting on
// it. That is, when the server is all set up and ready to receive connections,
// it will connect to a specified port and issue a simple HTTP request.

#include <stdio.h>
#include "TLSServer.h"
#include "ScopedNSSTypes.h"
#include "ssl.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::test;

enum OCSPStapleResponseType
{
  OSRTNull = 0,
  OSRTGood,             // the certificate is good
  OSRTRevoked,          // the certificate has been revoked
  OSRTUnknown,          // the responder doesn't know if the cert is good
  OSRTGoodOtherCert,    // the response references a different certificate
  OSRTGoodOtherCA,      // the wrong CA has signed the response
  OSRTExpired,          // the signature on the response has expired
  OSRTExpiredFreshCA,   // fresh signature, but old validity period
  OSRTNone,             // no stapled response
  OSRTMalformed,        // the response from the responder was malformed
  OSRTSrverr,           // the response indicates there was a server error
  OSRTTryLater,         // the responder replied with "try again later"
  OSRTNeedsSig,         // the response needs a signature
  OSRTUnauthorized      // the responder is not authorized for this certificate
};

struct OCSPHost
{
  const char *mHostName;
  OCSPStapleResponseType mOSRT;
};

const OCSPHost sOCSPHosts[] =
{
  { "ocsp-stapling-good.example.com", OSRTGood },
  { "ocsp-stapling-revoked.example.com", OSRTRevoked },
  { "ocsp-stapling-unknown.example.com", OSRTUnknown },
  { "ocsp-stapling-good-other.example.com", OSRTGoodOtherCert },
  { "ocsp-stapling-good-other-ca.example.com", OSRTGoodOtherCA },
  { "ocsp-stapling-expired.example.com", OSRTExpired },
  { "ocsp-stapling-expired-fresh-ca.example.com", OSRTExpiredFreshCA },
  { "ocsp-stapling-none.example.com", OSRTNone },
  { "ocsp-stapling-malformed.example.com", OSRTMalformed },
  { "ocsp-stapling-srverr.example.com", OSRTSrverr },
  { "ocsp-stapling-trylater.example.com", OSRTTryLater },
  { "ocsp-stapling-needssig.example.com", OSRTNeedsSig },
  { "ocsp-stapling-unauthorized.example.com", OSRTUnauthorized },
  { nullptr, OSRTNull }
};

SECItemArray *
GetOCSPResponseForType(OCSPStapleResponseType aOSRT, CERTCertificate *aCert,
                       PLArenaPool *aArena)
{
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
    case OSRTNone:
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
      break;
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
    case OSRTNone:
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

int32_t
DoSNISocketConfig(PRFileDesc *aFd, const SECItem *aSrvNameArr,
                  uint32_t aSrvNameArrSize, void *aArg)
{
  const OCSPHost *host = GetHostForSNI(aSrvNameArr, aSrvNameArrSize,
                                       sOCSPHosts);
  if (!host) {
    return SSL_SNI_SEND_ALERT;
  }

  if (gDebugLevel >= DEBUG_VERBOSE) {
    fprintf(stderr, "found pre-defined host '%s'\n", host->mHostName);
  }

  ScopedCERTCertificate cert;
  SSLKEAType certKEA;
  if (SECSuccess != ConfigSecureServerWithNamedCert(aFd, DEFAULT_CERT_NICKNAME,
                                                    &cert, &certKEA)) {
    return SSL_SNI_SEND_ALERT;
  }

  PLArenaPool *arena = PORT_NewArena(1024);
  if (!arena) {
    PrintPRError("PORT_NewArena failed");
    return SSL_SNI_SEND_ALERT;
  }

  // response is contained by the arena - freeing the arena will free it
  SECItemArray *response = GetOCSPResponseForType(host->mOSRT, cert, arena);
  if (!response) {
    PORT_FreeArena(arena, PR_FALSE);
    return SSL_SNI_SEND_ALERT;
  }

  // SSL_SetStapledOCSPResponses makes a deep copy of response
  SECStatus st = SSL_SetStapledOCSPResponses(aFd, response, certKEA);
  PORT_FreeArena(arena, PR_FALSE);
  if (st != SECSuccess) {
    PrintPRError("SSL_SetStapledOCSPResponses failed");
    return SSL_SNI_SEND_ALERT;
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <NSS DB directory>\n", argv[0]);
    return 1;
  }

  return StartServer(argv[1], DoSNISocketConfig, nullptr);
}
