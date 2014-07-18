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

#include "nssgtest.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "pkixtestutil.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

// Creates a self-signed certificate with the given extension.
static const SECItem*
CreateCert(PLArenaPool* arena, const char* subjectStr,
           SECItem const* const* extensions, // null-terminated array
           /*out*/ ScopedSECKEYPrivateKey& subjectKey)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  const SECItem* serialNumber(CreateEncodedSerialNumber(arena,
                                                        serialNumberValue));
  if (!serialNumber) {
    return nullptr;
  }
  const SECItem* issuerDER(ASCIIToDERName(arena, subjectStr));
  if (!issuerDER) {
    return nullptr;
  }
  const SECItem* subjectDER(ASCIIToDERName(arena, subjectStr));
  if (!subjectDER) {
    return nullptr;
  }

  return CreateEncodedCertificate(arena, v3,
                                  SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
                                  serialNumber, issuerDER,
                                  PR_Now() - ONE_DAY,
                                  PR_Now() + ONE_DAY,
                                  subjectDER, extensions,
                                  nullptr, SEC_OID_SHA256, subjectKey);
}

// Creates a self-signed certificate with the given extension.
static const SECItem*
CreateCert(PLArenaPool* arena, const char* subjectStr,
           const SECItem* extension,
           /*out*/ ScopedSECKEYPrivateKey& subjectKey)
{
  const SECItem * extensions[] = { extension, nullptr };
  return CreateCert(arena, subjectStr, extensions, subjectKey);
}

class TrustEverythingTrustDomain : public TrustDomain
{
private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              const SECItem& candidateCert,
                              /*out*/ TrustLevel* trustLevel)
  {
    *trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }

  virtual Result FindIssuer(const SECItem& /*encodedIssuerName*/,
                            IssuerChecker& /*checker*/, PRTime /*time*/)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckRevocation(EndEntityOrCA, const CertID&, PRTime,
                                 /*optional*/ const SECItem*,
                                 /*optional*/ const SECItem*)
  {
    return Success;
  }

  virtual Result IsChainValid(const DERArray&)
  {
    return Success;
  }

  virtual Result VerifySignedData(const SignedDataWithSignature& signedData,
                                  const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                             nullptr);
  }

  virtual Result DigestBuf(const SECItem&, /*out*/ uint8_t*, size_t)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
  }
};

class pkixcert_extension: public NSSTest
{
public:
  static void SetUpTestCase()
  {
    NSSTest::SetUpTestCase();
  }

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
  static const SECItem unknownCriticalExtension = {
    siBuffer,
    const_cast<unsigned char*>(unknownCriticalExtensionBytes),
    sizeof(unknownCriticalExtensionBytes)
  };
  const char* certCN = "CN=Cert With Unknown Critical Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN,
                                 &unknownCriticalExtension, key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem unknownNonCriticalExtension = {
    siBuffer,
    const_cast<unsigned char*>(unknownNonCriticalExtensionBytes),
    sizeof(unknownNonCriticalExtensionBytes)
  };
  const char* certCN = "CN=Cert With Unknown NonCritical Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN,
                                 &unknownNonCriticalExtension, key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem wrongOIDCriticalExtension = {
    siBuffer,
    const_cast<unsigned char*>(wrongOIDCriticalExtensionBytes),
    sizeof(wrongOIDCriticalExtensionBytes)
  };
  const char* certCN = "CN=Cert With Critical Wrong OID Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN,
                                 &wrongOIDCriticalExtension, key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem criticalAIAExtension = {
    siBuffer,
    const_cast<unsigned char*>(criticalAIAExtensionBytes),
    sizeof(criticalAIAExtensionBytes)
  };
  const char* certCN = "CN=Cert With Critical AIA Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN, &criticalAIAExtension,
                                 key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem unknownCriticalCEExtension = {
    siBuffer,
    const_cast<unsigned char*>(unknownCriticalCEExtensionBytes),
    sizeof(unknownCriticalCEExtensionBytes)
  };
  const char* certCN = "CN=Cert With Unknown Critical id-ce Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN,
                                 &unknownCriticalCEExtension, key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Result::ERROR_UNKNOWN_CRITICAL_EXTENSION,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem criticalCEExtension = {
    siBuffer,
    const_cast<unsigned char*>(criticalCEExtensionBytes),
    sizeof(criticalCEExtensionBytes)
  };
  const char* certCN = "CN=Cert With Known Critical id-ce Extension";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN, &criticalCEExtension,
                                 key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, *cert, now,
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
  static const SECItem DER = {
    siBuffer,
    const_cast<unsigned char*>(DER_BYTES),
    sizeof(DER_BYTES)
  };
  static SECItem const* const extensions[] = { &DER, &DER, nullptr };
  static const char* certCN = "CN=Cert With Duplicate subjectAltName";
  ScopedSECKEYPrivateKey key;
  // cert is owned by the arena
  const SECItem* cert(CreateCert(arena.get(), certCN, extensions, key));
  ASSERT_TRUE(cert);
  ASSERT_EQ(Result::ERROR_EXTENSION_VALUE_INVALID,
            BuildCertChain(trustDomain, *cert, now,
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}
