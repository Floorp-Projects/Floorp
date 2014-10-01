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
#include "pkixgtest.h"
#include "pkixtestutil.h"
#include "prinit.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

static bool
CreateCert(PLArenaPool* arena, const char* issuerStr,
           const char* subjectStr, EndEntityOrCA endEntityOrCA,
           /*optional*/ SECKEYPrivateKey* issuerKey,
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
  const SECItem* issuerDER(ASCIIToDERName(arena, issuerStr));
  if (!issuerDER) {
    return false;
  }
  const SECItem* subjectDER(ASCIIToDERName(arena, subjectStr));
  if (!subjectDER) {
    return false;
  }

  const SECItem* extensions[2] = { nullptr, nullptr };
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    extensions[0] =
      CreateEncodedBasicConstraints(arena, true, nullptr,
                                    ExtensionCriticality::Critical);
    if (!extensions[0]) {
      return false;
    }
  }

  SECItem* certDER(CreateEncodedCertificate(
                     arena, v3, SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
                     serialNumber, issuerDER,
                     PR_Now() - ONE_DAY, PR_Now() + ONE_DAY,
                     subjectDER, extensions, issuerKey, SEC_OID_SHA256,
                     subjectKey));
  if (!certDER) {
    return false;
  }
  subjectCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certDER,
                                        nullptr, false, true);
  return subjectCert.get() != nullptr;
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
        "CN=CA1 (Root)", "CN=CA2", "CN=CA3", "CN=CA4", "CN=CA5", "CN=CA6",
        "CN=CA7"
    };

    static_assert(PR_ARRAY_SIZE(names) == PR_ARRAY_SIZE(certChainTail),
                  "mismatch in sizes of names and certChainTail arrays");

    ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return false;
    }

    for (size_t i = 0; i < PR_ARRAY_SIZE(names); ++i) {
      const char* issuerName = i == 0 ? names[0]
                                      : certChainTail[i - 1]->subjectName;
      if (!CreateCert(arena.get(), issuerName, names[i],
                      EndEntityOrCA::MustBeCA, leafCAKey.get(), leafCAKey,
                      certChainTail[i])) {
        return false;
      }
    }

    return true;
  }

private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              const SECItem& candidateCert,
                              /*out*/ TrustLevel* trustLevel)
  {
    if (SECITEM_ItemsAreEqual(&candidateCert, &certChainTail[0]->derCert)) {
      *trustLevel = TrustLevel::TrustAnchor;
    } else {
      *trustLevel = TrustLevel::InheritsTrust;
    }
    return Success;
  }

  virtual Result FindIssuer(const SECItem& encodedIssuerName,
                            IssuerChecker& checker, PRTime time)
  {
    ScopedCERTCertList
      candidates(CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                            &encodedIssuerName, time, true));
    if (candidates) {
      for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
           !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
        bool keepGoing;
        Result rv = checker.Check(n->cert->derCert,
                                  nullptr/*additionalNameConstraints*/,
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

  virtual Result DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf,
                           size_t digestBufLen)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
  }

  // We hold references to CERTCertificates in the cert chain tail so that we
  // CERT_CreateSubjectCertList can find them.
  ScopedCERTCertificate certChainTail[7];

public:
  ScopedSECKEYPrivateKey leafCAKey;
  CERTCertificate* GetLeafCACert() const
  {
    return certChainTail[PR_ARRAY_SIZE(certChainTail) - 1].get();
  }
};

class pkixbuild : public NSSTest
{
public:
  static void SetUpTestCase()
  {
    NSSTest::SetUpTestCase();
    // Initialize the tail of the cert chains we'll be using once, to make the
    // tests run faster (generating the keys is slow).
    if (!trustDomain.SetUpCertChainTail()) {
      PR_Abort();
    }
  }

protected:
  static TestTrustDomain trustDomain;
};

/*static*/ TestTrustDomain pkixbuild::trustDomain;

TEST_F(pkixbuild, MaxAcceptableCertChainLength)
{
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, trustDomain.GetLeafCACert()->derCert,
                           now, EndEntityOrCA::MustBeCA,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));

  ScopedSECKEYPrivateKey privateKey;
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(),
                         trustDomain.GetLeafCACert()->subjectName,
                         "CN=Direct End-Entity",
                         EndEntityOrCA::MustBeEndEntity,
                         trustDomain.leafCAKey.get(), privateKey, cert));
  ASSERT_EQ(Success,
            BuildCertChain(trustDomain, cert->derCert, now,
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

TEST_F(pkixbuild, BeyondMaxAcceptableCertChainLength)
{
  ScopedSECKEYPrivateKey caPrivateKey;
  ScopedCERTCertificate caCert;
  ASSERT_TRUE(CreateCert(arena.get(),
                         trustDomain.GetLeafCACert()->subjectName,
                         "CN=CA Too Far", EndEntityOrCA::MustBeCA,
                         trustDomain.leafCAKey.get(),
                         caPrivateKey, caCert));
  ASSERT_EQ(Result::ERROR_UNKNOWN_ISSUER,
            BuildCertChain(trustDomain, caCert->derCert, now,
                           EndEntityOrCA::MustBeCA,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));

  ScopedSECKEYPrivateKey privateKey;
  ScopedCERTCertificate cert;
  ASSERT_TRUE(CreateCert(arena.get(), caCert->subjectName,
                         "CN=End-Entity Too Far",
                         EndEntityOrCA::MustBeEndEntity,
                         caPrivateKey.get(), privateKey, cert));
  ASSERT_EQ(Result::ERROR_UNKNOWN_ISSUER,
            BuildCertChain(trustDomain, cert->derCert, now,
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// A TrustDomain that explicitly fails if CheckRevocation is called.
// It is initialized with the DER encoding of a root certificate that
// is treated as a trust anchor and is assumed to have issued all certificates
// (i.e. FindIssuer always attempts to build the next step in the chain with
// it).
class ExpiredCertTrustDomain : public TrustDomain
{
public:
  ExpiredCertTrustDomain(CERTCertificate* rootCert)
    : rootCert(rootCert)
  {
  }

  // The CertPolicyId argument is unused because we don't care about EV.
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              const SECItem& candidateCert,
                              /*out*/ TrustLevel* trustLevel)
  {
    if (SECITEM_ItemsAreEqual(&candidateCert, &rootCert->derCert)) {
      *trustLevel = TrustLevel::TrustAnchor;
    } else {
      *trustLevel = TrustLevel::InheritsTrust;
    }
    return Success;
  }

  virtual Result FindIssuer(const SECItem& encodedIssuerName,
                            IssuerChecker& checker, PRTime time)
  {
    // keepGoing is an out parameter from IssuerChecker.Check. It would tell us
    // whether or not to continue attempting other potential issuers. We only
    // know of one potential issuer, however, so we ignore it.
    bool keepGoing;
    return checker.Check(rootCert->derCert, nullptr, keepGoing);
  }

  virtual Result CheckRevocation(EndEntityOrCA, const CertID&, PRTime,
                                 /*optional*/ const SECItem*,
                                 /*optional*/ const SECItem*)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
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

  virtual Result DigestBuf(const SECItem&, /*out*/uint8_t*, size_t)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
  }

private:
  ScopedCERTCertificate rootCert;
};

TEST_F(pkixbuild, NoRevocationCheckingForExpiredCert)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  ASSERT_NE(nullptr, arena.get());

  const char* rootCN = "CN=Root CA";
  ScopedSECKEYPrivateKey rootKey;
  ScopedCERTCertificate rootCert;
  EXPECT_TRUE(CreateCert(arena.get(), rootCN, rootCN, EndEntityOrCA::MustBeCA,
                         nullptr, rootKey, rootCert));
  ExpiredCertTrustDomain expiredCertTrustDomain(rootCert.release());

  const SECItem* serialNumber(CreateEncodedSerialNumber(arena.get(), 100));
  EXPECT_NE(nullptr, serialNumber);
  const SECItem* issuerDER(ASCIIToDERName(arena.get(), rootCN));
  EXPECT_NE(nullptr, issuerDER);
  const SECItem* subjectDER(ASCIIToDERName(arena.get(), "CN=Expired End-Entity Cert"));
  EXPECT_NE(nullptr, subjectDER);
  ScopedSECKEYPrivateKey unusedSubjectKey;
  SECItem* certDER(CreateEncodedCertificate(
                     arena.get(), v3,
                     SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
                     serialNumber, issuerDER,
                     PR_Now() - ONE_DAY - ONE_DAY,
                     PR_Now() - ONE_DAY,
                     subjectDER, nullptr, rootKey.get(),
                     SEC_OID_SHA256,
                     unusedSubjectKey));
  ASSERT_NE(nullptr, certDER);
  ASSERT_EQ(Result::ERROR_EXPIRED_CERTIFICATE,
            BuildCertChain(expiredCertTrustDomain, *certDER,
                           PR_Now(), EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr));
}
