/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
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

#include "pkix/bind.h"
#include "pkixutil.h"

namespace mozilla { namespace pkix {

Result
BackCert::Init()
{
  Result rv;

  // Certificate  ::=  SEQUENCE  {
  //         tbsCertificate       TBSCertificate,
  //         signatureAlgorithm   AlgorithmIdentifier,
  //         signatureValue       BIT STRING  }

  Input tbsCertificate;

  // The scope of |input| and |certificate| are limited to this block so we
  // don't accidentally confuse them for tbsCertificate later.
  {
    Input input;
    rv = input.Init(der.data, der.len);
    if (rv != Success) {
      return rv;
    }
    Input certificate;
    rv = der::ExpectTagAndGetValue(input, der::SEQUENCE, certificate);
    if (rv != Success) {
      return rv;
    }
    rv = der::End(input);
    if (rv != Success) {
      return rv;
    }
    rv = der::SignedData(certificate, tbsCertificate, signedData);
    if (rv != Success) {
      return rv;
    }
    rv = der::End(certificate);
    if (rv != Success) {
      return rv;
    }
  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                           -- If present, version MUST be v2 or v3
  //      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //                           -- If present, version MUST be v2 or v3
  //      extensions      [3]  EXPLICIT Extensions OPTIONAL
  //                           -- If present, version MUST be v3
  //      }
  rv = der::OptionalVersion(tbsCertificate, version);
  if (rv != Success) {
    return rv;
  }
  rv = der::CertificateSerialNumber(tbsCertificate, serialNumber);
  if (rv != Success) {
    return rv;
  }
  // XXX: Ignored. What are we supposed to check? This seems totally redundant
  // with Certificate.signatureAlgorithm. Is it important to check that they
  // are consistent with each other? It doesn't seem to matter!
  SignatureAlgorithm signature;
  rv = der::SignatureAlgorithmIdentifier(tbsCertificate, signature);
  if (rv != Success) {
    return rv;
  }
  rv = der::ExpectTagAndGetTLV(tbsCertificate, der::SEQUENCE, issuer);
  if (rv != Success) {
    return rv;
  }
  rv = der::ExpectTagAndGetValue(tbsCertificate, der::SEQUENCE, validity);
  if (rv != Success) {
    return rv;
  }
  // TODO(bug XXXXXXX): We rely on the the caller of mozilla::pkix to validate
  // that the name is syntactically valid, if they care. In Gecko we do this
  // implicitly by parsing the certificate into a CERTCertificate object.
  // Instead of relying on the caller to do this, we should do it ourselves.
  rv = der::ExpectTagAndGetTLV(tbsCertificate, der::SEQUENCE, subject);
  if (rv != Success) {
    return rv;
  }
  // TODO(bug XXXXXXX): We defer parsing/validating subjectPublicKeyInfo to
  // the point where the public key is needed. For end-entity certificates, we
  // assume that the caller will extract the public key and use it somehow; if
  // they don't do that then we'll never know whether the key is invalid. On
  // the other hand, if the caller never uses the key then in some ways it
  // doesn't matter. Regardless, we should parse and validate
  // subjectPublicKeyKeyInfo internally.
  rv = der::ExpectTagAndGetTLV(tbsCertificate, der::SEQUENCE,
                               subjectPublicKeyInfo);
  if (rv != Success) {
    return rv;
  }

  static const uint8_t CSC = der::CONTEXT_SPECIFIC | der::CONSTRUCTED;

  // RFC 5280 says: "These fields MUST only appear if the version is 2 or 3
  // (Section 4.1.2.1). These fields MUST NOT appear if the version is 1."
  if (version != der::Version::v1) {

    // Ignore issuerUniqueID if present.
    if (tbsCertificate.Peek(CSC | 1)) {
      rv = der::ExpectTagAndSkipValue(tbsCertificate, CSC | 1);
      if (rv != Success) {
        return rv;
      }
    }

    // Ignore subjectUniqueID if present.
    if (tbsCertificate.Peek(CSC | 2)) {
      rv = der::ExpectTagAndSkipValue(tbsCertificate, CSC | 2);
      if (rv != Success) {
        return rv;
      }
    }
  }

  // Extensions were added in v3, so only accept extensions in v3 certificates.
  if (version == der::Version::v3) {
    rv = der::OptionalExtensions(tbsCertificate, CSC | 3,
                                 bind(&BackCert::RememberExtension, this, _1,
                                      _2, _3));
    if (rv != Success) {
      return rv;
    }
  }

  return der::End(tbsCertificate);
}

Result
BackCert::RememberExtension(Input& extnID, const SECItem& extnValue,
                            /*out*/ bool& understood)
{
  understood = false;

  // python DottedOIDToCode.py id-ce-keyUsage 2.5.29.15
  static const uint8_t id_ce_keyUsage[] = {
    0x55, 0x1d, 0x0f
  };
  // python DottedOIDToCode.py id-ce-subjectAltName 2.5.29.17
  static const uint8_t id_ce_subjectAltName[] = {
    0x55, 0x1d, 0x11
  };
  // python DottedOIDToCode.py id-ce-basicConstraints 2.5.29.19
  static const uint8_t id_ce_basicConstraints[] = {
    0x55, 0x1d, 0x13
  };
  // python DottedOIDToCode.py id-ce-nameConstraints 2.5.29.30
  static const uint8_t id_ce_nameConstraints[] = {
    0x55, 0x1d, 0x1e
  };
  // python DottedOIDToCode.py id-ce-certificatePolicies 2.5.29.32
  static const uint8_t id_ce_certificatePolicies[] = {
    0x55, 0x1d, 0x20
  };
  // python DottedOIDToCode.py id-ce-policyConstraints 2.5.29.36
  static const uint8_t id_ce_policyConstraints[] = {
    0x55, 0x1d, 0x24
  };
  // python DottedOIDToCode.py id-ce-extKeyUsage 2.5.29.37
  static const uint8_t id_ce_extKeyUsage[] = {
    0x55, 0x1d, 0x25
  };
  // python DottedOIDToCode.py id-ce-inhibitAnyPolicy 2.5.29.54
  static const uint8_t id_ce_inhibitAnyPolicy[] = {
    0x55, 0x1d, 0x36
  };
  // python DottedOIDToCode.py id-pe-authorityInfoAccess 1.3.6.1.5.5.7.1.1
  static const uint8_t id_pe_authorityInfoAccess[] = {
    0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01
  };

  SECItem* out = nullptr;

  // We already enforce the maximum possible constraints for policies so we
  // can safely ignore even critical policy constraint extensions.
  //
  // XXX: Doing it this way won't allow us to detect duplicate
  // policyConstraints extensions, but that's OK because (and only because) we
  // ignore the extension.
  SECItem dummyPolicyConstraints = { siBuffer, nullptr, 0 };

  // RFC says "Conforming CAs MUST mark this extension as non-critical" for
  // both authorityKeyIdentifier and subjectKeyIdentifier, and we do not use
  // them for anything, so we totally ignore them here.

  if (extnID.MatchRest(id_ce_keyUsage)) {
    out = &keyUsage;
  } else if (extnID.MatchRest(id_ce_subjectAltName)) {
    out = &subjectAltName;
  } else if (extnID.MatchRest(id_ce_basicConstraints)) {
    out = &basicConstraints;
  } else if (extnID.MatchRest(id_ce_nameConstraints)) {
    out = &nameConstraints;
  } else if (extnID.MatchRest(id_ce_certificatePolicies)) {
    out = &certificatePolicies;
  } else if (extnID.MatchRest(id_ce_policyConstraints)) {
    out = &dummyPolicyConstraints;
  } else if (extnID.MatchRest(id_ce_extKeyUsage)) {
    out = &extKeyUsage;
  } else if (extnID.MatchRest(id_ce_inhibitAnyPolicy)) {
    out = &inhibitAnyPolicy;
  } else if (extnID.MatchRest(id_pe_authorityInfoAccess)) {
    out = &authorityInfoAccess;
  }

  if (out) {
    // Don't allow an empty value for any extension we understand. This way, we
    // can test out->len to check for duplicates.
    if (extnValue.len == 0) {
      return Result::ERROR_EXTENSION_VALUE_INVALID;
    }
    if (out->len != 0) {
      // Duplicate extension
      return Result::ERROR_EXTENSION_VALUE_INVALID;
    }
    *out = extnValue;
    understood = true;
  }

  return Success;
}

} } // namespace mozilla::pkix
