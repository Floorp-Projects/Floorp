/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
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

#ifndef mozilla_pkix__pkixutil_h
#define mozilla_pkix__pkixutil_h

#include "pkix/enumclass.h"
#include "pkix/pkixtypes.h"
#include "prerror.h"
#include "seccomon.h"
#include "secerr.h"

namespace mozilla { namespace pkix {

enum Result
{
  Success = 0,
  FatalError = -1,      // An error was encountered that caused path building
                        // to stop immediately. example: out-of-memory.
  RecoverableError = -2 // an error that will cause path building to continue
                        // searching for alternative paths. example: expired
                        // certificate.
};

// When returning errors, use this function instead of calling PR_SetError
// directly. This helps ensure that we always call PR_SetError when we return
// an error code. This is a useful place to set a breakpoint when a debugging
// a certificate verification failure.
inline Result
Fail(Result result, PRErrorCode errorCode)
{
  PR_ASSERT(result != Success);
  PR_SetError(errorCode, 0);
  return result;
}

inline Result
MapSECStatus(SECStatus srv)
{
  if (srv == SECSuccess) {
    return Success;
  }

  PRErrorCode error = PORT_GetError();
  switch (error) {
    case SEC_ERROR_EXTENSION_NOT_FOUND:
      return RecoverableError;

    case PR_INVALID_STATE_ERROR:
    case SEC_ERROR_LIBRARY_FAILURE:
    case SEC_ERROR_NO_MEMORY:
      return FatalError;
  }

  // TODO: PORT_Assert(false); // we haven't classified the error yet
  return RecoverableError;
}

// During path building and verification, we build a linked list of BackCerts
// from the current cert toward the end-entity certificate. The linked list
// is used to verify properties that aren't local to the current certificate
// and/or the direct link between the current certificate and its issuer,
// such as name constraints.
//
// Each BackCert contains pointers to all the given certificate's extensions
// so that we can parse the extension block once and then process the
// extensions in an order that may be different than they appear in the cert.
class BackCert
{
public:
  // IncludeCN::No means that GetConstrainedNames won't include the subject CN
  // in its results. IncludeCN::Yes means that GetConstrainedNames will include
  // the subject CN in its results.
  MOZILLA_PKIX_ENUM_CLASS IncludeCN { No = 0, Yes = 1 };

  // nssCert and childCert must be valid for the lifetime of BackCert
  BackCert(BackCert* childCert, IncludeCN includeCN)
    : encodedBasicConstraints(nullptr)
    , encodedCertificatePolicies(nullptr)
    , encodedExtendedKeyUsage(nullptr)
    , encodedKeyUsage(nullptr)
    , encodedNameConstraints(nullptr)
    , encodedInhibitAnyPolicy(nullptr)
    , childCert(childCert)
    , constrainedNames(nullptr)
    , includeCN(includeCN)
  {
  }

  Result Init(const SECItem& certDER);

  const SECItem& GetDER() const { return nssCert->derCert; }
  const SECItem& GetIssuer() const { return nssCert->derIssuer; }
  const SECItem& GetSubject() const { return nssCert->derSubject; }
  const SECItem& GetSubjectPublicKeyInfo() const
  {
    return nssCert->derPublicKey;
  }

  Result VerifyOwnSignatureWithKey(TrustDomain& trustDomain,
                                   const SECItem& subjectPublicKeyInfo) const;

  const SECItem* encodedBasicConstraints;
  const SECItem* encodedCertificatePolicies;
  const SECItem* encodedExtendedKeyUsage;
  const SECItem* encodedKeyUsage;
  const SECItem* encodedNameConstraints;
  const SECItem* encodedInhibitAnyPolicy;

  BackCert* const childCert;

  // Only non-const so that we can pass this to TrustDomain::IsRevoked,
  // which only takes a non-const pointer because VerifyEncodedOCSPResponse
  // requires it, and that is only because the implementation of
  // VerifyEncodedOCSPResponse does a CERT_DupCertificate. TODO: get rid
  // of that CERT_DupCertificate so that we can make all these things const.
  /*const*/ CERTCertificate* GetNSSCert() const { return nssCert.get(); }

  // Returns the names that should be considered when evaluating name
  // constraints. The list is constructed lazily and cached. The result is a
  // weak reference; do not try to free it, and do not hold long-lived
  // references to it.
  Result GetConstrainedNames(/*out*/ const CERTGeneralName** result);

  PLArenaPool* GetArena();

private:
  ScopedCERTCertificate nssCert;

  ScopedPLArenaPool arena;
  CERTGeneralName* constrainedNames;
  IncludeCN includeCN;

  BackCert(const BackCert&) /* = delete */;
  void operator=(const BackCert&); /* = delete */;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixutil_h
