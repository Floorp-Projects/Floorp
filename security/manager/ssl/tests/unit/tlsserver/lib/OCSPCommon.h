/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implements generating OCSP responses of various types. Used by the
// programs in tlsserver/cmd.

#ifndef OCSPCommon_h
#define OCSPCommon_h

#include "certt.h"
#include "seccomon.h"

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
  OSRTEmpty,            // an empty stapled response
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

SECItemArray *
GetOCSPResponseForType(OCSPStapleResponseType aOSRT, CERTCertificate *aCert,
                       PLArenaPool *aArena);

#endif // OCSPCommon_h
