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
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

// Creates a self-signed certificate with the given extension.
static bool
CreateCert(PLArenaPool* arena, const char* subjectStr,
           const SECItem* extension,
           /*out*/ ScopedSECKEYPrivateKey& subjectKey,
           /*out*/ ScopedCERTCertificate& subjectCert)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  const SECItem* serialNumber(CreateEncodedSerialNumber(arena,
                                                        serialNumberValue));
  if (!serialNumber) {
    return false;
  }
  const SECItem* issuerDER(ASCIIToDERName(arena, subjectStr));
  if (!issuerDER) {
    return false;
  }
  const SECItem* subjectDER(ASCIIToDERName(arena, subjectStr));
  if (!subjectDER) {
    return false;
  }

  const SECItem* extensions[2] = { extension, nullptr };

  SECItem* certDER(CreateEncodedCertificate(arena, v3, SEC_OID_SHA256,
                                            serialNumber, issuerDER,
                                            PR_Now() - ONE_DAY,
                                            PR_Now() + ONE_DAY,
                                            subjectDER, extensions,
                                            nullptr, SEC_OID_SHA256,
                                            subjectKey));
  if (!certDER) {
    return false;
  }
  subjectCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certDER,
                                        nullptr, false, true);
  return subjectCert.get() != nullptr;
}

class TrustEverythingTrustDomain : public TrustDomain
{
private:
  SECStatus GetCertTrust(EndEntityOrCA,
                         const CertPolicyId&,
                         const SECItem& candidateCert,
                         /*out*/ TrustLevel* trustLevel)
  {
    *trustLevel = TrustLevel::TrustAnchor;
    return SECSuccess;
  }

  SECStatus FindPotentialIssuers(const SECItem* encodedIssuerName,
                                 PRTime time,
                                 /*out*/ ScopedCERTCertList& results)
  {
    return SECSuccess;
  }

  SECStatus VerifySignedData(const CERTSignedData* signedData,
                             const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                             nullptr);
  }

  SECStatus CheckRevocation(EndEntityOrCA, const CERTCertificate*,
                            /*const*/ CERTCertificate*, PRTime,
                            /*optional*/ const SECItem*)
  {
    return SECSuccess;
  }

  virtual SECStatus IsChainValid(const CERTCertList*)
  {
    return SECSuccess;
  }
};

class pkix_cert_extensions: public NSSTest
{
public:
  static void SetUpTestCase()
  {
    NSSTest::SetUpTestCase();
  }

protected:
  static TrustEverythingTrustDomain trustDomain;
};

/*static*/ TrustEverythingTrustDomain pkix_cert_extensions::trustDomain;

// Tests that a critical extension not in the id-ce or id-pe arcs (which is
// thus unknown to us) is detected and that verification fails with the
// appropriate error.
TEST_F(pkix_cert_extensions, UnknownCriticalExtension)
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
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &unknownCriticalExtension, key,
                         cert));
  ScopedCERTCertList results;
  ASSERT_SECFailure(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION,
                    BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}

// Tests that a non-critical extension not in the id-ce or id-pe arcs (which is
// thus unknown to us) verifies successfully.
TEST_F(pkix_cert_extensions, UnknownNonCriticalExtension)
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
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &unknownNonCriticalExtension, key,
                         cert));
  ScopedCERTCertList results;
  ASSERT_SECSuccess(BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}

// Tests that an incorrect OID for id-pe-authorityInformationAccess
// (when marked critical) is detected and that verification fails.
// (Until bug 1020993 was fixed, the code checked for this OID.)
TEST_F(pkix_cert_extensions, WrongOIDCriticalExtension)
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
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &wrongOIDCriticalExtension, key,
                         cert));
  ScopedCERTCertList results;
  ASSERT_SECFailure(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION,
                    BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}

// Tests that a id-pe-authorityInformationAccess critical extension
// is detected and that verification succeeds.
TEST_F(pkix_cert_extensions, CriticalAIAExtension)
{
  static const uint8_t criticalAIAExtensionBytes[] = {
    0x30, 0x0f, // SEQUENCE (length = 15)
      0x06, 0x08, // OID (length = 8)
        // 1.3.6.1.5.5.7.1.1
        0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x01, 0x01,
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const SECItem criticalAIAExtension = {
    siBuffer,
    const_cast<unsigned char*>(criticalAIAExtensionBytes),
    sizeof(criticalAIAExtensionBytes)
  };
  const char* certCN = "CN=Cert With Critical AIA Extension";
  ScopedSECKEYPrivateKey key;
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &criticalAIAExtension, key,
                         cert));
  ScopedCERTCertList results;
  ASSERT_SECSuccess(BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}

// We know about some id-ce extensions (OID arc 2.5.29), but not all of them.
// Tests that an unknown id-ce extension is detected and that verification
// fails.
TEST_F(pkix_cert_extensions, UnknownCriticalCEExtension)
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
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &unknownCriticalCEExtension, key,
                         cert));
  ScopedCERTCertList results;
  ASSERT_SECFailure(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION,
                    BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}

// Tests that a certificate with a known critical id-ce extension (in this case,
// OID 2.5.29.54, which is id-ce-inhibitAnyPolicy), verifies successfully.
TEST_F(pkix_cert_extensions, KnownCriticalCEExtension)
{
  static const uint8_t criticalCEExtensionBytes[] = {
    0x30, 0x0a, // SEQUENCE (length = 10)
      0x06, 0x03, // OID (length = 3)
        0x55, 0x1d, 0x36, // 2.5.29.54 (id-ce-inhibitAnyPolicy)
      0x01, 0x01, 0xff, // BOOLEAN (length = 1) TRUE
      0x04, 0x00 // OCTET STRING (length = 0)
  };
  static const SECItem criticalCEExtension = {
    siBuffer,
    const_cast<unsigned char*>(criticalCEExtensionBytes),
    sizeof(criticalCEExtensionBytes)
  };
  const char* certCN = "CN=Cert With Known Critical id-ce Extension";
  ScopedSECKEYPrivateKey key;
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), certCN, &criticalCEExtension, key, cert));
  ScopedCERTCertList results;
  ASSERT_SECSuccess(BuildCertChain(trustDomain, cert.get(),
                                   now, EndEntityOrCA::MustBeEndEntity,
                                   KeyUsage::noParticularKeyUsageRequired,
                                   KeyPurposeId::anyExtendedKeyUsage,
                                   CertPolicyId::anyPolicy,
                                   nullptr, // stapled OCSP response
                                   results));
}
