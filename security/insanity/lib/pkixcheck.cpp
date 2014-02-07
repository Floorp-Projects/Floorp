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

#include "insanity/pkix.h"
#include "pkixcheck.h"
#include "pkixder.h"
#include "pkixutil.h"
#include "secder.h"

namespace insanity { namespace pkix {

Result
CheckTimes(const CERTCertificate* cert, PRTime time)
{
  PR_ASSERT(cert);

  SECCertTimeValidity validity = CERT_CheckCertValidTimes(cert, time, false);
  if (validity != secCertTimeValid) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }

  return Success;
}

// RFC5280 4.2.1.9. Basic Constraints (id-ce-basicConstraints)
Result
CheckBasicConstraints(const BackCert& cert,
                      EndEntityOrCA endEntityOrCA,
                      bool isTrustAnchor,
                      unsigned int subCACount)
{
  CERTBasicConstraints basicConstraints;
  if (cert.encodedBasicConstraints) {
    SECStatus rv = CERT_DecodeBasicConstraintValue(&basicConstraints,
                                                   cert.encodedBasicConstraints);
    if (rv != SECSuccess) {
      return MapSECStatus(rv);
    }
  } else {
    // Synthesize a non-CA basic constraints by default
    basicConstraints.isCA = false;
    basicConstraints.pathLenConstraint = 0;

    // "If the basic constraints extension is not present in a version 3
    //  certificate, or the extension is present but the cA boolean is not
    //  asserted, then the certified public key MUST NOT be used to verify
    //  certificate signatures."
    //
    // For compatibility, we must accept v1 trust anchors without basic
    // constraints as CAs.
    //
    // TODO: add check for self-signedness?
    if (endEntityOrCA == MustBeCA && isTrustAnchor) {
      const CERTCertificate* nssCert = cert.GetNSSCert();

      der::Input versionDer;
      if (versionDer.Init(nssCert->version.data, nssCert->version.len)
            != der::Success) {
        return RecoverableError;
      }
      uint8_t version;
      if (der::OptionalVersion(versionDer, version) || der::End(versionDer)
            != der::Success) {
        return RecoverableError;
      }
      if (version == 1) {
        basicConstraints.isCA = true;
        basicConstraints.pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
      }
    }
  }

  if (endEntityOrCA == MustBeEndEntity) {
    // CA certificates are not trusted as EE certs.

    if (basicConstraints.isCA) {
      // XXX: We use SEC_ERROR_INADEQUATE_CERT_TYPE here so we can distinguish
      // this error from other errors, given that NSS does not have a "CA cert
      // used as end-entity" error code since it doesn't have such a
      // prohibition. We should add such an error code and stop abusing
      // SEC_ERROR_INADEQUATE_CERT_TYPE this way.
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }

    return Success;
  }

  PORT_Assert(endEntityOrCA == MustBeCA);

  // End-entity certificates are not allowed to act as CA certs.
  if (!basicConstraints.isCA) {
    return Fail(RecoverableError, SEC_ERROR_CA_CERT_INVALID);
  }

  if (basicConstraints.pathLenConstraint >= 0) {
    if (subCACount >
           static_cast<unsigned int>(basicConstraints.pathLenConstraint)) {
      return Fail(RecoverableError, SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID);
    }
  }

  return Success;
}

// Checks extensions that apply to both EE and intermediate certs,
// except for AIA, CRL, and AKI/SKI, which are handled elsewhere.
Result
CheckExtensions(BackCert& cert,
                EndEntityOrCA endEntityOrCA,
                bool isTrustAnchor,
                unsigned int subCACount)
{
  // 4.2.1.1. Authority Key Identifier dealt with as part of path building
  // 4.2.1.2. Subject Key Identifier dealt with as part of path building

  PLArenaPool* arena = cert.GetArena();
  if (!arena) {
    return FatalError;
  }

  Result rv;

  // 4.2.1.3. Key Usage
  // 4.2.1.4. Certificate Policies
  // 4.2.1.5. Policy Mappings are rejected in BackCert::Init()
  // 4.2.1.6. Subject Alternative Name dealt with elsewhere
  // 4.2.1.7. Issuer Alternative Name is not something that needs checking
  // 4.2.1.8. Subject Directory Attributes is not something that needs checking

  // 4.2.1.9. Basic Constraints. We check basic constraints before other
  // properties that take endEntityOrCA so that those checks can use
  // endEntityOrCA as a proxy for the isCA bit from the basic constraints.
  rv = CheckBasicConstraints(cert, endEntityOrCA, isTrustAnchor, subCACount);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.10. Name Constraints
  // 4.2.1.11. Policy Constraints
  // 4.2.1.12. Extended Key Usage
  // 4.2.1.13. CRL Distribution Points will be dealt with elsewhere
  // 4.2.1.14. Inhibit anyPolicy

  return Success;
}

} } // namespace insanity::pkix
