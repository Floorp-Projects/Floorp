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

#if defined(_MSC_VER) && _MSC_VER < 1900
// When building with -D_HAS_EXCEPTIONS=0, MSVC's <xtree> header triggers
// warning C4702: unreachable code.
// https://connect.microsoft.com/VisualStudio/feedback/details/809962
#pragma warning(push)
#pragma warning(disable: 4702)
#endif

#include <map>
#include <vector>

#if defined(_MSC_VER) && _MSC_VER < 1900
#pragma warning(pop)
#endif

#include "pkixder.h"
#include "pkixgtest.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

static ByteString
CreateCert(const char* issuerCN, // null means "empty name"
           const char* subjectCN, // null means "empty name"
           EndEntityOrCA endEntityOrCA,
           /*optional modified*/ std::map<ByteString, ByteString>*
             subjectDERToCertDER = nullptr,
           /*optional*/ const ByteString* extension = nullptr)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString issuerDER(issuerCN ? CNToDERName(issuerCN) : Name(ByteString()));
  ByteString subjectDER(subjectCN ? CNToDERName(subjectCN) : Name(ByteString()));

  std::vector<ByteString> extensions;
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    ByteString basicConstraints =
      CreateEncodedBasicConstraints(true, nullptr, Critical::Yes);
    EXPECT_FALSE(ENCODING_FAILED(basicConstraints));
    extensions.push_back(basicConstraints);
  }
  if (extension) {
    extensions.push_back(*extension);
  }
  extensions.push_back(ByteString()); // marks the end of the list

  ScopedTestKeyPair reusedKey(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(
                       v3, sha256WithRSAEncryption(), serialNumber, issuerDER,
                       oneDayBeforeNow, oneDayAfterNow, subjectDER,
                       *reusedKey, extensions.data(), *reusedKey,
                       sha256WithRSAEncryption()));
  EXPECT_FALSE(ENCODING_FAILED(certDER));

  if (subjectDERToCertDER) {
    (*subjectDERToCertDER)[subjectDER] = certDER;
  }

  return certDER;
}

class TestTrustDomain final : public DefaultCryptoTrustDomain
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

    for (size_t i = 0; i < MOZILLA_PKIX_ARRAY_LENGTH(names); ++i) {
      const char* issuerName = i == 0 ? names[0] : names[i-1];
      CreateCACert(issuerName, names[i]);
      if (i == 0) {
        rootCACertDER = leafCACertDER;
      }
    }

    return true;
  }

  void CreateCACert(const char* issuerName, const char* subjectName)
  {
    leafCACertDER = CreateCert(issuerName, subjectName,
                               EndEntityOrCA::MustBeCA, &subjectDERToCertDER);
    assert(!ENCODING_FAILED(leafCACertDER));
  }

  ByteString GetLeafCACertDER() const { return leafCACertDER; }

private:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = InputEqualsByteString(candidateCert, rootCACertDER)
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result FindIssuer(Input encodedIssuerName, IssuerChecker& checker, Time)
                    override
  {
    ByteString subjectDER(InputToByteString(encodedIssuerName));
    ByteString certDER(subjectDERToCertDER[subjectDER]);
    Input derCert;
    Result rv = derCert.Init(certDER.data(), certDER.length());
    if (rv != Success) {
      return rv;
    }
    bool keepGoing;
    rv = checker.Check(derCert, nullptr/*additionalNameConstraints*/,
                       keepGoing);
    if (rv != Success) {
      return rv;
    }
    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*, /*optional*/ const Input*)
                         override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time) override
  {
    return Success;
  }

  std::map<ByteString, ByteString> subjectDERToCertDER;
  ByteString leafCACertDER;
  ByteString rootCACertDER;
};

class pkixbuild : public ::testing::Test
{
public:
  static void SetUpTestCase()
  {
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
    ByteString leafCACert(trustDomain.GetLeafCACertDER());
    Input certDER;
    ASSERT_EQ(Success, certDER.Init(leafCACert.data(), leafCACert.length()));
    ASSERT_EQ(Success,
              BuildCertChain(trustDomain, certDER, Now(),
                             EndEntityOrCA::MustBeCA,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::id_kp_serverAuth,
                             CertPolicyId::anyPolicy,
                             nullptr/*stapledOCSPResponse*/));
  }

  {
    ByteString certDER(CreateCert("CA7", "Direct End-Entity",
                                  EndEntityOrCA::MustBeEndEntity));
    ASSERT_FALSE(ENCODING_FAILED(certDER));
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

  trustDomain.CreateCACert("CA7", caCertName);

  {
    ByteString certDER(trustDomain.GetLeafCACertDER());
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
    ByteString certDER(CreateCert(caCertName, "End-Entity Too Far",
                                  EndEntityOrCA::MustBeEndEntity));
    ASSERT_FALSE(ENCODING_FAILED(certDER));
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

// A TrustDomain that checks certificates against a given root certificate.
// It is initialized with the DER encoding of a root certificate that
// is treated as a trust anchor and is assumed to have issued all certificates
// (i.e. FindIssuer always attempts to build the next step in the chain with
// it).
class SingleRootTrustDomain : public DefaultCryptoTrustDomain
{
public:
  explicit SingleRootTrustDomain(ByteString rootDER)
    : rootDER(rootDER)
  {
  }

  // The CertPolicyId argument is unused because we don't care about EV.
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      /*out*/ TrustLevel& trustLevel) override
  {
    Input rootCert;
    Result rv = rootCert.Init(rootDER.data(), rootDER.length());
    if (rv != Success) {
      return rv;
    }
    if (InputsAreEqual(candidateCert, rootCert)) {
      trustLevel = TrustLevel::TrustAnchor;
    } else {
      trustLevel = TrustLevel::InheritsTrust;
    }
    return Success;
  }

  Result FindIssuer(Input, IssuerChecker& checker, Time) override
  {
    // keepGoing is an out parameter from IssuerChecker.Check. It would tell us
    // whether or not to continue attempting other potential issuers. We only
    // know of one potential issuer, however, so we ignore it.
    bool keepGoing;
    Input rootCert;
    Result rv = rootCert.Init(rootDER.data(), rootDER.length());
    if (rv != Success) {
      return rv;
    }
    return checker.Check(rootCert, nullptr, keepGoing);
  }

  Result IsChainValid(const DERArray&, Time) override
  {
    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*, /*optional*/ const Input*)
                         override
  {
    return Success;
  }

private:
  ByteString rootDER;
};

// A TrustDomain that explicitly fails if CheckRevocation is called.
class ExpiredCertTrustDomain final : public SingleRootTrustDomain
{
public:
  explicit ExpiredCertTrustDomain(ByteString rootDER)
    : SingleRootTrustDomain(rootDER)
  {
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*, /*optional*/ const Input*)
                         override
  {
    ADD_FAILURE();
    return NotReached("CheckRevocation should not be called",
                      Result::FATAL_ERROR_LIBRARY_FAILURE);
  }
};

TEST_F(pkixbuild, NoRevocationCheckingForExpiredCert)
{
  const char* rootCN = "Root CA";
  ByteString rootDER(CreateCert(rootCN, rootCN, EndEntityOrCA::MustBeCA,
                                nullptr));
  EXPECT_FALSE(ENCODING_FAILED(rootDER));
  ExpiredCertTrustDomain expiredCertTrustDomain(rootDER);

  ByteString serialNumber(CreateEncodedSerialNumber(100));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));
  ByteString issuerDER(CNToDERName(rootCN));
  ByteString subjectDER(CNToDERName("Expired End-Entity Cert"));
  ScopedTestKeyPair reusedKey(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(
                       v3, sha256WithRSAEncryption(),
                       serialNumber, issuerDER,
                       twoDaysBeforeNow,
                       oneDayBeforeNow,
                       subjectDER, *reusedKey, nullptr, *reusedKey,
                       sha256WithRSAEncryption()));
  EXPECT_FALSE(ENCODING_FAILED(certDER));

  Input cert;
  ASSERT_EQ(Success, cert.Init(certDER.data(), certDER.length()));
  ASSERT_EQ(Result::ERROR_EXPIRED_CERTIFICATE,
            BuildCertChain(expiredCertTrustDomain, cert, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr));
}

class DSSTrustDomain final : public EverythingFailsByDefaultTrustDomain
{
public:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                      Input, /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = TrustLevel::TrustAnchor;
    return Success;
  }
};

class pkixbuild_DSS : public ::testing::Test { };

TEST_F(pkixbuild_DSS, DSSEndEntityKeyNotAccepted)
{
  DSSTrustDomain trustDomain;

  ByteString serialNumber(CreateEncodedSerialNumber(1));
  ASSERT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString subjectDER(CNToDERName("DSS"));
  ASSERT_FALSE(ENCODING_FAILED(subjectDER));
  ScopedTestKeyPair subjectKey(GenerateDSSKeyPair());
  ASSERT_TRUE(subjectKey.get());

  ByteString issuerDER(CNToDERName("RSA"));
  ASSERT_FALSE(ENCODING_FAILED(issuerDER));
  ScopedTestKeyPair issuerKey(CloneReusedKeyPair());
  ASSERT_TRUE(issuerKey.get());

  ByteString cert(CreateEncodedCertificate(v3, sha256WithRSAEncryption(),
                                           serialNumber, issuerDER,
                                           oneDayBeforeNow, oneDayAfterNow,
                                           subjectDER, *subjectKey, nullptr,
                                           *issuerKey, sha256WithRSAEncryption()));
  ASSERT_FALSE(ENCODING_FAILED(cert));
  Input certDER;
  ASSERT_EQ(Success, certDER.Init(cert.data(), cert.length()));

  ASSERT_EQ(Result::ERROR_UNSUPPORTED_KEYALG,
            BuildCertChain(trustDomain, certDER, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

class IssuerNameCheckTrustDomain final : public DefaultCryptoTrustDomain
{
public:
  IssuerNameCheckTrustDomain(const ByteString& issuer, bool expectedKeepGoing)
    : issuer(issuer)
    , expectedKeepGoing(expectedKeepGoing)
  {
  }

  Result GetCertTrust(EndEntityOrCA endEntityOrCA, const CertPolicyId&, Input,
                      /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = endEntityOrCA == EndEntityOrCA::MustBeCA
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result FindIssuer(Input, IssuerChecker& checker, Time) override
  {
    Input issuerInput;
    EXPECT_EQ(Success, issuerInput.Init(issuer.data(), issuer.length()));
    bool keepGoing;
    EXPECT_EQ(Success,
              checker.Check(issuerInput, nullptr /*additionalNameConstraints*/,
                            keepGoing));
    EXPECT_EQ(expectedKeepGoing, keepGoing);
    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*, /*optional*/ const Input*)
                         override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time) override
  {
    return Success;
  }

private:
  const ByteString issuer;
  const bool expectedKeepGoing;
};

struct IssuerNameCheckParams
{
  const char* subjectIssuerCN; // null means "empty name"
  const char* issuerSubjectCN; // null means "empty name"
  bool matches;
  Result expectedError;
};

static const IssuerNameCheckParams ISSUER_NAME_CHECK_PARAMS[] =
{
  { "foo", "foo", true, Success },
  { "foo", "bar", false, Result::ERROR_UNKNOWN_ISSUER },
  { "f", "foo", false, Result::ERROR_UNKNOWN_ISSUER }, // prefix
  { "foo", "f", false, Result::ERROR_UNKNOWN_ISSUER }, // prefix
  { "foo", "Foo", false, Result::ERROR_UNKNOWN_ISSUER }, // case sensitive
  { "", "", true, Success },
  { nullptr, nullptr, false, Result::ERROR_EMPTY_ISSUER_NAME }, // empty issuer

  // check that certificate-related errors are deferred and superseded by
  // ERROR_UNKNOWN_ISSUER when a chain can't be built due to name mismatches
  { "foo", nullptr, false, Result::ERROR_UNKNOWN_ISSUER },
  { nullptr, "foo", false, Result::ERROR_UNKNOWN_ISSUER }
};

class pkixbuild_IssuerNameCheck
  : public ::testing::Test
  , public ::testing::WithParamInterface<IssuerNameCheckParams>
{
};

TEST_P(pkixbuild_IssuerNameCheck, MatchingName)
{
  const IssuerNameCheckParams& params(GetParam());

  ByteString issuerCertDER(CreateCert(params.issuerSubjectCN,
                                      params.issuerSubjectCN,
                                      EndEntityOrCA::MustBeCA, nullptr));
  ASSERT_FALSE(ENCODING_FAILED(issuerCertDER));

  ByteString subjectCertDER(CreateCert(params.subjectIssuerCN, "end-entity",
                                       EndEntityOrCA::MustBeEndEntity,
                                       nullptr));
  ASSERT_FALSE(ENCODING_FAILED(subjectCertDER));

  Input subjectCertDERInput;
  ASSERT_EQ(Success, subjectCertDERInput.Init(subjectCertDER.data(),
                                              subjectCertDER.length()));

  IssuerNameCheckTrustDomain trustDomain(issuerCertDER, !params.matches);
  ASSERT_EQ(params.expectedError,
            BuildCertChain(trustDomain, subjectCertDERInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

INSTANTIATE_TEST_CASE_P(pkixbuild_IssuerNameCheck, pkixbuild_IssuerNameCheck,
                        testing::ValuesIn(ISSUER_NAME_CHECK_PARAMS));


// Records the embedded SCT list extension for later examination.
class EmbeddedSCTListTestTrustDomain final : public SingleRootTrustDomain
{
public:
  explicit EmbeddedSCTListTestTrustDomain(ByteString rootDER)
    : SingleRootTrustDomain(rootDER)
  {
  }

  virtual void NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                      Input extensionData) override
  {
    if (extension == AuxiliaryExtension::EmbeddedSCTList) {
      signedCertificateTimestamps = InputToByteString(extensionData);
    } else {
      ADD_FAILURE();
    }
  }

  ByteString signedCertificateTimestamps;
};

TEST_F(pkixbuild, CertificateTransparencyExtension)
{
  // python security/pkix/tools/DottedOIDToCode.py --tlv
  //   id-embeddedSctList 1.3.6.1.4.1.11129.2.4.2
  static const uint8_t tlv_id_embeddedSctList[] = {
    0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x02
  };
  static const uint8_t dummySctList[] = {
    0x01, 0x02, 0x03, 0x04, 0x05
  };

  ByteString ctExtension = TLV(der::SEQUENCE,
    BytesToByteString(tlv_id_embeddedSctList) +
    Boolean(false) +
    TLV(der::OCTET_STRING,
      // SignedCertificateTimestampList structure is encoded as an OCTET STRING
      // within the X.509v3 extension (see RFC 6962 section 3.3).
      // pkix decodes it internally and returns the actual structure.
      TLV(der::OCTET_STRING, BytesToByteString(dummySctList))));

  const char* rootCN = "Root CA";
  ByteString rootDER(CreateCert(rootCN, rootCN, EndEntityOrCA::MustBeCA));
  ASSERT_FALSE(ENCODING_FAILED(rootDER));

  ByteString certDER(CreateCert(rootCN, "Cert with SCT list",
                                EndEntityOrCA::MustBeEndEntity,
                                nullptr, /*subjectDERToCertDER*/
                                &ctExtension));
  ASSERT_FALSE(ENCODING_FAILED(certDER));

  Input certInput;
  ASSERT_EQ(Success, certInput.Init(certDER.data(), certDER.length()));

  EmbeddedSCTListTestTrustDomain extTrustDomain(rootDER);
  ASSERT_EQ(Success,
            BuildCertChain(extTrustDomain, certInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::anyExtendedKeyUsage,
                           CertPolicyId::anyPolicy,
                           nullptr /*stapledOCSPResponse*/));
  ASSERT_EQ(BytesToByteString(dummySctList),
            extTrustDomain.signedCertificateTimestamps);
}
