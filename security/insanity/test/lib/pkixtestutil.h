/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef insanity_test__pkixtestutils_h
#define insanity_test__pkixtestutils_h

#include "insanity/ScopedPtr.h"
#include "insanity/pkixtypes.h"
#include "seccomon.h"

namespace insanity { namespace test {

class OCSPResponseContext
{
public:
  PLArenaPool* arena;
  // TODO(bug 980538): add a way to specify what certificates are included.
  pkix::ScopedCERTCertificate cert; // The subject of the OCSP response
  pkix::ScopedCERTCertificate issuerCert; // The issuer of the subject
  pkix::ScopedCERTCertificate signerCert; // This cert signs the response
  uint8_t responseStatus; // See the OCSPResponseStatus enum in rfc 6960
  // TODO(bug 979070): add ability to generate a response with no responseBytes

  // The following fields are on a per-SingleResponse basis. In the future we
  // may support including multiple SingleResponses per response.
  PRTime producedAt;
  PRTime thisUpdate;
  PRTime nextUpdate;
  bool includeNextUpdate;
  SECOidTag certIDHashAlg;
  uint8_t certStatus;     // See the CertStatus choice in rfc 6960
  PRTime revocationTime; // For certStatus == revoked
  bool badSignature; // If true, alter the signature to fail verification

  enum ResponderIDType {
    ByName = 1,
    ByKeyHash = 2
  };
  ResponderIDType responderIDType;
};

// The return value, if non-null, is owned by the arena in the context
// and MUST NOT be freed.
// This function does its best to respect the NSPR error code convention
// (that is, if it returns null, calling PR_GetError() will return the
// error of the failed operation). However, this is not guaranteed.
SECItem* CreateEncodedOCSPResponse(OCSPResponseContext& context);

} } // namespace insanity::test

#endif // insanity_test__pkixtestutils_h
