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

#ifndef insanity_pkix__pkixutil_h
#define insanity_pkix__pkixutil_h

#include "insanity/pkixtypes.h"
#include "prerror.h"
#include "seccomon.h"
#include "secerr.h"

namespace insanity { namespace pkix {

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
  // nssCert and childCert must be valid for the lifetime of BackCert
  BackCert(CERTCertificate* nssCert, BackCert* childCert)
    : encodedBasicConstraints(nullptr)
    , encodedKeyUsage(nullptr)
    , childCert(childCert)
    , nssCert(nssCert)
  {
  }

  Result Init();

  const SECItem* encodedBasicConstraints;
  const SECItem* encodedKeyUsage;

  BackCert* const childCert;

  const CERTCertificate* GetNSSCert() const { return nssCert; }

  // This is the only place where we should be dealing with non-const
  // CERTCertificates.
  Result PrependNSSCertToList(CERTCertList* results);

  PLArenaPool* GetArena();

private:
  CERTCertificate* nssCert;

  ScopedPLArenaPool arena;

  BackCert(const BackCert&) /* = delete */;
  void operator=(const BackCert&); /* = delete */;
};

} } // namespace insanity::pkix

#endif // insanity_pkix__pkixutil_h
