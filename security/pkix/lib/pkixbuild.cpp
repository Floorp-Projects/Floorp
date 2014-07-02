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

namespace mozilla { namespace pkix {

static Result BuildForward(TrustDomain& trustDomain,
                           const BackCert& subject,
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
                  const BackCert& subject,
                  PRTime time,
                  EndEntityOrCA endEntityOrCA,
                  KeyPurposeId requiredEKUIfPresent,
                  const CertPolicyId& requiredPolicy,
                  const SECItem& potentialIssuerDER,
                  /*optional*/ const SECItem* stapledOCSPResponse,
                  unsigned int subCACount,
                  /*out*/ ScopedCERTCertList& results)
{
  BackCert potentialIssuer(potentialIssuerDER, &subject,
                           BackCert::IncludeCN::No);
  Result rv = potentialIssuer.Init();
  if (rv != Success) {
    return rv;
  }

  // RFC5280 4.2.1.1. Authority Key Identifier
  // RFC5280 4.2.1.2. Subject Key Identifier

  // Loop prevention, done as recommended by RFC4158 Section 5.2
  // TODO: this doesn't account for subjectAltNames!
  // TODO(perf): This probably can and should be optimized in some way.
  bool loopDetected = false;
  for (const BackCert* prev = potentialIssuer.childCert;
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

  SECStatus srv = trustDomain.VerifySignedData(
                                subject.GetSignedData(),
                                potentialIssuer.GetSubjectPublicKeyInfo());
  if (srv != SECSuccess) {
    return MapSECStatus(srv);
  }

  CertID certID(subject.GetIssuer(), potentialIssuer.GetSubjectPublicKeyInfo(),
                subject.GetSerialNumber());
  srv = trustDomain.CheckRevocation(endEntityOrCA, certID, time,
                                    stapledOCSPResponse,
                                    subject.GetAuthorityInfoAccess());
  if (srv != SECSuccess) {
    return MapSECStatus(SECFailure);
  }

  return Success;
}

// Recursively build the path from the given subject certificate to the root.
//
// Be very careful about changing the order of checks. The order is significant
// because it affects which error we return when a certificate or certificate
// chain has multiple problems. See the error ranking documentation in
// pkix/pkix.h.
static Result
BuildForward(TrustDomain& trustDomain,
             const BackCert& subject,
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
    for (const BackCert* cert = &subject; cert; cert = cert->childCert) {
      ScopedCERTCertificate
        nssCert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                        const_cast<SECItem*>(&cert->GetDER()),
                                        nullptr, false, true));
      if (CERT_AddCertToListHead(results.get(), nssCert.get()) != SECSuccess) {
        return MapSECStatus(SECFailure);
      }
      nssCert.release(); // nssCert is now owned by results.
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
    rv = BuildForwardInner(trustDomain, subject, time, endEntityOrCA,
                           requiredEKUIfPresent,
                           requiredPolicy, n->cert->derCert,
                           stapledOCSPResponse, subCACount, results);
    if (rv == Success) {
      // We have built a complete chain from the current subject to a root.

      // If we built a valid chain but deferred reporting an error with the
      // end-entity certificate, report that error now.
      if (deferredEndEntityError != 0) {
        return Fail(FatalError, deferredEndEntityError);
      }

      // We have built a valid chain for the current subject.
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
BuildCertChain(TrustDomain& trustDomain, const SECItem& certDER,
               PRTime time, EndEntityOrCA endEntityOrCA,
               KeyUsage requiredKeyUsageIfPresent,
               KeyPurposeId requiredEKUIfPresent,
               const CertPolicyId& requiredPolicy,
               /*optional*/ const SECItem* stapledOCSPResponse,
               /*out*/ ScopedCERTCertList& results)
{
  // XXX: Support the legacy use of the subject CN field for indicating the
  // domain name the certificate is valid for.
  BackCert::IncludeCN includeCN
    = endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
      requiredEKUIfPresent == KeyPurposeId::id_kp_serverAuth
    ? BackCert::IncludeCN::Yes
    : BackCert::IncludeCN::No;

  BackCert cert(certDER, nullptr, includeCN);
  Result rv = cert.Init();
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
