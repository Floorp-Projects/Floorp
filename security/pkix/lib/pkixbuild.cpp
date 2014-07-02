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

#include "pkix/pkix.h"

#include <limits>

#include "pkixcheck.h"
#include "pkixder.h"

namespace mozilla { namespace pkix {

// We assume ext has been zero-initialized by its constructor and otherwise
// not modified.
//
// TODO(perf): This sorting of extensions should be be moved into the
// certificate decoder so that the results are cached with the certificate, so
// that the decoding doesn't have to happen more than once per cert.
Result
BackCert::Init(const SECItem& certDER)
{
  // XXX: Currently-known uses of mozilla::pkix create CERTCertificate objects
  // for all certs anyway, so the overhead of CERT_NewTempCertificate will be
  // reduced to a lookup in NSS's SECItem* -> CERTCertificate cache and
  // a CERT_DupCertificate. Eventually, we should parse the certificate using
  // mozilla::pkix::der and avoid the need to create a CERTCertificate at all.
  nssCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                    const_cast<SECItem*>(&certDER),
                                    nullptr, false, true);
  if (!nssCert) {
    return MapSECStatus(SECFailure);
  }

  if (nssCert->version.len == 1 &&
      nssCert->version.data[0] == static_cast<uint8_t>(der::Version::v3)) {
    version = der::Version::v3;
  } else if (nssCert->version.len == 1 &&
             nssCert->version.data[0] == static_cast<uint8_t>(der::Version::v2)) {
    version = der::Version::v2;
  } else if (nssCert->version.len == 1 &&
             nssCert->version.data[0] == static_cast<uint8_t>(der::Version::v2)) {
    // XXX(bug 1031093): We shouldn't accept an explicit encoding of v1, but we
    // do here for compatibility reasons.
    version = der::Version::v1;
  } else if (nssCert->version.len == 0) {
    version = der::Version::v1;
  } else {
    // Explicit encoding of v1 is not allowed. We do not support any other
    // version except v3.
    return Fail(RecoverableError, SEC_ERROR_BAD_DER);
  }

  const CERTCertExtension* const* exts = nssCert->extensions;
  if (!exts) {
    return Success;
  }

  // Extensions are only allowed in v3 certificates, not v1 or v2. Also, we
  // use presence of the basic constraints extension with isCA==true to decide
  // whether to treat a certificate as a CA certificate, and we don't want to
  // allow v1 or v2 intermediate CA certificates; this check is part of that
  // enforcement as well.
  if (version != der::Version::v3) {
    return Fail(RecoverableError, SEC_ERROR_EXTENSION_VALUE_INVALID);
  }

  const SECItem* dummyEncodedSubjectKeyIdentifier = nullptr;
  const SECItem* dummyEncodedAuthorityKeyIdentifier = nullptr;
  const SECItem* dummyEncodedSubjectAltName = nullptr;

  for (const CERTCertExtension* ext = *exts; ext; ext = *++exts) {
    const SECItem** out = nullptr;

    // python DottedOIDToCode.py id-ce 2.5.29
    static const uint8_t id_ce[] = {
      0x55, 0x1d
    };

    // python DottedOIDToCode.py id-pe-authorityInfoAccess 1.3.6.1.5.5.7.1.1
    static const uint8_t id_pe_authorityInfoAccess[] = {
      0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01
    };

    if (ext->id.len == PR_ARRAY_SIZE(id_ce) + 1 &&
        !memcmp(ext->id.data, id_ce, PR_ARRAY_SIZE(id_ce))) {
      switch (ext->id.data[ext->id.len - 1]) {
        case 14: out = &dummyEncodedSubjectKeyIdentifier; break; // bug 965136
        case 15: out = &encodedKeyUsage; break;
        case 17: out = &dummyEncodedSubjectAltName; break; // bug 970542
        case 19: out = &encodedBasicConstraints; break;
        case 30: out = &encodedNameConstraints; break;
        case 32: out = &encodedCertificatePolicies; break;
        case 35: out = &dummyEncodedAuthorityKeyIdentifier; break; // bug 965136
        case 37: out = &encodedExtendedKeyUsage; break;
        case 54: out = &encodedInhibitAnyPolicy; break; // Bug 989051
      }
    } else if (ext->id.len == PR_ARRAY_SIZE(id_pe_authorityInfoAccess) &&
               !memcmp(ext->id.data, id_pe_authorityInfoAccess,
                       PR_ARRAY_SIZE(id_pe_authorityInfoAccess))) {
      // We should remember the value of the encoded AIA extension here, but
      // since our TrustDomain implementations get the OCSP URI using
      // CERT_GetOCSPAuthorityInfoAccessLocation, we currently don't need to.
      out = &encodedAuthorityInfoAccess;
    }

    // If this is an extension we don't understand and it's marked critical,
    // we must reject this certificate.
    // (The only valid explicit value of the critical flag is TRUE because
    // it is defined as BOOLEAN DEFAULT FALSE, so we just assume it is true.)
    if (!out && ext->critical.data && ext->critical.len > 0) {
      return Fail(RecoverableError, SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
    }

    if (out) {
      // This is an extension we understand. Save it in results unless we've
      // already found the extension previously.
      if (*out) {
        // Duplicate extension
        return Fail(RecoverableError, SEC_ERROR_EXTENSION_VALUE_INVALID);
      }
      *out = &ext->value;
    }
  }

  return Success;
}

Result
BackCert::VerifyOwnSignatureWithKey(TrustDomain& trustDomain,
                                    const SECItem& subjectPublicKeyInfo) const
{
  return MapSECStatus(trustDomain.VerifySignedData(&nssCert->signatureWrap,
                                                   subjectPublicKeyInfo));
}

static Result BuildForward(TrustDomain& trustDomain,
                           BackCert& subject,
                           PRTime time,
                           EndEntityOrCA endEntityOrCA,
                           KeyUsage requiredKeyUsageIfPresent,
                           KeyPurposeId requiredEKUIfPresent,
                           const CertPolicyId& requiredPolicy,
                           /*optional*/ const SECItem* stapledOCSPResponse,
                           unsigned int subCACount,
                           /*out*/ ScopedCERTCertList& results);

// The code that executes in the inner loop of BuildForward
static Result
BuildForwardInner(TrustDomain& trustDomain,
                  BackCert& subject,
                  PRTime time,
                  KeyPurposeId requiredEKUIfPresent,
                  const CertPolicyId& requiredPolicy,
                  const SECItem& potentialIssuerDER,
                  unsigned int subCACount,
                  /*out*/ ScopedCERTCertList& results)
{
  BackCert potentialIssuer(&subject, BackCert::IncludeCN::No);
  Result rv = potentialIssuer.Init(potentialIssuerDER);
  if (rv != Success) {
    return rv;
  }

  // RFC5280 4.2.1.1. Authority Key Identifier
  // RFC5280 4.2.1.2. Subject Key Identifier

  // Loop prevention, done as recommended by RFC4158 Section 5.2
  // TODO: this doesn't account for subjectAltNames!
  // TODO(perf): This probably can and should be optimized in some way.
  bool loopDetected = false;
  for (BackCert* prev = potentialIssuer.childCert;
       !loopDetected && prev != nullptr; prev = prev->childCert) {
    if (SECITEM_ItemsAreEqual(&potentialIssuer.GetSubjectPublicKeyInfo(),
                              &prev->GetSubjectPublicKeyInfo()) &&
        SECITEM_ItemsAreEqual(&potentialIssuer.GetSubject(),
                              &prev->GetSubject())) {
      return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER); // XXX: error code
    }
  }

  rv = CheckNameConstraints(potentialIssuer);
  if (rv != Success) {
    return rv;
  }

  // RFC 5280, Section 4.2.1.3: "If the keyUsage extension is present, then the
  // subject public key MUST NOT be used to verify signatures on certificates
  // or CRLs unless the corresponding keyCertSign or cRLSign bit is set."
  rv = BuildForward(trustDomain, potentialIssuer, time, EndEntityOrCA::MustBeCA,
                    KeyUsage::keyCertSign, requiredEKUIfPresent,
                    requiredPolicy, nullptr, subCACount, results);
  if (rv != Success) {
    return rv;
  }

  return subject.VerifyOwnSignatureWithKey(
                   trustDomain, potentialIssuer.GetSubjectPublicKeyInfo());
}

// Recursively build the path from the given subject certificate to the root.
//
// Be very careful about changing the order of checks. The order is significant
// because it affects which error we return when a certificate or certificate
// chain has multiple problems. See the error ranking documentation in
// pkix/pkix.h.
static Result
BuildForward(TrustDomain& trustDomain,
             BackCert& subject,
             PRTime time,
             EndEntityOrCA endEntityOrCA,
             KeyUsage requiredKeyUsageIfPresent,
             KeyPurposeId requiredEKUIfPresent,
             const CertPolicyId& requiredPolicy,
             /*optional*/ const SECItem* stapledOCSPResponse,
             unsigned int subCACount,
             /*out*/ ScopedCERTCertList& results)
{
  Result rv;

  TrustLevel trustLevel;
  // If this is an end-entity and not a trust anchor, we defer reporting
  // any error found here until after attempting to find a valid chain.
  // See the explanation of error prioritization in pkix.h.
  rv = CheckIssuerIndependentProperties(trustDomain, subject, time,
                                        endEntityOrCA,
                                        requiredKeyUsageIfPresent,
                                        requiredEKUIfPresent, requiredPolicy,
                                        subCACount, &trustLevel);
  PRErrorCode deferredEndEntityError = 0;
  if (rv != Success) {
    if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
        trustLevel != TrustLevel::TrustAnchor) {
      deferredEndEntityError = PR_GetError();
    } else {
      return rv;
    }
  }

  if (trustLevel == TrustLevel::TrustAnchor) {
    // End of the recursion.

    // Construct the results cert chain.
    results = CERT_NewCertList();
    if (!results) {
      return MapSECStatus(SECFailure);
    }
    for (BackCert* cert = &subject; cert; cert = cert->childCert) {
      CERTCertificate* dup = CERT_DupCertificate(cert->GetNSSCert());
      if (CERT_AddCertToListHead(results.get(), dup) != SECSuccess) {
        CERT_DestroyCertificate(dup);
        return MapSECStatus(SECFailure);
      }
      // dup is now owned by results.
    }

    // This must be done here, after the chain is built but before any
    // revocation checks have been done.
    SECStatus srv = trustDomain.IsChainValid(results.get());
    if (srv != SECSuccess) {
      return MapSECStatus(srv);
    }

    return Success;
  }

  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    // Avoid stack overflows and poor performance by limiting cert chain
    // length.
    static const unsigned int MAX_SUBCA_COUNT = 6;
    if (subCACount >= MAX_SUBCA_COUNT) {
      return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER);
    }
    ++subCACount;
  } else {
    PR_ASSERT(subCACount == 0);
  }

  // Find a trusted issuer.
  // TODO(bug 965136): Add SKI/AKI matching optimizations
  ScopedCERTCertList candidates;
  if (trustDomain.FindPotentialIssuers(&subject.GetIssuer(), time,
                                       candidates) != SECSuccess) {
    return MapSECStatus(SECFailure);
  }
  if (!candidates) {
    return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER);
  }

  PRErrorCode errorToReturn = 0;

  for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
       !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
    rv = BuildForwardInner(trustDomain, subject, time, requiredEKUIfPresent,
                           requiredPolicy, n->cert->derCert, subCACount,
                           results);
    if (rv == Success) {
      // If we found a valid chain but deferred reporting an error with the
      // end-entity certificate, report it now.
      if (deferredEndEntityError != 0) {
        return Fail(FatalError, deferredEndEntityError);
      }

      CertID certID(subject.GetIssuer(), n->cert->derPublicKey,
                    subject.GetSerialNumber());
      SECStatus srv = trustDomain.CheckRevocation(
                                    endEntityOrCA, certID, time,
                                    stapledOCSPResponse,
                                    subject.encodedAuthorityInfoAccess);
      if (srv != SECSuccess) {
        return MapSECStatus(SECFailure);
      }

      // We found a trusted issuer. At this point, we know the cert is valid
      // and results contains the complete cert chain.
      return Success;
    }
    if (rv != RecoverableError) {
      return rv;
    }

    PRErrorCode currentError = PR_GetError();
    switch (currentError) {
      case 0:
        PR_NOT_REACHED("Error code not set!");
        return Fail(FatalError, PR_INVALID_STATE_ERROR);
      case SEC_ERROR_UNTRUSTED_CERT:
        currentError = SEC_ERROR_UNTRUSTED_ISSUER;
        break;
      default:
        break;
    }
    if (errorToReturn == 0) {
      errorToReturn = currentError;
    } else if (errorToReturn != currentError) {
      errorToReturn = SEC_ERROR_UNKNOWN_ISSUER;
    }
  }

  if (errorToReturn == 0) {
    errorToReturn = SEC_ERROR_UNKNOWN_ISSUER;
  }

  return Fail(RecoverableError, errorToReturn);
}

SECStatus
BuildCertChain(TrustDomain& trustDomain,
               const CERTCertificate* nssCert,
               PRTime time,
               EndEntityOrCA endEntityOrCA,
               KeyUsage requiredKeyUsageIfPresent,
               KeyPurposeId requiredEKUIfPresent,
               const CertPolicyId& requiredPolicy,
               /*optional*/ const SECItem* stapledOCSPResponse,
               /*out*/ ScopedCERTCertList& results)
{
  if (!nssCert) {
    PR_NOT_REACHED("null cert passed to BuildCertChain");
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  // XXX: Support the legacy use of the subject CN field for indicating the
  // domain name the certificate is valid for.
  BackCert::IncludeCN includeCN
    = endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
      requiredEKUIfPresent == KeyPurposeId::id_kp_serverAuth
    ? BackCert::IncludeCN::Yes
    : BackCert::IncludeCN::No;

  BackCert cert(nullptr, includeCN);
  Result rv = cert.Init(nssCert->derCert);
  if (rv != Success) {
    return SECFailure;
  }

  rv = BuildForward(trustDomain, cert, time, endEntityOrCA,
                    requiredKeyUsageIfPresent, requiredEKUIfPresent,
                    requiredPolicy, stapledOCSPResponse, 0, results);
  if (rv != Success) {
    results = nullptr;
    return SECFailure;
  }

  return SECSuccess;
}

} } // namespace mozilla::pkix
