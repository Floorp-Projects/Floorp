/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implements generating OCSP responses of various types. Used by the
// programs in tlsserver/cmd.

#ifndef OCSPCommon_h
#define OCSPCommon_h

#include "certt.h"
#include "seccomon.h"

enum OCSPResponseType
{
  ORTNull = 0,
  ORTGood,             // the certificate is good
  ORTRevoked,          // the certificate has been revoked
  ORTRevokedOld,       // same, but the response is old
  ORTUnknown,          // the responder doesn't know if the cert is good
  ORTUnknownOld,       // same, but the response is old
  ORTGoodOtherCert,    // the response references a different certificate
  ORTGoodOtherCA,      // the wrong CA has signed the response
  ORTExpired,          // the signature on the response has expired
  ORTExpiredFreshCA,   // fresh signature, but old validity period
  ORTNone,             // no stapled response
  ORTEmpty,            // an empty stapled response
  ORTMalformed,        // the response from the responder was malformed
  ORTSrverr,           // the response indicates there was a server error
  ORTTryLater,         // the responder replied with "try again later"
  ORTNeedsSig,         // the response needs a signature
  ORTUnauthorized,     // the responder is not authorized for this certificate
  ORTBadSignature,     // the response has a signature that does not verify
  ORTSkipResponseBytes, // the response does not include responseBytes
  ORTCriticalExtension, // the response includes a critical extension
  ORTNoncriticalExtension, // the response includes an extension that is not critical
  ORTEmptyExtensions,  // the response includes a SEQUENCE OF Extension that is empty
  ORTDelegatedIncluded, // the response is signed by an included delegated responder
  ORTDelegatedIncludedLast, // same, but multiple other certificates are included
  ORTDelegatedMissing, // the response is signed by a not included delegated responder
  ORTDelegatedMissingMultiple, // same, but multiple other certificates are included
  ORTLongValidityAlmostExpired, // a good response, but that was generated a almost a year ago
  ORTAncientAlmostExpired, // a good response, with a validity of almost two years almost expiring
};

struct OCSPHost
{
  const char *mHostName;
  OCSPResponseType mORT;
  const char *mAdditionalCertName; // useful for ORTGoodOtherCert, etc.
  const char *mServerCertName;
};

SECItemArray *
GetOCSPResponseForType(OCSPResponseType aORT, CERTCertificate *aCert,
                       PLArenaPool *aArena, const char *aAdditionalCertName);

#endif // OCSPCommon_h
