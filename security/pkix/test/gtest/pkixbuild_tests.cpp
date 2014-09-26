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
#include "nss.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "pkixgtest.h"
#include "pkixtestutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

typedef ScopedPtr<CERTCertificate, CERT_DestroyCertificate>
          ScopedCERTCertificate;
typedef ScopedPtr<CERTCertList, CERT_DestroyCertList> ScopedCERTCertList;

static ByteString
CreateCert(const char* issuerCN,
           const char* subjectCN,
           EndEntityOrCA endEntityOrCA,
           /*optional*/ TestKeyPair* issuerKey,
           /*out*/ ScopedTestKeyPair& subjectKey,
           /*out*/ ScopedCERTCertificate* subjectCert = nullptr)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_NE(ENCODING_FAILED, serialNumber);

  ByteString issuerDER(CNToDERName(issuerCN));
  EXPECT_NE(ENCODING_FAILED, issuerDER);
  ByteString subjectDER(CNToDERName(subjectCN));
  EXPECT_NE(ENCODING_FAILED, subjectDER);

  ByteString extensions[2];
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    extensions[0] =
      CreateEncodedBasicConstraints(true, nullptr,
                                    ExtensionCriticality::Critical);
    EXPECT_NE(ENCODING_FAILED, extensions[0]);
  }

  ByteString certDER(CreateEncodedCertificate(
                       v3, sha256WithRSAEncryption,
                       serialNumber, issuerDER,
                       oneDayBeforeNow, oneDayAfterNow,
                       subjectDER, extensions, issuerKey,
                       SignatureAlgorithm::rsa_pkcs1_with_sha256,
                       subjectKey));
  EXPECT_NE(ENCODING_FAILED, certDER);
  if (subjectCert) {
    SECItem certDERItem = {
      siBuffer,
      const_cast<uint8_t*>(certDER.data()),
      static_cast<unsigned int>(certDER.length())
    };
    *subjectCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                                           &certDERItem, nullptr, false, true);
    EXPECT_TRUE(*subjectCert);
  }
  return certDER;
}

class TestTrustDomain : public TrustDomain
{
public:
  // The "cert chain tail" is a longish chain of certificates that is used by
  // all of the tests here. We share this chain across all the tests in order
  // to speed up the tests (generating keypairs for the certs is very slow).
  bool SetUpCertChainTail()
  {
    static char const* const names[] = {
        "CA1 (Root)", "CA2", "CA3", "CA4", "CA5", "CA6", "CA7"
    };

    static_assert(MOZILLA_PKIX_ARRAY_LENGTH(names) ==
                    MOZILLA_PKIX_ARRAY_LENGTH(certChainTail),
                  "mismatch in sizes of names and certChainTail arrays");

    for (size_t i = 0; i < MOZILLA_PKIX_ARRAY_LENGTH(names); ++i) {
      const char* issuerName = i == 0 ? names[0] : names[i-1];
      (void) CreateCert(issuerName, names[i], EndEntityOrCA::MustBeCA,
                        leafCAKey.get(), leafCAKey, &certChainTail[i]);
    }

    return true;
  }

private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              Input candidateCert,
                              /*out*/ TrustLevel& trustLevel)
  {
    Input rootDER;
    Result rv = rootDER.Init(certChainTail[0]->derCert.data,
                             certChainTail[0]->derCert.len);
    EXPECT_EQ(Success, rv);
    if (InputsAreEqual(candidateCert, rootDER)) {
      trustLevel = TrustLevel::TrustAnchor;
    } else {
      trustLevel = TrustLevel::InheritsTrust;
    }
    return Success;
  }

  virtual Result FindIssuer(Input encodedIssuerName,
                            IssuerChecker& checker, Time time)
  {
    SECItem encodedIssuerNameSECItem =
      UnsafeMapInputToSECItem(encodedIssuerName);
    ScopedCERTCertList
      candidates(CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                            &encodedIssuerNameSECItem, 0,
                                            false));
    if (candidates) {
      for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
           !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
        bool keepGoing;
        Input derCert;
        Result rv = derCert.Init(n->cert->derCert.data, n->cert->derCert.len);
        EXPECT_EQ(Success, rv);
        if (rv != Success) {
          return rv;
        }
        rv = checker.Check(derCert, nullptr/*additionalNameConstraints*/,
                           keepGoing);
        if (rv != Success) {
          return rv;
        }
        if (!keepGoing) {
          break;
        }
      }
    }

    return Success;
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
    return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                             nullptr);
  }

  virtual Result DigestBuf(Input item, /*out*/ uint8_t *digestBuf,
                           size_t digestBufLen)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(Input subjectPublicKeyInfo)
  {
    return TestCheckPublicKey(subjectPublicKeyInfo);
  }

  // We hold references to CERTCertificates in the cert chain tail so that we
  // CERT_CreateSubjectCertList can find them.
  ScopedCERTCertificate certChainTail[7];

public:
  ScopedTestKeyPair leafCAKey;
  CERTCertificate* GetLeafCACert() const
  {
    return certChainTail[MOZILLA_PKIX_ARRAY_LENGTH(certChainTail) - 1].get();
  }
};

class pkixbuild : public ::testing::Test
{
public:
  static void SetUpTestCase()
  {
    // XXX(Bug 1070444): We have to initialize NSS explicitly for these tests,
    // unlike other tests, because we're using NSS directly.
    if (NSS_NoDB_Init(nullptr) != SECSuccess) {
      abort();
    }

    if (!trustDomain.SetUpCertChainTail()) {
      abort();
    }
  }

protected:

  static TestTrustDomain trustDomain;
};

/*static*/ TestTrustDomain pkixbuild::trustDomain;

TEST_F(pkixbuild, MaxAcceptableCertChainLength)
{
  {
    Input certDER;
    ASSERT_EQ(Success, certDER.Init(trustDomain.GetLeafCACert()->derCert.data,
                                    trustDomain.GetLeafCACert()->derCert.len));
    ASSERT_EQ(Success,
              BuildCertChain(trustDomain, certDER, Now(),
                             EndEntityOrCA::MustBeCA,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::id_kp_serverAuth,
                             CertPolicyId::anyPolicy,
                             nullptr/*stapledOCSPResponse*/));
  }

  {
    ScopedTestKeyPair unusedKeyPair;
    ScopedCERTCertificate cert;
    ByteString certDER(CreateCert("CA7", "Direct End-Entity",
                                  EndEntityOrCA::MustBeEndEntity,
                                  trustDomain.leafCAKey.get(), unusedKeyPair));
    ASSERT_NE(ENCODING_FAILED, certDER);
    Input certDERInput;
    ASSERT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
    ASSERT_EQ(Success,
              BuildCertChain(trustDomain, certDERInput, Now(),
                             EndEntityOrCA::MustBeEndEntity,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::id_kp_serverAuth,
                             CertPolicyId::anyPolicy,
                             nullptr/*stapledOCSPResponse*/));
  }
}

TEST_F(pkixbuild, BeyondMaxAcceptableCertChainLength)
{
  static char const* const caCertName = "CA Too Far";
  ScopedTestKeyPair caKeyPair;

  // We need a CERTCertificate for caCert so that the trustdomain's FindIssuer
  // method can find it through the NSS cert DB.
  ScopedCERTCertificate caCert;

  {
    ByteString certDER(CreateCert("CA7", caCertName, EndEntityOrCA::MustBeCA,
                                  trustDomain.leafCAKey.get(), caKeyPair,
                                  &caCert));
    ASSERT_NE(ENCODING_FAILED, certDER);
    Input certDERInput;
    ASSERT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
    ASSERT_EQ(Result::ERROR_UNKNOWN_ISSUER,
              BuildCertChain(trustDomain, certDERInput, Now(),
                             EndEntityOrCA::MustBeCA,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::id_kp_serverAuth,
                             CertPolicyId::anyPolicy,
                             nullptr/*stapledOCSPResponse*/));
  }

  {
    ScopedTestKeyPair unusedKeyPair;
    ByteString certDER(CreateCert(caCertName, "End-Entity Too Far",
                                  EndEntityOrCA::MustBeEndEntity,
                                  caKeyPair.get(), unusedKeyPair));
    ASSERT_NE(ENCODING_FAILED, certDER);
    Input certDERInput;
    ASSERT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
    ASSERT_EQ(Result::ERROR_UNKNOWN_ISSUER,
              BuildCertChain(trustDomain, certDERInput, Now(),
                             EndEntityOrCA::MustBeEndEntity,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::id_kp_serverAuth,
                             CertPolicyId::anyPolicy,
                             nullptr/*stapledOCSPResponse*/));
  }
}
