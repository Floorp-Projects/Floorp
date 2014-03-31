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

#include <limits>

#include "pkix/pkix.h"
#include "pkixcheck.h"
#include "pkixder.h"
#include "pkixutil.h"
#include "secder.h"

namespace mozilla { namespace pkix {

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

// 4.2.1.3. Key Usage (id-ce-keyUsage)
// Modeled after GetKeyUsage in certdb.c
Result
CheckKeyUsage(EndEntityOrCA endEntityOrCA,
              bool isTrustAnchor,
              const SECItem* encodedKeyUsage,
              KeyUsages requiredKeyUsagesIfPresent,
              PLArenaPool* arena)
{
  if (!encodedKeyUsage) {
    // TODO: Reject certificates that are being used to verify certificate
    // signatures unless the certificate is a trust anchor, to reduce the
    // chances of an end-entity certificate being abused as a CA certificate.
    // if (endEntityOrCA == MustBeCA && !isTrustAnchor) {
    //   return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    // }
    //
    // TODO: Users may configure arbitrary certificates as trust anchors, not
    // just roots. We should only allow a certificate without a key usage to be
    // used as a CA when it is self-issued and self-signed.
    return Success;
  }

  SECItem tmpItem;
  Result rv = MapSECStatus(SEC_QuickDERDecodeItem(arena, &tmpItem,
                              SEC_ASN1_GET(SEC_BitStringTemplate),
                              encodedKeyUsage));
  if (rv != Success) {
    return rv;
  }

  // TODO XXX: Why is tmpItem.len > 1?

  KeyUsages allowedKeyUsages = tmpItem.data[0];
  if ((allowedKeyUsages & requiredKeyUsagesIfPresent)
        != requiredKeyUsagesIfPresent) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }

  if (endEntityOrCA == MustBeCA) {
   // "If the keyUsage extension is present, then the subject public key
   //  MUST NOT be used to verify signatures on certificates or CRLs unless
   //  the corresponding keyCertSign or cRLSign bit is set."
   if ((allowedKeyUsages & KU_KEY_CERT_SIGN) == 0) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    }
  } else {
    // "The keyCertSign bit is asserted when the subject public key is
    //  used for verifying signatures on public key certificates.  If the
    //  keyCertSign bit is asserted, then the cA bit in the basic
    //  constraints extension (Section 4.2.1.9) MUST also be asserted."
    // TODO XXX: commented out to match classic NSS behavior.
    //if ((allowedKeyUsages & KU_KEY_CERT_SIGN) != 0) {
    //  // XXX: better error code.
    //  return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    //}
  }

  return Success;
}

// RFC5820 4.2.1.4. Certificate Policies
//
// "The user-initial-policy-set contains the special value any-policy if the
// user is not concerned about certificate policy."
Result
CheckCertificatePolicies(BackCert& cert, EndEntityOrCA endEntityOrCA,
                         bool isTrustAnchor, SECOidTag requiredPolicy)
{
  if (requiredPolicy == SEC_OID_X509_ANY_POLICY) {
    return Success;
  }

  // It is likely some callers will pass SEC_OID_UNKNOWN when they don't care,
  // instead of passing SEC_OID_X509_ANY_POLICY. Help them out by failing hard.
  if (requiredPolicy == SEC_OID_UNKNOWN) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return FatalError;
  }

  // Bug 989051. Until we handle inhibitAnyPolicy we will fail close when
  // inhibitAnyPolicy extension is present and we need to evaluate certificate
  // policies.
  if (cert.encodedInhibitAnyPolicy) {
    PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
    return RecoverableError;
  }

  // The root CA certificate may omit the policies that it has been
  // trusted for, so we cannot require the policies to be present in those
  // certificates. Instead, the determination of which roots are trusted for
  // which policies is made by the TrustDomain's GetCertTrust method.
  if (isTrustAnchor && endEntityOrCA == MustBeCA) {
    return Success;
  }

  if (!cert.encodedCertificatePolicies) {
    PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
    return RecoverableError;
  }

  ScopedPtr<CERTCertificatePolicies, CERT_DestroyCertificatePoliciesExtension>
    policies(CERT_DecodeCertificatePoliciesExtension(
                cert.encodedCertificatePolicies));
  if (!policies) {
    return MapSECStatus(SECFailure);
  }

  for (const CERTPolicyInfo* const* policyInfos = policies->policyInfos;
       *policyInfos; ++policyInfos) {
    if ((*policyInfos)->oid == requiredPolicy) {
      return Success;
    }
    // Intermediate certs are allowed to have the anyPolicy OID
    if (endEntityOrCA == MustBeCA &&
        (*policyInfos)->oid == SEC_OID_X509_ANY_POLICY) {
      return Success;
    }
  }

  PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
  return RecoverableError;
}

//  BasicConstraints ::= SEQUENCE {
//          cA                      BOOLEAN DEFAULT FALSE,
//          pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
der::Result
DecodeBasicConstraints(const SECItem* encodedBasicConstraints,
                       CERTBasicConstraints& basicConstraints)
{
  PR_ASSERT(encodedBasicConstraints);
  if (!encodedBasicConstraints) {
    return der::Fail(SEC_ERROR_INVALID_ARGS);
  }

  basicConstraints.isCA = false;
  basicConstraints.pathLenConstraint = 0;

  der::Input input;
  if (input.Init(encodedBasicConstraints->data, encodedBasicConstraints->len)
        != der::Success) {
    return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }

  if (der::ExpectTagAndIgnoreLength(input, der::SEQUENCE) != der::Success) {
    return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }

  bool isCA = false;
  if (der::OptionalBoolean(input, isCA) != der::Success) {
    return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }
  basicConstraints.isCA = isCA;

  if (input.Peek(der::INTEGER)) {
    SECItem pathLenConstraintEncoded;
    if (der::Integer(input, pathLenConstraintEncoded) != der::Success) {
      return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
    }
    long pathLenConstraint = DER_GetInteger(&pathLenConstraintEncoded);
    if (pathLenConstraint >= std::numeric_limits<int>::max() ||
        pathLenConstraint < 0) {
      return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
    }
    basicConstraints.pathLenConstraint = static_cast<int>(pathLenConstraint);
    // TODO(bug 985025): If isCA is false, pathLenConstraint MUST NOT
    // be included (as per RFC 5280 section 4.2.1.9), but for compatibility
    // reasons, we don't check this for now.
  } else if (basicConstraints.isCA) {
    // If this is a CA but the path length is omitted, it is unlimited.
    basicConstraints.pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
  }

  if (der::End(input) != der::Success) {
    return der::Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }
  return der::Success;
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
    if (DecodeBasicConstraints(cert.encodedBasicConstraints,
                               basicConstraints) != der::Success) {
      return RecoverableError;
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
      // We only allow trust anchor CA certs to omit the
      // basicConstraints extension if they are v1. v1 is encoded
      // implicitly.
      if (!nssCert->version.data && !nssCert->version.len) {
        basicConstraints.isCA = true;
        basicConstraints.pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
      }
    }
  }

  if (endEntityOrCA == MustBeEndEntity) {
    // CA certificates are not trusted as EE certs.

    if (basicConstraints.isCA) {
      // XXX: We use SEC_ERROR_CA_CERT_INVALID here so we can distinguish
      // this error from other errors, given that NSS does not have a "CA cert
      // used as end-entity" error code since it doesn't have such a
      // prohibition. We should add such an error code and stop abusing
      // SEC_ERROR_CA_CERT_INVALID this way.
      //
      // Note, in particular, that this check prevents a delegated OCSP
      // response signing certificate with the CA bit from successfully
      // validating when we check it from pkixocsp.cpp, which is a good thing.
      //
      return Fail(RecoverableError, SEC_ERROR_CA_CERT_INVALID);
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

Result
BackCert::GetConstrainedNames(/*out*/ const CERTGeneralName** result)
{
  if (!constrainedNames) {
    if (!GetArena()) {
      return FatalError;
    }

    constrainedNames =
      CERT_GetConstrainedCertificateNames(nssCert, arena.get(),
                                          cnOptions == IncludeCN);
    if (!constrainedNames) {
      return MapSECStatus(SECFailure);
    }
  }

  *result = constrainedNames;
  return Success;
}

// 4.2.1.10. Name Constraints
Result
CheckNameConstraints(BackCert& cert)
{
  if (!cert.encodedNameConstraints) {
    return Success;
  }

  PLArenaPool* arena = cert.GetArena();
  if (!arena) {
    return FatalError;
  }

  // Owned by arena
  const CERTNameConstraints* constraints =
    CERT_DecodeNameConstraintsExtension(arena, cert.encodedNameConstraints);
  if (!constraints) {
    return MapSECStatus(SECFailure);
  }

  for (BackCert* prev = cert.childCert; prev; prev = prev->childCert) {
    const CERTGeneralName* names = nullptr;
    Result rv = prev->GetConstrainedNames(&names);
    if (rv != Success) {
      return rv;
    }
    PORT_Assert(names);
    CERTGeneralName* currentName = const_cast<CERTGeneralName*>(names);
    do {
      if (CERT_CheckNameSpace(arena, constraints, currentName) != SECSuccess) {
        // XXX: It seems like CERT_CheckNameSpace doesn't always call
        // PR_SetError when it fails. We set the error code here, though this
        // may be papering over some fatal errors. NSS's
        // cert_VerifyCertChainOld does something similar.
        PR_SetError(SEC_ERROR_CERT_NOT_IN_NAME_SPACE, 0);
        return RecoverableError;
      }
      currentName = CERT_GetNextGeneralName(currentName);
    } while (currentName != names);
  }

  return Success;
}

// 4.2.1.12. Extended Key Usage (id-ce-extKeyUsage)
// 4.2.1.12. Extended Key Usage (id-ce-extKeyUsage)
Result
CheckExtendedKeyUsage(EndEntityOrCA endEntityOrCA, const SECItem* encodedEKUs,
                      SECOidTag requiredEKU)
{
  // TODO: Either do not allow anyExtendedKeyUsage to be passed as requiredEKU,
  // or require that callers pass anyExtendedKeyUsage instead of
  // SEC_OID_UNKNWON and disallow SEC_OID_UNKNWON.

  // XXX: We're using SEC_ERROR_INADEQUATE_CERT_TYPE here so that callers can
  // distinguish EKU mismatch from KU mismatch from basic constraints mismatch.
  // We should probably add a new error code that is more clear for this type
  // of problem.

  bool foundOCSPSigning = false;

  if (encodedEKUs) {
    ScopedPtr<CERTOidSequence, CERT_DestroyOidSequence>
      seq(CERT_DecodeOidSequence(encodedEKUs));
    if (!seq) {
      PR_SetError(SEC_ERROR_INADEQUATE_CERT_TYPE, 0);
      return RecoverableError;
    }

    bool found = false;

    // XXX: We allow duplicate entries.
    for (const SECItem* const* oids = seq->oids; oids && *oids; ++oids) {
      SECOidTag oidTag = SECOID_FindOIDTag(*oids);
      if (requiredEKU != SEC_OID_UNKNOWN && oidTag == requiredEKU) {
        found = true;
      } else {
        // Treat CA certs with step-up OID as also having SSL server type.
        // COMODO has issued certificates that require this behavior
        // that don't expire until June 2020!
        // TODO 982932: Limit this expection to old certificates
        if (endEntityOrCA == MustBeCA &&
            requiredEKU == SEC_OID_EXT_KEY_USAGE_SERVER_AUTH &&
            oidTag == SEC_OID_NS_KEY_USAGE_GOVT_APPROVED) {
          found = true;
        }
      }
      if (oidTag == SEC_OID_OCSP_RESPONDER) {
        foundOCSPSigning = true;
      }
    }

    // If the EKU extension was included, then the required EKU must be in the
    // list.
    if (!found) {
      PR_SetError(SEC_ERROR_INADEQUATE_CERT_TYPE, 0);
      return RecoverableError;
    }
  }

  // pkixocsp.cpp depends on the following additional checks.

  if (foundOCSPSigning) {
    // When validating anything other than an delegated OCSP signing cert,
    // reject any cert that also claims to be an OCSP responder, because such
    // a cert does not make sense. For example, if an SSL certificate were to
    // assert id-kp-OCSPSigning then it could sign OCSP responses for itself,
    // if not for this check.
    if (requiredEKU != SEC_OID_OCSP_RESPONDER) {
      PR_SetError(SEC_ERROR_INADEQUATE_CERT_TYPE, 0);
      return RecoverableError;
    }
  } else if (requiredEKU == SEC_OID_OCSP_RESPONDER &&
             endEntityOrCA == MustBeEndEntity) {
    // http://tools.ietf.org/html/rfc6960#section-4.2.2.2:
    // "OCSP signing delegation SHALL be designated by the inclusion of
    // id-kp-OCSPSigning in an extended key usage certificate extension
    // included in the OCSP response signer's certificate."
    //
    // id-kp-OCSPSigning is the only EKU that isn't implicitly assumed when the
    // EKU extension is missing from an end-entity certificate. However, any CA
    // certificate can issue a delegated OCSP response signing certificate, so
    // we can't require the EKU be explicitly included for CA certificates.
    PR_SetError(SEC_ERROR_INADEQUATE_CERT_TYPE, 0);
    return RecoverableError;
  }

  return Success;
}

Result
CheckIssuerIndependentProperties(TrustDomain& trustDomain,
                                 BackCert& cert,
                                 PRTime time,
                                 EndEntityOrCA endEntityOrCA,
                                 KeyUsages requiredKeyUsagesIfPresent,
                                 SECOidTag requiredEKUIfPresent,
                                 SECOidTag requiredPolicy,
                                 unsigned int subCACount,
                /*optional out*/ TrustDomain::TrustLevel* trustLevelOut)
{
  Result rv;

  TrustDomain::TrustLevel trustLevel;
  rv = MapSECStatus(trustDomain.GetCertTrust(endEntityOrCA,
                                             requiredPolicy,
                                             cert.GetNSSCert(),
                                             &trustLevel));
  if (rv != Success) {
    return rv;
  }
  if (trustLevel == TrustDomain::ActivelyDistrusted) {
    PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
    return RecoverableError;
  }
  if (trustLevel != TrustDomain::TrustAnchor &&
      trustLevel != TrustDomain::InheritsTrust) {
    // The TrustDomain returned a trust level that we weren't expecting.
    PORT_SetError(PR_INVALID_STATE_ERROR);
    return FatalError;
  }
  if (trustLevelOut) {
    *trustLevelOut = trustLevel;
  }

  bool isTrustAnchor = endEntityOrCA == MustBeCA &&
                       trustLevel == TrustDomain::TrustAnchor;

  PLArenaPool* arena = cert.GetArena();
  if (!arena) {
    return FatalError;
  }

  // 4.2.1.1. Authority Key Identifier is ignored (see bug 965136).

  // 4.2.1.2. Subject Key Identifier is ignored (see bug 965136).

  // 4.2.1.3. Key Usage
  rv = CheckKeyUsage(endEntityOrCA, isTrustAnchor, cert.encodedKeyUsage,
                     requiredKeyUsagesIfPresent, arena);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.4. Certificate Policies
  rv = CheckCertificatePolicies(cert, endEntityOrCA, isTrustAnchor,
                                requiredPolicy);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.5. Policy Mappings are not supported; see the documentation about
  //          policy enforcement in pkix.h.

  // 4.2.1.6. Subject Alternative Name dealt with during name constraint
  //          checking and during name verification (CERT_VerifyCertName).

  // 4.2.1.7. Issuer Alternative Name is not something that needs checking.

  // 4.2.1.8. Subject Directory Attributes is not something that needs
  //          checking.

  // 4.2.1.9. Basic Constraints.
  rv = CheckBasicConstraints(cert, endEntityOrCA, isTrustAnchor, subCACount);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.10. Name Constraints is dealt with in during path building.

  // 4.2.1.11. Policy Constraints are implicitly supported; see the
  //           documentation about policy enforcement in pkix.h.

  // 4.2.1.12. Extended Key Usage
  rv = CheckExtendedKeyUsage(endEntityOrCA, cert.encodedExtendedKeyUsage,
                             requiredEKUIfPresent);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.13. CRL Distribution Points is not supported, though the
  //           TrustDomain's CheckRevocation method may parse it and process it
  //           on its own.

  // 4.2.1.14. Inhibit anyPolicy is implicitly supported; see the documentation
  //           about policy enforcement in pkix.h.

  // IMPORTANT: This check must come after the other checks in order for error
  // ranking to work correctly.
  rv = CheckTimes(cert.GetNSSCert(), time);
  if (rv != Success) {
    return rv;
  }

  return Success;
}

} } // namespace mozilla::pkix
