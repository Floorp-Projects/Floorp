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

#include "mozpkix/pkix.h"

#include "mozpkix/pkixcheck.h"
#include "mozpkix/pkixutil.h"

namespace mozilla { namespace pkix {

static Result BuildForward(TrustDomain& trustDomain,
                           const BackCert& subject,
                           Time time,
                           KeyUsage requiredKeyUsageIfPresent,
                           KeyPurposeId requiredEKUIfPresent,
                           const CertPolicyId& requiredPolicy,
                           /*optional*/ const Input* stapledOCSPResponse,
                           unsigned int subCACount,
                           unsigned int& buildForwardCallBudget);

TrustDomain::IssuerChecker::IssuerChecker() { }
TrustDomain::IssuerChecker::~IssuerChecker() { }

// The implementation of TrustDomain::IssuerTracker is in a subclass only to
// hide the implementation from external users.
class PathBuildingStep final : public TrustDomain::IssuerChecker
{
public:
  PathBuildingStep(TrustDomain& aTrustDomain, const BackCert& aSubject,
                   Time aTime, KeyPurposeId aRequiredEKUIfPresent,
                   const CertPolicyId& aRequiredPolicy,
                   /*optional*/ const Input* aStapledOCSPResponse,
                   unsigned int aSubCACount, Result aDeferredSubjectError,
                   unsigned int& aBuildForwardCallBudget)
    : trustDomain(aTrustDomain)
    , subject(aSubject)
    , time(aTime)
    , requiredEKUIfPresent(aRequiredEKUIfPresent)
    , requiredPolicy(aRequiredPolicy)
    , stapledOCSPResponse(aStapledOCSPResponse)
    , subCACount(aSubCACount)
    , deferredSubjectError(aDeferredSubjectError)
    , subjectSignaturePublicKeyAlg(der::PublicKeyAlgorithm::Uninitialized)
    , result(Result::FATAL_ERROR_LIBRARY_FAILURE)
    , resultWasSet(false)
    , buildForwardCallBudget(aBuildForwardCallBudget)
  {
  }

  Result Check(Input potentialIssuerDER,
               /*optional*/ const Input* additionalNameConstraints,
               /*out*/ bool& keepGoing) override;

  Result CheckResult() const;

private:
  TrustDomain& trustDomain;
  const BackCert& subject;
  const Time time;
  const KeyPurposeId requiredEKUIfPresent;
  const CertPolicyId& requiredPolicy;
  /*optional*/ Input const* const stapledOCSPResponse;
  const unsigned int subCACount;
  const Result deferredSubjectError;

  // Initialized lazily.
  uint8_t subjectSignatureDigestBuf[MAX_DIGEST_SIZE_IN_BYTES];
  der::PublicKeyAlgorithm subjectSignaturePublicKeyAlg;
  SignedDigest subjectSignature;

  Result RecordResult(Result currentResult, /*out*/ bool& keepGoing);
  Result result;
  bool resultWasSet;
  unsigned int& buildForwardCallBudget;

  PathBuildingStep(const PathBuildingStep&) = delete;
  void operator=(const PathBuildingStep&) = delete;
};

Result
PathBuildingStep::RecordResult(Result newResult, /*out*/ bool& keepGoing)
{
  if (newResult == Result::ERROR_UNTRUSTED_CERT) {
    newResult = Result::ERROR_UNTRUSTED_ISSUER;
  } else if (newResult == Result::ERROR_EXPIRED_CERTIFICATE) {
    newResult = Result::ERROR_EXPIRED_ISSUER_CERTIFICATE;
  } else if (newResult == Result::ERROR_NOT_YET_VALID_CERTIFICATE) {
    newResult = Result::ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE;
  }

  if (resultWasSet) {
    if (result == Success) {
      return NotReached("RecordResult called after finding a chain",
                        Result::FATAL_ERROR_INVALID_STATE);
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
PathBuildingStep::Check(Input potentialIssuerDER,
           /*optional*/ const Input* additionalNameConstraints,
                /*out*/ bool& keepGoing)
{
  BackCert potentialIssuer(potentialIssuerDER, EndEntityOrCA::MustBeCA,
                           &subject);
  Result rv = potentialIssuer.Init();
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  // Simple TrustDomain::FindIssuers implementations may pass in all possible
  // CA certificates without any filtering. Because of this, we don't consider
  // a mismatched name to be an error. Instead, we just pretend that any
  // certificate without a matching name was never passed to us. In particular,
  // we treat the case where the TrustDomain only asks us to check CA
  // certificates with mismatched names as equivalent to the case where the
  // TrustDomain never called Check() at all.
  if (!InputsAreEqual(potentialIssuer.GetSubject(), subject.GetIssuer())) {
    keepGoing = true;
    return Success;
  }

  // Loop prevention, done as recommended by RFC4158 Section 5.2
  // TODO: this doesn't account for subjectAltNames!
  // TODO(perf): This probably can and should be optimized in some way.
  for (const BackCert* prev = potentialIssuer.childCert; prev;
       prev = prev->childCert) {
    if (InputsAreEqual(potentialIssuer.GetSubjectPublicKeyInfo(),
                       prev->GetSubjectPublicKeyInfo()) &&
        InputsAreEqual(potentialIssuer.GetSubject(), prev->GetSubject())) {
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

  rv = CheckTLSFeatures(subject, potentialIssuer);
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  // If we've ran out of budget, stop searching.
  if (buildForwardCallBudget == 0) {
    Result savedRv = RecordResult(Result::ERROR_UNKNOWN_ISSUER, keepGoing);
    keepGoing = false;
    return savedRv;
  }
  buildForwardCallBudget--;

  // RFC 5280, Section 4.2.1.3: "If the keyUsage extension is present, then the
  // subject public key MUST NOT be used to verify signatures on certificates
  // or CRLs unless the corresponding keyCertSign or cRLSign bit is set."
  rv = BuildForward(trustDomain, potentialIssuer, time, KeyUsage::keyCertSign,
                    requiredEKUIfPresent, requiredPolicy, nullptr, subCACount,
                    buildForwardCallBudget);
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  // Calculate the digest of the subject's signed data if we haven't already
  // done so. We do this lazily to avoid doing it at all if we backtrack before
  // getting to this point. We cache the result to avoid recalculating it if we
  // backtrack after getting to this point.
  if (subjectSignature.digest.GetLength() == 0) {
    rv = DigestSignedData(trustDomain, subject.GetSignedData(),
                          subjectSignatureDigestBuf,
                          subjectSignaturePublicKeyAlg, subjectSignature);
    if (rv != Success) {
      return rv;
    }
  }

  rv = VerifySignedDigest(trustDomain, subjectSignaturePublicKeyAlg,
                          subjectSignature,
                          potentialIssuer.GetSubjectPublicKeyInfo());
  if (rv != Success) {
    return RecordResult(rv, keepGoing);
  }

  // We avoid doing revocation checking for expired certificates because OCSP
  // responders are allowed to forget about expired certificates, and many OCSP
  // responders return an error when asked for the status of an expired
  // certificate.
  if (deferredSubjectError != Result::ERROR_EXPIRED_CERTIFICATE) {
    CertID certID(subject.GetIssuer(), potentialIssuer.GetSubjectPublicKeyInfo(),
                  subject.GetSerialNumber());
    Time notBefore(Time::uninitialized);
    Time notAfter(Time::uninitialized);
    // This should never fail. If we're here, we've already parsed the validity
    // and checked that the given time is in the certificate's validity period.
    rv = ParseValidity(subject.GetValidity(), &notBefore, &notAfter);
    if (rv != Success) {
      return rv;
    }
    Duration validityDuration(notAfter, notBefore);
    rv = trustDomain.CheckRevocation(subject.endEntityOrCA, certID, time,
                                     notBefore, validityDuration,
                                     stapledOCSPResponse,
                                     subject.GetAuthorityInfoAccess());
    if (rv != Success) {
      // Since this is actually a problem with the current subject certificate
      // (rather than the issuer), it doesn't make sense to keep going; all
      // paths through this certificate will fail.
      Result savedRv = RecordResult(rv, keepGoing);
      keepGoing = false;
      return savedRv;
    }

    if (subject.endEntityOrCA == EndEntityOrCA::MustBeEndEntity) {
      const Input* sctExtension = subject.GetSignedCertificateTimestamps();
      if (sctExtension) {
        Input sctList;
        rv = ExtractSignedCertificateTimestampListFromExtension(*sctExtension,
                                                                sctList);
        if (rv != Success) {
          // Again, the problem is with this certificate, and all paths through
          // it will fail.
          Result savedRv = RecordResult(rv, keepGoing);
          keepGoing = false;
          return savedRv;
        }
        trustDomain.NoteAuxiliaryExtension(AuxiliaryExtension::EmbeddedSCTList,
                                           sctList);
      }
    }
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
             Time time,
             KeyUsage requiredKeyUsageIfPresent,
             KeyPurposeId requiredEKUIfPresent,
             const CertPolicyId& requiredPolicy,
             /*optional*/ const Input* stapledOCSPResponse,
             unsigned int subCACount,
             unsigned int& buildForwardCallBudget)
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
        return NotReached("NonOwningDERArray::SetItem failed.", rv);
      }
    }

    // This must be done here, after the chain is built but before any
    // revocation checks have been done.
    return trustDomain.IsChainValid(chain, time, requiredPolicy);
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
    assert(subCACount == 0);
  }

  // Find a trusted issuer.

  PathBuildingStep pathBuilder(trustDomain, subject, time,
                               requiredEKUIfPresent, requiredPolicy,
                               stapledOCSPResponse, subCACount,
                               deferredEndEntityError, buildForwardCallBudget);

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
BuildCertChain(TrustDomain& trustDomain, Input certDER,
               Time time, EndEntityOrCA endEntityOrCA,
               KeyUsage requiredKeyUsageIfPresent,
               KeyPurposeId requiredEKUIfPresent,
               const CertPolicyId& requiredPolicy,
               /*optional*/ const Input* stapledOCSPResponse)
{
  // XXX: Support the legacy use of the subject CN field for indicating the
  // domain name the certificate is valid for.
  BackCert cert(certDER, endEntityOrCA, nullptr);
  Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }

  // See bug 1056341 for context. If mozilla::pkix is being used in an
  // environment where there are many certificates that all have the same
  // distinguished name as their subject and issuer (but different SPKIs - see
  // the loop prevention as per RFC4158 Section 5.2 in PathBuildingStep::Check),
  // the space to search becomes exponential. Because it would be prohibitively
  // expensive to explore the entire space, we introduce a budget here that,
  // when exhausted, terminates the search with the result
  // Result::ERROR_UNKNOWN_ISSUER. Essentially, we limit the total number of
  // times `BuildForward` can be called. The current value appears to be a good
  // balance between finding a path when one exists (when the space isn't too
  // large) and timing out quickly enough when the space is too large or there
  // is no valid path to a trust anchor.
  unsigned int buildForwardCallBudget = 200000;
  return BuildForward(trustDomain, cert, time, requiredKeyUsageIfPresent,
                      requiredEKUIfPresent, requiredPolicy, stapledOCSPResponse,
                      0/*subCACount*/, buildForwardCallBudget);
}

} } // namespace mozilla::pkix
