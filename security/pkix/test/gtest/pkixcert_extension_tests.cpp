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
#include "pkixgtest.h"
#include "pkixtestutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

// Creates a self-signed certificate with the given extension.
static ByteString
CreateCert(const char* subjectCN,
           const ByteString* extensions, // empty-string-terminated array
           /*out*/ ScopedTestKeyPair& subjectKey)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));
  ByteString issuerDER(CNToDERName(subjectCN));
  EXPECT_FALSE(ENCODING_FAILED(issuerDER));
  ByteString subjectDER(CNToDERName(subjectCN));
  EXPECT_FALSE(ENCODING_FAILED(subjectDER));
  return CreateEncodedCertificate(v3, sha256WithRSAEncryption,
                                  serialNumber, issuerDER,
                                  oneDayBeforeNow, oneDayAfterNow,
                                  subjectDER, extensions,
                                  nullptr,
                                  sha256WithRSAEncryption,
                                  subjectKey);
}

// Creates a self-signed certificate with the given extension.
static ByteString
CreateCert(const char* subjectStr,
           const ByteString& extension,
           /*out*/ ScopedTestKeyPair& subjectKey)
{
  const ByteString extensions[] = { extension, ByteString() };
  return CreateCert(subjectStr, extensions, subjectKey);
}

class TrustEverythingTrustDomain : public TrustDomain
{
private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              Input candidateCert,
                              /*out*/ TrustLevel& trustLevel)
  {
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }

  virtual Result FindIssuer(Input /*encodedIssuerName*/,
                            IssuerChecker& /*checker*/, Time /*time*/)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckRevocation(EndEntityOrCA, const CertID&, Time,
                                 /*optional*/ const Input*,
                                 /*optional*/ const Input*)
  {
    return Success;
  }

  virtual Result IsChainValid(const DERArray&, Time)
  {
    return Success;
  }

  virtual Result VerifySignedData(const SignedDataWithSignature& signedData,
                                  Input subjectPublicKeyInfo)
  {
    return TestVerifySignedData(signedData, subjectPublicKeyInfo);
  }

  virtual Result DigestBuf(Input, /*out*/ uint8_t*, size_t)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(Input subjectPublicKeyInfo)
  {
    return TestCheckPublicKey(subjectPublicKeyInfo);
  }
};

class pkixcert_extension : public ::testing::Test
{
protected:
  static TrustEverythingTrustDomain trustDomain;
};

/*static*/ TrustEverythingTrustDomain pkixcert_extension::trustDomain;

// Tests that a critical extension not in the id-ce or id-pe arcs (which is
// thus unknown to us) is detected and that verification fails with the
// appropriate error.
TEST_F(pkixcert_extension, UnknownCriticalExtension)
{
  static const uint8_t unknownCriticalExtensionBytes[] = {
    0x30, 0x19, // SEQUENCE (length = 25)
      0x06, 0x12, // OID (length = 18)
        // 1.3.6.1.4.1.13769.666.666.666.1.500.9.3
        0x2b, 0x06, 0x01, 0x04, 0x01, 0xeb, 0x49, 0x85, 0x1a,
        0x85, 0x1a, 0x85, 0x1a, 0x01, 0x83, 0x74, 0x09, 0x03,
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const ByteString
    unknownCriticalExtension(unknownCriticalExtensionBytes,
                             sizeof(unknownCriticalExtensionBytes));
  const char* certCN = "Cert With Unknown Critical Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, unknownCriticalExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Tests that a non-critical extension not in the id-ce or id-pe arcs (which is
// thus unknown to us) verifies successfully.
TEST_F(pkixcert_extension, UnknownNonCriticalExtension)
{
  static const uint8_t unknownNonCriticalExtensionBytes[] = {
    0x30, 0x16, // SEQUENCE (length = 22)
      0x06, 0x12, // OID (length = 18)
        // 1.3.6.1.4.1.13769.666.666.666.1.500.9.3
        0x2b, 0x06, 0x01, 0x04, 0x01, 0xeb, 0x49, 0x85, 0x1a,
        0x85, 0x1a, 0x85, 0x1a, 0x01, 0x83, 0x74, 0x09, 0x03,
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const ByteString
    unknownNonCriticalExtension(unknownNonCriticalExtensionBytes,
                                sizeof(unknownNonCriticalExtensionBytes));
  const char* certCN = "Cert With Unknown NonCritical Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, unknownNonCriticalExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Tests that an incorrect OID for id-pe-authorityInformationAccess
// (when marked critical) is detected and that verification fails.
// (Until bug 1020993 was fixed, the code checked for this OID.)
TEST_F(pkixcert_extension, WrongOIDCriticalExtension)
{
  static const uint8_t wrongOIDCriticalExtensionBytes[] = {
    0x30, 0x10, // SEQUENCE (length = 16)
      0x06, 0x09, // OID (length = 9)
        // 1.3.6.6.1.5.5.7.1.1 (there is an extra "6" that shouldn't be there)
        0x2b, 0x06, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01,
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const ByteString
    wrongOIDCriticalExtension(wrongOIDCriticalExtensionBytes,
                              sizeof(wrongOIDCriticalExtensionBytes));
  const char* certCN = "Cert With Critical Wrong OID Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, wrongOIDCriticalExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Tests that a id-pe-authorityInformationAccess critical extension
// is detected and that verification succeeds.
TEST_F(pkixcert_extension, CriticalAIAExtension)
{
  // XXX: According to RFC 5280 an AIA that consists of an empty sequence is
  // not legal, but  we accept it and that is not what we're testing here.
  static const uint8_t criticalAIAExtensionBytes[] = {
    0x30, 0x11, // SEQUENCE (length = 17)
      0x06, 0x08, // OID (length = 8)
        // 1.3.6.1.5.5.7.1.1
        0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01,
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x02, // OCTET STRING (length = 2)
        0x30, 0x00, // SEQUENCE (length = 0)
  };
  static const ByteString
    criticalAIAExtension(criticalAIAExtensionBytes,
                         sizeof(criticalAIAExtensionBytes));
  const char* certCN = "Cert With Critical AIA Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, criticalAIAExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// We know about some id-ce extensions (OID arc 2.5.29), but not all of them.
// Tests that an unknown id-ce extension is detected and that verification
// fails.
TEST_F(pkixcert_extension, UnknownCriticalCEExtension)
{
  static const uint8_t unknownCriticalCEExtensionBytes[] = {
    0x30, 0x0a, // SEQUENCE (length = 10)
      0x06, 0x03, // OID (length = 3)
        0x55, 0x1d, 0x37, // 2.5.29.55
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const ByteString
    unknownCriticalCEExtension(unknownCriticalCEExtensionBytes,
                               sizeof(unknownCriticalCEExtensionBytes));
  const char* certCN = "Cert With Unknown Critical id-ce Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, unknownCriticalCEExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Tests that a certificate with a known critical id-ce extension (in this case,
// OID 2.5.29.54, which is id-ce-inhibitAnyPolicy), verifies successfully.
TEST_F(pkixcert_extension, KnownCriticalCEExtension)
{
  static const uint8_t criticalCEExtensionBytes[] = {
    0x30, 0x0d, // SEQUENCE (length = 13)
      0x06, 0x03, // OID (length = 3)
        0x55, 0x1d, 0x36, // 2.5.29.54 (id-ce-inhibitAnyPolicy)
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x03, // OCTET STRING (length = 3)
        0x02, 0x01, 0x00, // INTEGER (length = 1, value = 0)
  };
  static const ByteString
    criticalCEExtension(criticalCEExtensionBytes,
                        sizeof(criticalCEExtensionBytes));
  const char* certCN = "Cert With Known Critical id-ce Extension";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, criticalCEExtension, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Two subjectAltNames must result in an error.
TEST_F(pkixcert_extension, DuplicateSubjectAltName)
{
  static const uint8_t DER_BYTES[] = {
    0x30, 22, // SEQUENCE (length = 22)
      0x06, 3, // OID (length = 3)
        0x55, 0x1d, 0x11, // 2.5.29.17
      0x04, 15, // OCTET STRING (length = 15)
        0x30, 13, // GeneralNames (SEQUENCE) (length = 13)
          0x82, 11, // [2] (dNSName) (length = 11)
            'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm'
  };
  static const ByteString DER(DER_BYTES, sizeof(DER_BYTES));
  static const ByteString extensions[] = { DER, DER, ByteString() };
  static const char* certCN = "Cert With Duplicate subjectAltName";
  ScopedTestKeyPair key;
  ByteString cert(CreateCert(certCN, extensions, key));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certInput;
  ASSERT_EQ(Success, certInput.Init(cert.data(), cert.length()));
  ASSERT_EQ(Result::ERROR_EXTENSION_VALUE_INVALID,
            BuildCertChain(trustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}
