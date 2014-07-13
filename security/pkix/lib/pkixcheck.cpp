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

#include "cert.h"
#include "pkix/bind.h"
#include "pkix/pkix.h"
#include "pkix/ScopedPtr.h"
#include "pkixcheck.h"
#include "pkixder.h"
#include "pkixutil.h"

namespace mozilla { namespace pkix {

Result
CheckValidity(const SECItem& encodedValidity, PRTime time)
{
  Input validity;
  if (validity.Init(encodedValidity.data, encodedValidity.len) != Success) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }
  PRTime notBefore;
  if (der::TimeChoice(validity, notBefore) != Success) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }
  if (time < notBefore) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }

  PRTime notAfter;
  if (der::TimeChoice(validity, notAfter) != Success) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }
  if (time > notAfter) {
    return Fail(RecoverableError, SEC_ERROR_EXPIRED_CERTIFICATE);
  }

  return der::End(validity);
}

// 4.2.1.3. Key Usage (id-ce-keyUsage)

// As explained in the comment in CheckKeyUsage, bit 0 is the most significant
// bit and bit 7 is the least significant bit.
inline uint8_t KeyUsageToBitMask(KeyUsage keyUsage)
{
  PR_ASSERT(keyUsage != KeyUsage::noParticularKeyUsageRequired);
  return 0x80u >> static_cast<uint8_t>(keyUsage);
}

Result
CheckKeyUsage(EndEntityOrCA endEntityOrCA, const SECItem* encodedKeyUsage,
              KeyUsage requiredKeyUsageIfPresent)
{
  if (!encodedKeyUsage) {
    // TODO(bug 970196): Reject certificates that are being used to verify
    // certificate signatures unless the certificate is a trust anchor, to
    // reduce the chances of an end-entity certificate being abused as a CA
    // certificate.
    // if (endEntityOrCA == EndEntityOrCA::MustBeCA && !isTrustAnchor) {
    //   return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    // }
    //
    // TODO: Users may configure arbitrary certificates as trust anchors, not
    // just roots. We should only allow a certificate without a key usage to be
    // used as a CA when it is self-issued and self-signed.
    return Success;
  }

  Input input;
  if (input.Init(encodedKeyUsage->data, encodedKeyUsage->len) != Success) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }
  Input value;
  if (der::ExpectTagAndGetValue(input, der::BIT_STRING, value) != Success) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }

  uint8_t numberOfPaddingBits;
  if (value.Read(numberOfPaddingBits) != Success) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }
  if (numberOfPaddingBits > 7) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }

  uint8_t bits;
  if (value.Read(bits) != Success) {
    // Reject empty bit masks.
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }

  // The most significant bit is numbered 0 (digitalSignature) and the least
  // significant bit is numbered 7 (encipherOnly), and the padding is in the
  // least significant bits of the last byte. The numbering of bits in a byte
  // is backwards from how we usually interpret them.
  //
  // For example, let's say bits is encoded in one byte with of value 0xB0 and
  // numberOfPaddingBits == 4. Then, bits is 10110000 in binary:
  //
  //      bit 0  bit 3
  //          |  |
  //          v  v
  //          10110000
  //              ^^^^
  //               |
  //               4 padding bits
  //
  // Since bits is the last byte, we have to consider the padding by ensuring
  // that the least significant 4 bits are all zero, since DER rules require
  // all padding bits to be zero. Then we have to look at the bit N bits to the
  // right of the most significant bit, where N is a value from the KeyUsage
  // enumeration.
  //
  // Let's say we're interested in the keyCertSign (5) bit. We'd need to look
  // at bit 5, which is zero, so keyCertSign is not asserted. (Since we check
  // that the padding is all zeros, it is OK to read from the padding bits.)
  //
  // Let's say we're interested in the digitalSignature (0) bit. We'd need to
  // look at the bit 0 (the most significant bit), which is set, so that means
  // digitalSignature is asserted. Similarly, keyEncipherment (2) and
  // dataEncipherment (3) are asserted.
  //
  // Note that since the KeyUsage enumeration is limited to values 0-7, we
  // only ever need to examine the first byte test for
  // requiredKeyUsageIfPresent.

  if (requiredKeyUsageIfPresent != KeyUsage::noParticularKeyUsageRequired) {
    // Check that the required key usage bit is set.
    if ((bits & KeyUsageToBitMask(requiredKeyUsageIfPresent)) == 0) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    }
  }

  if (endEntityOrCA != EndEntityOrCA::MustBeCA) {
    // RFC 5280 says "The keyCertSign bit is asserted when the subject public
    // key is used for verifying signatures on public key certificates. If the
    // keyCertSign bit is asserted, then the cA bit in the basic constraints
    // extension (Section 4.2.1.9) MUST also be asserted."
    if ((bits & KeyUsageToBitMask(KeyUsage::keyCertSign)) != 0) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    }
  }

  // The padding applies to the last byte, so skip to the last byte.
  while (!value.AtEnd()) {
    if (value.Read(bits) != Success) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
    }
  }

  // All of the padding bits must be zero, according to DER rules.
  uint8_t paddingMask = static_cast<uint8_t>((1 << numberOfPaddingBits) - 1);
  if ((bits & paddingMask) != 0) {
    return Fail(RecoverableError, SEC_ERROR_INADEQUATE_KEY_USAGE);
  }

  return Success;
}

// RFC5820 4.2.1.4. Certificate Policies

// "The user-initial-policy-set contains the special value any-policy if the
// user is not concerned about certificate policy."
//
// id-ce OBJECT IDENTIFIER  ::=  {joint-iso-ccitt(2) ds(5) 29}
// id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
// anyPolicy OBJECT IDENTIFIER ::= { id-ce-certificatePolicies 0 }

/*static*/ const CertPolicyId CertPolicyId::anyPolicy = {
  4, { (40*2)+5, 29, 32, 0 }
};

bool CertPolicyId::IsAnyPolicy() const
{
  return this == &anyPolicy ||
         (numBytes == anyPolicy.numBytes &&
          !memcmp(bytes, anyPolicy.bytes, anyPolicy.numBytes));
}

// PolicyInformation ::= SEQUENCE {
//         policyIdentifier   CertPolicyId,
//         policyQualifiers   SEQUENCE SIZE (1..MAX) OF
//                                 PolicyQualifierInfo OPTIONAL }
inline Result
CheckPolicyInformation(Input& input, EndEntityOrCA endEntityOrCA,
                       const CertPolicyId& requiredPolicy,
                       /*in/out*/ bool& found)
{
  if (input.MatchTLV(der::OIDTag, requiredPolicy.numBytes,
                     requiredPolicy.bytes)) {
    found = true;
  } else if (endEntityOrCA == EndEntityOrCA::MustBeCA &&
             input.MatchTLV(der::OIDTag, CertPolicyId::anyPolicy.numBytes,
                            CertPolicyId::anyPolicy.bytes)) {
    found = true;
  }

  // RFC 5280 Section 4.2.1.4 says "Optional qualifiers, which MAY be present,
  // are not expected to change the definition of the policy." Also, it seems
  // that Section 6, which defines validation, does not require any matching of
  // qualifiers. Thus, doing anything with the policy qualifiers would be a
  // waste of time and a source of potential incompatibilities, so we just
  // ignore them.

  // Skip unmatched OID and/or policyQualifiers
  input.SkipToEnd();

  return Success;
}

// certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
Result
CheckCertificatePolicies(EndEntityOrCA endEntityOrCA,
                         const SECItem* encodedCertificatePolicies,
                         const SECItem* encodedInhibitAnyPolicy,
                         TrustLevel trustLevel,
                         const CertPolicyId& requiredPolicy)
{
  if (requiredPolicy.numBytes == 0 ||
      requiredPolicy.numBytes > sizeof requiredPolicy.bytes) {
    return Fail(FatalError, SEC_ERROR_INVALID_ARGS);
  }

  // Ignore all policy information if the caller indicates any policy is
  // acceptable. See TrustDomain::GetCertTrust and the policy part of
  // BuildCertChain's documentation.
  if (requiredPolicy.IsAnyPolicy()) {
    return Success;
  }

  // Bug 989051. Until we handle inhibitAnyPolicy we will fail close when
  // inhibitAnyPolicy extension is present and we need to evaluate certificate
  // policies.
  if (encodedInhibitAnyPolicy) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }

  // The root CA certificate may omit the policies that it has been
  // trusted for, so we cannot require the policies to be present in those
  // certificates. Instead, the determination of which roots are trusted for
  // which policies is made by the TrustDomain's GetCertTrust method.
  if (trustLevel == TrustLevel::TrustAnchor &&
      endEntityOrCA == EndEntityOrCA::MustBeCA) {
    return Success;
  }

  if (!encodedCertificatePolicies) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }

  bool found = false;

  Input input;
  if (input.Init(encodedCertificatePolicies->data,
                 encodedCertificatePolicies->len) != Success) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }
  if (der::NestedOf(input, der::SEQUENCE, der::SEQUENCE, der::EmptyAllowed::No,
                    bind(CheckPolicyInformation, _1, endEntityOrCA,
                         requiredPolicy, ref(found))) != Success) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }
  if (der::End(input) != Success) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }
  if (!found) {
    return Fail(RecoverableError, SEC_ERROR_POLICY_VALIDATION_FAILED);
  }

  return Success;
}

static const long UNLIMITED_PATH_LEN = -1; // must be less than zero

//  BasicConstraints ::= SEQUENCE {
//          cA                      BOOLEAN DEFAULT FALSE,
//          pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
static Result
DecodeBasicConstraints(Input& input, /*out*/ bool& isCA,
                       /*out*/ long& pathLenConstraint)
{
  // TODO(bug 989518): cA is by default false. According to DER, default
  // values must not be explicitly encoded in a SEQUENCE. So, if this
  // value is present and false, it is an encoding error. However, Go Daddy
  // has issued many certificates with this improper encoding, so we can't
  // enforce this yet (hence passing true for allowInvalidExplicitEncoding
  // to der::OptionalBoolean).
  if (der::OptionalBoolean(input, true, isCA) != Success) {
    return Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }

  // TODO(bug 985025): If isCA is false, pathLenConstraint MUST NOT
  // be included (as per RFC 5280 section 4.2.1.9), but for compatibility
  // reasons, we don't check this for now.
  if (der::OptionalInteger(input, UNLIMITED_PATH_LEN, pathLenConstraint)
        != Success) {
    return Fail(SEC_ERROR_EXTENSION_VALUE_INVALID);
  }

  return Success;
}

// RFC5280 4.2.1.9. Basic Constraints (id-ce-basicConstraints)
Result
CheckBasicConstraints(EndEntityOrCA endEntityOrCA,
                      const SECItem* encodedBasicConstraints,
                      const der::Version version, TrustLevel trustLevel,
                      unsigned int subCACount)
{
  bool isCA = false;
  long pathLenConstraint = UNLIMITED_PATH_LEN;

  if (encodedBasicConstraints) {
    Input input;
    if (input.Init(encodedBasicConstraints->data,
                   encodedBasicConstraints->len) != Success) {
      return Fail(RecoverableError, SEC_ERROR_EXTENSION_VALUE_INVALID);
    }
    if (der::Nested(input, der::SEQUENCE,
                    bind(DecodeBasicConstraints, _1, ref(isCA),
                         ref(pathLenConstraint))) != Success) {
      return Fail(RecoverableError, SEC_ERROR_EXTENSION_VALUE_INVALID);
    }
    if (der::End(input) != Success) {
      return Fail(RecoverableError, SEC_ERROR_EXTENSION_VALUE_INVALID);
    }
  } else {
    // "If the basic constraints extension is not present in a version 3
    //  certificate, or the extension is present but the cA boolean is not
    //  asserted, then the certified public key MUST NOT be used to verify
    //  certificate signatures."
    //
    // For compatibility, we must accept v1 trust anchors without basic
    // constraints as CAs.
    //
    // TODO: add check for self-signedness?
    if (endEntityOrCA == EndEntityOrCA::MustBeCA &&
        trustLevel == TrustLevel::TrustAnchor && version == der::Version::v1) {
      isCA = true;
    }
  }

  if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity) {
    // CA certificates are not trusted as EE certs.

    if (isCA) {
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

  PORT_Assert(endEntityOrCA == EndEntityOrCA::MustBeCA);

  // End-entity certificates are not allowed to act as CA certs.
  if (!isCA) {
    return Fail(RecoverableError, SEC_ERROR_CA_CERT_INVALID);
  }

  if (pathLenConstraint >= 0 &&
      static_cast<long>(subCACount) > pathLenConstraint) {
    return Fail(RecoverableError, SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID);
  }

  return Success;
}

// 4.2.1.10. Name Constraints

inline void
PORT_FreeArena_false(PLArenaPool* arena) {
  // PL_FreeArenaPool can't be used because it doesn't actually free the
  // memory, which doesn't work well with memory analysis tools
  return PORT_FreeArena(arena, PR_FALSE);
}

Result
CheckNameConstraints(const SECItem& encodedNameConstraints,
                     const BackCert& firstChild,
                     KeyPurposeId requiredEKUIfPresent)
{
  ScopedPtr<PLArenaPool, PORT_FreeArena_false>
    arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return MapSECStatus(SECFailure);
  }

  // Owned by arena
  const CERTNameConstraints* constraints =
    CERT_DecodeNameConstraintsExtension(arena.get(), &encodedNameConstraints);
  if (!constraints) {
    return MapSECStatus(SECFailure);
  }

  for (const BackCert* child = &firstChild; child; child = child->childCert) {
    ScopedPtr<CERTCertificate, CERT_DestroyCertificate>
      nssCert(CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                      const_cast<SECItem*>(&child->GetDER()),
                                      nullptr, false, true));
    if (!nssCert) {
      return MapSECStatus(SECFailure);
    }

    bool includeCN = child->endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
                     requiredEKUIfPresent == KeyPurposeId::id_kp_serverAuth;
    // owned by arena
    const CERTGeneralName*
      names(CERT_GetConstrainedCertificateNames(nssCert.get(), arena.get(),
                                                includeCN));
    if (!names) {
      return MapSECStatus(SECFailure);
    }

    CERTGeneralName* currentName = const_cast<CERTGeneralName*>(names);
    do {
      if (CERT_CheckNameSpace(arena.get(), constraints, currentName)
            != SECSuccess) {
        // XXX: It seems like CERT_CheckNameSpace doesn't always call
        // PR_SetError when it fails. We set the error code here, though this
        // may be papering over some fatal errors. NSS's
        // cert_VerifyCertChainOld does something similar.
        return Fail(RecoverableError, SEC_ERROR_CERT_NOT_IN_NAME_SPACE);
      }
      currentName = CERT_GetNextGeneralName(currentName);
    } while (currentName != names);
  }

  return Success;
}

// 4.2.1.12. Extended Key Usage (id-ce-extKeyUsage)

static Result
MatchEKU(Input& value, KeyPurposeId requiredEKU,
         EndEntityOrCA endEntityOrCA, /*in/out*/ bool& found,
         /*in/out*/ bool& foundOCSPSigning)
{
  // See Section 5.9 of "A Layman's Guide to a Subset of ASN.1, BER, and DER"
  // for a description of ASN.1 DER encoding of OIDs.

  // id-pkix  OBJECT IDENTIFIER  ::=
  //            { iso(1) identified-organization(3) dod(6) internet(1)
  //                    security(5) mechanisms(5) pkix(7) }
  // id-kp OBJECT IDENTIFIER ::= { id-pkix 3 }
  // id-kp-serverAuth      OBJECT IDENTIFIER ::= { id-kp 1 }
  // id-kp-clientAuth      OBJECT IDENTIFIER ::= { id-kp 2 }
  // id-kp-codeSigning     OBJECT IDENTIFIER ::= { id-kp 3 }
  // id-kp-emailProtection OBJECT IDENTIFIER ::= { id-kp 4 }
  // id-kp-OCSPSigning     OBJECT IDENTIFIER ::= { id-kp 9 }
  static const uint8_t server[] = { (40*1)+3, 6, 1, 5, 5, 7, 3, 1 };
  static const uint8_t client[] = { (40*1)+3, 6, 1, 5, 5, 7, 3, 2 };
  static const uint8_t code  [] = { (40*1)+3, 6, 1, 5, 5, 7, 3, 3 };
  static const uint8_t email [] = { (40*1)+3, 6, 1, 5, 5, 7, 3, 4 };
  static const uint8_t ocsp  [] = { (40*1)+3, 6, 1, 5, 5, 7, 3, 9 };

  // id-Netscape        OBJECT IDENTIFIER ::= { 2 16 840 1 113730 }
  // id-Netscape-policy OBJECT IDENTIFIER ::= { id-Netscape 4 }
  // id-Netscape-stepUp OBJECT IDENTIFIER ::= { id-Netscape-policy 1 }
  static const uint8_t serverStepUp[] =
    { (40*2)+16, 128+6,72, 1, 128+6,128+120,66, 4, 1 };

  bool match = false;

  if (!found) {
    switch (requiredEKU) {
      case KeyPurposeId::id_kp_serverAuth:
        // Treat CA certs with step-up OID as also having SSL server type.
        // Comodo has issued certificates that require this behavior that don't
        // expire until June 2020! TODO(bug 982932): Limit this exception to
        // old certificates.
        match = value.MatchRest(server) ||
                (endEntityOrCA == EndEntityOrCA::MustBeCA &&
                 value.MatchRest(serverStepUp));
        break;

      case KeyPurposeId::id_kp_clientAuth:
        match = value.MatchRest(client);
        break;

      case KeyPurposeId::id_kp_codeSigning:
        match = value.MatchRest(code);
        break;

      case KeyPurposeId::id_kp_emailProtection:
        match = value.MatchRest(email);
        break;

      case KeyPurposeId::id_kp_OCSPSigning:
        match = value.MatchRest(ocsp);
        break;

      case KeyPurposeId::anyExtendedKeyUsage:
        PR_NOT_REACHED("anyExtendedKeyUsage should start with found==true");
        return Fail(SEC_ERROR_LIBRARY_FAILURE);

      default:
        PR_NOT_REACHED("unrecognized EKU");
        return Fail(SEC_ERROR_LIBRARY_FAILURE);
    }
  }

  if (match) {
    found = true;
    if (requiredEKU == KeyPurposeId::id_kp_OCSPSigning) {
      foundOCSPSigning = true;
    }
  } else if (value.MatchRest(ocsp)) {
    foundOCSPSigning = true;
  }

  value.SkipToEnd(); // ignore unmatched OIDs.

  return Success;
}

Result
CheckExtendedKeyUsage(EndEntityOrCA endEntityOrCA,
                      const SECItem* encodedExtendedKeyUsage,
                      KeyPurposeId requiredEKU)
{
  // XXX: We're using SEC_ERROR_INADEQUATE_CERT_TYPE here so that callers can
  // distinguish EKU mismatch from KU mismatch from basic constraints mismatch.
  // We should probably add a new error code that is more clear for this type
  // of problem.

  bool foundOCSPSigning = false;

  if (encodedExtendedKeyUsage) {
    bool found = requiredEKU == KeyPurposeId::anyExtendedKeyUsage;

    Input input;
    if (input.Init(encodedExtendedKeyUsage->data,
                   encodedExtendedKeyUsage->len) != Success) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }
    if (der::NestedOf(input, der::SEQUENCE, der::OIDTag, der::EmptyAllowed::No,
                      bind(MatchEKU, _1, requiredEKU, endEntityOrCA,
                           ref(found), ref(foundOCSPSigning)))
          != Success) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }
    if (der::End(input) != Success) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }

    // If the EKU extension was included, then the required EKU must be in the
    // list.
    if (!found) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }
  }

  // pkixocsp.cpp depends on the following additional checks.

  if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity) {
    // When validating anything other than an delegated OCSP signing cert,
    // reject any cert that also claims to be an OCSP responder, because such
    // a cert does not make sense. For example, if an SSL certificate were to
    // assert id-kp-OCSPSigning then it could sign OCSP responses for itself,
    // if not for this check.
    // That said, we accept CA certificates with id-kp-OCSPSigning because
    // some CAs in Mozilla's CA program have issued such intermediate
    // certificates, and because some CAs have reported some Microsoft server
    // software wrongly requires CA certificates to have id-kp-OCSPSigning.
    // Allowing this exception does not cause any security issues because we
    // require delegated OCSP response signing certificates to be end-entity
    // certificates.
    if (foundOCSPSigning && requiredEKU != KeyPurposeId::id_kp_OCSPSigning) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }
    // http://tools.ietf.org/html/rfc6960#section-4.2.2.2:
    // "OCSP signing delegation SHALL be designated by the inclusion of
    // id-kp-OCSPSigning in an extended key usage certificate extension
    // included in the OCSP response signer's certificate."
    //
    // id-kp-OCSPSigning is the only EKU that isn't implicitly assumed when the
    // EKU extension is missing from an end-entity certificate. However, any CA
    // certificate can issue a delegated OCSP response signing certificate, so
    // we can't require the EKU be explicitly included for CA certificates.
    if (!foundOCSPSigning && requiredEKU == KeyPurposeId::id_kp_OCSPSigning) {
      return Fail(RecoverableError, SEC_ERROR_INADEQUATE_CERT_TYPE);
    }
  }

  return Success;
}

Result
CheckIssuerIndependentProperties(TrustDomain& trustDomain,
                                 const BackCert& cert,
                                 PRTime time,
                                 KeyUsage requiredKeyUsageIfPresent,
                                 KeyPurposeId requiredEKUIfPresent,
                                 const CertPolicyId& requiredPolicy,
                                 unsigned int subCACount,
                /*optional out*/ TrustLevel* trustLevelOut)
{
  Result rv;

  const EndEntityOrCA endEntityOrCA = cert.endEntityOrCA;

  TrustLevel trustLevel;
  rv = MapSECStatus(trustDomain.GetCertTrust(endEntityOrCA, requiredPolicy,
                                             cert.GetDER(), &trustLevel));
  if (rv != Success) {
    return rv;
  }
  if (trustLevel == TrustLevel::ActivelyDistrusted) {
    return Fail(RecoverableError, SEC_ERROR_UNTRUSTED_CERT);
  }
  if (trustLevel != TrustLevel::TrustAnchor &&
      trustLevel != TrustLevel::InheritsTrust) {
    // The TrustDomain returned a trust level that we weren't expecting.
    PORT_SetError(PR_INVALID_STATE_ERROR);
    return FatalError;
  }
  if (trustLevelOut) {
    *trustLevelOut = trustLevel;
  }

  // 4.2.1.1. Authority Key Identifier is ignored (see bug 965136).

  // 4.2.1.2. Subject Key Identifier is ignored (see bug 965136).

  // 4.2.1.3. Key Usage
  rv = CheckKeyUsage(endEntityOrCA, cert.GetKeyUsage(),
                     requiredKeyUsageIfPresent);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.4. Certificate Policies
  rv = CheckCertificatePolicies(endEntityOrCA, cert.GetCertificatePolicies(),
                                cert.GetInhibitAnyPolicy(), trustLevel,
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
  rv = CheckBasicConstraints(endEntityOrCA, cert.GetBasicConstraints(),
                             cert.GetVersion(), trustLevel, subCACount);
  if (rv != Success) {
    return rv;
  }

  // 4.2.1.10. Name Constraints is dealt with in during path building.

  // 4.2.1.11. Policy Constraints are implicitly supported; see the
  //           documentation about policy enforcement in pkix.h.

  // 4.2.1.12. Extended Key Usage
  rv = CheckExtendedKeyUsage(endEntityOrCA, cert.GetExtKeyUsage(),
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
  rv = CheckValidity(cert.GetValidity(), time);
  if (rv != Success) {
    return rv;
  }

  return Success;
}

} } // namespace mozilla::pkix
