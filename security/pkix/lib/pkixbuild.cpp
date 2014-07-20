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
                           KeyUsage requiredKeyUsageIfPresent,
                           KeyPurposeId requiredEKUIfPresent,
                           const CertPolicyId& requiredPolicy,
                           /*optional*/ const SECItem* stapledOCSPResponse,
                           unsigned int subCACount);

TrustDomain::IssuerChecker::IssuerChecker() { }
TrustDomain::IssuerChecker::~IssuerChecker() { }

// The implementation of TrustDomain::IssuerTracker is in a subclass only to
// hide the implementation from external users.
class PathBuildingStep : public TrustDomain::IssuerChecker
{
public:
  PathBuildingStep(TrustDomain& trustDomain, const BackCert& subject,
                   PRTime time, KeyPurposeId requiredEKUIfPresent,
                   const CertPolicyId& requiredPolicy,
                   /*optional*/ const SECItem* stapledOCSPResponse,
                   unsigned int subCACount)
    : trustDomain(trustDomain)
    , subject(subject)
    , time(time)
    , requiredEKUIfPresent(requiredEKUIfPresent)
    , requiredPolicy(requiredPolicy)
    , stapledOCSPResponse(stapledOCSPResponse)
    , subCACount(subCACount)
    , result(Result::FATAL_ERROR_LIBRARY_FAILURE)
    , resultWasSet(false)
  {
  }

  Result Check(const SECItem& potentialIssuerDER,
               /*optional*/ const SECItem* additionalNameConstraints,
               /*out*/ bool& keepGoing);

  Result CheckResult() const;

private:
  TrustDomain& trustDomain;
  const BackCert& subject;
  const PRTime time;
  const KeyPurposeId requiredEKUIfPresent;
  const CertPolicyId& requiredPolicy;
  /*optional*/ SECItem const* const stapledOCSPResponse;
  const unsigned int subCACount;

  Result RecordResult(Result currentResult, /*out*/ bool& keepGoing);
  Result result;
  bool resultWasSet;

  PathBuildingStep(const PathBuildingStep&) /*= delete*/;
  void operator=(const PathBuildingStep&) /*= delete*/;
};

Result
PathBuildingStep::RecordResult(Result newResult, /*out*/ bool& keepGoing)
{
  if (newResult == Result::ERROR_UNTRUSTED_CERT) {
    newResult = Result::ERROR_UNTRUSTED_ISSUER;
  }

  if (resultWasSet) {
    if (result == Success) {
      PR_NOT_REACHED("RecordResult called after finding a chain");
      return Result::FATAL_ERROR_INVALID_STATE;
    }
    // If every potential issuer has the same problem (e.g. expired) and/or if
    // there is only one bad potential issuer, then return a more specific
    // error. Otherwise, punt on trying to decide which error should be
    // returned by returning the generic Result::ERROR_UNKNOWN_ISSUER error.
    if (newResult != Success && newResult != result) {
      newResult = Result::ERROR_UNKNOWN_ISSUER;
    }
  }

  result = newResult;
  resultWasSet = true;
  keepGoing = result != Success;
  return Success;
}

Result
PathBuildingStep::CheckResult() const
{
  if (!resultWasSet) {
    return Result::ERROR_UNKNOWN_ISSUER;
  }
  return result;
}

// The code that executes in the inner loop of BuildForward
Result
PathBuildingStep::Check(const SECItem& potentialIssuerDER,
                        /*optional*/ const SECItem* additionalNameConstraints,
                        /*out*/ bool& keepGoing)
{
  BackCert potentialIssuer(potentialIssuerDER, EndEntityOrCA::MustBeCA,
                           &subject);
  Result rv = potentialIssuer.Init();
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
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
      // XXX: error code
      return RecordResult(Result::ERROR_UNKNOWN_ISSUER, keepGoing);
    }
  }

  if (potentialIssuer.GetNameConstraints()) {
    rv = CheckNameConstraints(*potentialIssuer.GetNameConstraints(),
                              subject, requiredEKUIfPresent);
    if (rv != Success) {
       return RecordResult(rv, keepGoing);
    }
  }

  if (additionalNameConstraints) {
    rv = CheckNameConstraints(*additionalNameConstraints, subject,
                              requiredEKUIfPresent);
    if (rv != Success) {
       return RecordResult(rv, keepGoing);
    }
  }

  // RFC 5280, Section 4.2.1.3: "If the keyUsage extension is present, then the
  // subject public key MUST NOT be used to verify signatures on certificates
  // or CRLs unless the corresponding keyCertSign or cRLSign bit is set."
  rv = BuildForward(trustDomain, potentialIssuer, time, KeyUsage::keyCertSign,
                    requiredEKUIfPresent, requiredPolicy, nullptr, subCACount);
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  rv = trustDomain.VerifySignedData(subject.GetSignedData(),
                                    potentialIssuer.GetSubjectPublicKeyInfo());
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  CertID certID(subject.GetIssuer(), potentialIssuer.GetSubjectPublicKeyInfo(),
                subject.GetSerialNumber());
  rv = trustDomain.CheckRevocation(subject.endEntityOrCA, certID, time,
                                   stapledOCSPResponse,
                                   subject.GetAuthorityInfoAccess());
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  return RecordResult(Success, keepGoing);
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
             KeyUsage requiredKeyUsageIfPresent,
             KeyPurposeId requiredEKUIfPresent,
             const CertPolicyId& requiredPolicy,
             /*optional*/ const SECItem* stapledOCSPResponse,
             unsigned int subCACount)
{
  Result rv;

  TrustLevel trustLevel;
  // If this is an end-entity and not a trust anchor, we defer reporting
  // any error found here until after attempting to find a valid chain.
  // See the explanation of error prioritization in pkix.h.
  rv = CheckIssuerIndependentProperties(trustDomain, subject, time,
                                        requiredKeyUsageIfPresent,
                                        requiredEKUIfPresent, requiredPolicy,
                                        subCACount, trustLevel);
  Result deferredEndEntityError = Success;
  if (rv != Success) {
    if (subject.endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
        trustLevel != TrustLevel::TrustAnchor) {
      deferredEndEntityError = rv;
    } else {
      return rv;
    }
  }

  if (trustLevel == TrustLevel::TrustAnchor) {
    // End of the recursion.

    NonOwningDERArray chain;
    for (const BackCert* cert = &subject; cert; cert = cert->childCert) {
      rv = chain.Append(cert->GetDER());
      if (rv != Success) {
        PR_NOT_REACHED("NonOwningDERArray::SetItem failed.");
        return rv;
      }
    }

    // This must be done here, after the chain is built but before any
    // revocation checks have been done.
    return trustDomain.IsChainValid(chain);
  }

  if (subject.endEntityOrCA == EndEntityOrCA::MustBeCA) {
    // Avoid stack overflows and poor performance by limiting cert chain
    // length.
    static const unsigned int MAX_SUBCA_COUNT = 6;
    static_assert(1/*end-entity*/ + MAX_SUBCA_COUNT + 1/*root*/ ==
                  NonOwningDERArray::MAX_LENGTH,
                  "MAX_SUBCA_COUNT and NonOwningDERArray::MAX_LENGTH mismatch.");
    if (subCACount >= MAX_SUBCA_COUNT) {
      return Result::ERROR_UNKNOWN_ISSUER;
    }
    ++subCACount;
  } else {
    PR_ASSERT(subCACount == 0);
  }

  // Find a trusted issuer.

  PathBuildingStep pathBuilder(trustDomain, subject, time,
                               requiredEKUIfPresent, requiredPolicy,
                               stapledOCSPResponse, subCACount);

  // TODO(bug 965136): Add SKI/AKI matching optimizations
  rv = trustDomain.FindIssuer(subject.GetIssuer(), pathBuilder, time);
  if (rv != Success) {
    return rv;
  }

  rv = pathBuilder.CheckResult();
  if (rv != Success) {
    return rv;
  }

  // If we found a valid chain but deferred reporting an error with the
  // end-entity certificate, report it now.
  if (deferredEndEntityError != Success) {
    return deferredEndEntityError;
  }

  // We've built a valid chain from the subject cert up to a trusted root.
  return Success;
}

Result
BuildCertChain(TrustDomain& trustDomain, const SECItem& certDER,
               PRTime time, EndEntityOrCA endEntityOrCA,
               KeyUsage requiredKeyUsageIfPresent,
               KeyPurposeId requiredEKUIfPresent,
               const CertPolicyId& requiredPolicy,
               /*optional*/ const SECItem* stapledOCSPResponse)
{
  // XXX: Support the legacy use of the subject CN field for indicating the
  // domain name the certificate is valid for.
  BackCert cert(certDER, endEntityOrCA, nullptr);
  Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }

  // See documentation for CheckPublicKey() in pkixtypes.h for why the public
  // key also needs to be checked here when trustDomain.VerifySignedData()
  // should already be doing it.
  rv = trustDomain.CheckPublicKey(cert.GetSubjectPublicKeyInfo());
  if (rv != Success) {
    return rv;
  }

  return BuildForward(trustDomain, cert, time, requiredKeyUsageIfPresent,
                      requiredEKUIfPresent, requiredPolicy, stapledOCSPResponse,
                      0/*subCACount*/);
}

} } // namespace mozilla::pkix
