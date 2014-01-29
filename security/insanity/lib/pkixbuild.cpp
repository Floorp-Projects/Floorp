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

#include <limits>

#include "pkixcheck.h"
#include "pkixder.h"

namespace insanity { namespace pkix {

// We assume ext has been zero-initialized by its constructor and otherwise
// not modified.
//
// TODO(perf): This sorting of extensions should be be moved into the
// certificate decoder so that the results are cached with the certificate, so
// that the decoding doesn't have to happen more than once per cert.
Result
BackCert::Init()
{
  const CERTCertExtension* const* exts = nssCert->extensions;
  if (!exts) {
    return Success;
  }

  const SECItem* dummyEncodedSubjectKeyIdentifier = nullptr;
  const SECItem* dummyEncodedAuthorityKeyIdentifier = nullptr;
  const SECItem* dummyEncodedAuthorityInfoAccess = nullptr;

  for (const CERTCertExtension* ext = *exts; ext; ext = *++exts) {
    const SECItem** out = nullptr;

    if (ext->id.len == 3 &&
        ext->id.data[0] == 0x55 && ext->id.data[1] == 0x1d) {
      // { id-ce x }
      switch (ext->id.data[2]) {
        case 14: out = &dummyEncodedSubjectKeyIdentifier; break; // bug 965136
        case 15: out = &encodedKeyUsage; break;
        case 19: out = &encodedBasicConstraints; break;
        case 35: out = &dummyEncodedAuthorityKeyIdentifier; break; // bug 965136
      }
    } else if (ext->id.len == 9 &&
               ext->id.data[0] == 0x2b && ext->id.data[1] == 0x06 &&
               ext->id.data[2] == 0x06 && ext->id.data[3] == 0x01 &&
               ext->id.data[4] == 0x05 && ext->id.data[5] == 0x05 &&
               ext->id.data[6] == 0x07 && ext->id.data[7] == 0x01) {
      // { id-pe x }
      switch (ext->id.data[8]) {
        // We should remember the value of the encoded AIA extension here, but
        // since our TrustDomain implementations get the OCSP URI using
        // CERT_GetOCSPAuthorityInfoAccessLocation, we currently don't need to.
        case 1: out = &dummyEncodedAuthorityInfoAccess; break;
      }
    } else if (ext->critical.data && ext->critical.len > 0) {
      // The only valid explicit value of the critical flag is TRUE because
      // it is defined as BOOLEAN DEFAULT FALSE, so we just assume it is true.
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

static Result BuildForward(TrustDomain& trustDomain,
                           BackCert& subject,
                           PRTime time,
                           EndEntityOrCA endEntityOrCA,
                           KeyUsages requiredKeyUsagesIfPresent,
                           unsigned int subCACount,
                           /*out*/ ScopedCERTCertList& results);

// The code that executes in the inner loop of BuildForward
static Result
BuildForwardInner(TrustDomain& trustDomain,
                  BackCert& subject,
                  PRTime time,
                  EndEntityOrCA endEntityOrCA,
                  CERTCertificate* potentialIssuerCertToDup,
                  unsigned int subCACount,
                  ScopedCERTCertList& results)
{
  PORT_Assert(potentialIssuerCertToDup);

  BackCert potentialIssuer(potentialIssuerCertToDup, &subject);
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
  for (BackCert* prev = potentialIssuer.childCert;
       !loopDetected && prev != nullptr; prev = prev->childCert) {
    if (SECITEM_ItemsAreEqual(&potentialIssuer.GetNSSCert()->derPublicKey,
                              &prev->GetNSSCert()->derPublicKey) &&
        SECITEM_ItemsAreEqual(&potentialIssuer.GetNSSCert()->derSubject,
                              &prev->GetNSSCert()->derSubject)) {
      return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER); // XXX: error code
    }
  }

  rv = CheckTimes(potentialIssuer.GetNSSCert(), time);
  if (rv != Success) {
    return rv;
  }

  unsigned int newSubCACount = subCACount;
  if (endEntityOrCA == MustBeCA) {
    newSubCACount = subCACount + 1;
  } else {
    PR_ASSERT(newSubCACount == 0);
  }

  rv = BuildForward(trustDomain, potentialIssuer, time, MustBeCA,
                    KU_KEY_CERT_SIGN,
                    newSubCACount, results);
  if (rv != Success) {
    return rv;
  }

  if (trustDomain.VerifySignedData(&subject.GetNSSCert()->signatureWrap,
                                   potentialIssuer.GetNSSCert()) != SECSuccess) {
    return MapSECStatus(SECFailure);
  }

  return Success;
}

// Caller must check for expiration before calling this function
static Result
BuildForward(TrustDomain& trustDomain,
             BackCert& subject,
             PRTime time,
             EndEntityOrCA endEntityOrCA,
             KeyUsages requiredKeyUsagesIfPresent,
             unsigned int subCACount,
             /*out*/ ScopedCERTCertList& results)
{
  // Avoid stack overflows and poor performance by limiting cert length.
  // XXX: 6 is not enough for chains.sh anypolicywithlevel.cfg tests
  static const size_t MAX_DEPTH = 8;
  if (subCACount >= MAX_DEPTH - 1) {
    return RecoverableError;
  }

  TrustDomain::TrustLevel trustLevel;
  Result rv = MapSECStatus(trustDomain.GetCertTrust(endEntityOrCA,
                                                    subject.GetNSSCert(),
                                                    &trustLevel));
  if (rv != Success) {
    return rv;
  }
  if (trustLevel == TrustDomain::ActivelyDistrusted) {
    return Fail(RecoverableError, SEC_ERROR_UNTRUSTED_CERT);
  }
  if (trustLevel != TrustDomain::TrustAnchor &&
      trustLevel != TrustDomain::InheritsTrust) {
    // The TrustDomain returned a trust level that we weren't expecting.
    return Fail(FatalError, PR_INVALID_STATE_ERROR);
  }

  rv = CheckExtensions(subject, endEntityOrCA,
                       trustLevel == TrustDomain::TrustAnchor,
                       requiredKeyUsagesIfPresent,
                       subCACount);
  if (rv != Success) {
    return rv;
  }

  if (trustLevel == TrustDomain::TrustAnchor) {
    // End of the recursion. Create the result list and add the trust anchor to
    // it.
    results = CERT_NewCertList();
    if (!results) {
      return FatalError;
    }
    rv = subject.PrependNSSCertToList(results.get());
    return rv;
  }

  // Find a trusted issuer.
  // TODO(bug 965136): Add SKI/AKI matching optimizations
  ScopedCERTCertList candidates;
  if (trustDomain.FindPotentialIssuers(&subject.GetNSSCert()->derIssuer, time,
                                       candidates) != SECSuccess) {
    return MapSECStatus(SECFailure);
  }
  PORT_Assert(candidates.get());
  if (!candidates) {
    return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER);
  }

  for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
       !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
    rv = BuildForwardInner(trustDomain, subject, time, endEntityOrCA,
                           n->cert, subCACount, results);
    if (rv == Success) {
      // We found a trusted issuer. At this point, we know the cert is valid
      return subject.PrependNSSCertToList(results.get());
    }
    if (rv != RecoverableError) {
      return rv;
    }
  }

  return Fail(RecoverableError, SEC_ERROR_UNKNOWN_ISSUER);
}

SECStatus
BuildCertChain(TrustDomain& trustDomain,
               CERTCertificate* certToDup,
               PRTime time,
               /*optional*/ KeyUsages requiredKeyUsagesIfPresent,
               /*out*/ ScopedCERTCertList& results)
{
  PORT_Assert(certToDup);

  if (!certToDup) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  // The only non-const operation on the cert we are allowed to do is
  // CERT_DupCertificate.

  BackCert ee(certToDup, nullptr);
  Result rv = ee.Init();
  if (rv != Success) {
    return SECFailure;
  }

  rv = BuildForward(trustDomain, ee, time, MustBeEndEntity,
                    requiredKeyUsagesIfPresent,
                    0, results);
  if (rv != Success) {
    results = nullptr;
    return SECFailure;
  }

  // Build the cert chain even if the cert is expired, because we would
  // rather report the untrusted issuer error than the expired error.
  if (CheckTimes(ee.GetNSSCert(), time) != Success) {
    PR_SetError(SEC_ERROR_EXPIRED_CERTIFICATE, 0);
    return SECFailure;
  }

  return SECSuccess;
}

PLArenaPool*
BackCert::GetArena()
{
  if (!arena) {
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  }
  return arena.get();
}

Result
BackCert::PrependNSSCertToList(CERTCertList* results)
{
  PORT_Assert(results);

  CERTCertificate* dup = CERT_DupCertificate(nssCert);
  if (CERT_AddCertToListHead(results, dup) != SECSuccess) { // takes ownership
    CERT_DestroyCertificate(dup);
    return FatalError;
  }

  return Success;
}

} } // namespace insanity::pkix
