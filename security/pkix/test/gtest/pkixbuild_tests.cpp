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
           /*optional*/ const ByteString* extension = nullptr,
           /*optional*/ const TestKeyPair* issuerKeyPair = nullptr,
           /*optional*/ const TestKeyPair* subjectKeyPair = nullptr)
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
                       subjectKeyPair ? *subjectKeyPair : *reusedKey,
                       extensions.data(),
                       issuerKeyPair ? *issuerKeyPair : *reusedKey,
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

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
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
  explicit SingleRootTrustDomain(ByteString aRootDER)
    : rootDER(aRootDER)
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

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
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
  explicit ExpiredCertTrustDomain(ByteString aRootDER)
    : SingleRootTrustDomain(aRootDER)
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
  IssuerNameCheckTrustDomain(const ByteString& aIssuer, bool aExpectedKeepGoing)
    : issuer(aIssuer)
    , expectedKeepGoing(aExpectedKeepGoing)
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

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
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
  explicit EmbeddedSCTListTestTrustDomain(ByteString aRootDER)
    : SingleRootTrustDomain(aRootDER)
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

// This TrustDomain implements a hierarchy like so:
//
// A   B
// |   |
// C   D
//  \ /
//   E
//
// where A is a trust anchor, B is not a trust anchor and has no known issuer, C
// and D are intermediates with the same subject and subject public key, and E
// is an end-entity (in practice, the end-entity will be generated by the test
// functions using this trust domain).
class MultiplePathTrustDomain: public DefaultCryptoTrustDomain
{
public:
  void SetUpCerts()
  {
    ASSERT_FALSE(ENCODING_FAILED(CreateCert("UntrustedRoot", "UntrustedRoot",
                                            EndEntityOrCA::MustBeCA,
                                            &subjectDERToCertDER)));
    // The subject DER -> cert DER mapping would be overwritten for subject
    // "Intermediate" when we create the second "Intermediate" certificate, so
    // we keep a copy of this "Intermediate".
    intermediateSignedByUntrustedRootCertDER =
      CreateCert("UntrustedRoot", "Intermediate", EndEntityOrCA::MustBeCA);
    ASSERT_FALSE(ENCODING_FAILED(intermediateSignedByUntrustedRootCertDER));
    rootCACertDER = CreateCert("TrustedRoot", "TrustedRoot",
                               EndEntityOrCA::MustBeCA, &subjectDERToCertDER);
    ASSERT_FALSE(ENCODING_FAILED(rootCACertDER));
    ASSERT_FALSE(ENCODING_FAILED(CreateCert("TrustedRoot", "Intermediate",
                                            EndEntityOrCA::MustBeCA,
                                            &subjectDERToCertDER)));
  }

private:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = InputEqualsByteString(candidateCert, rootCACertDER)
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result CheckCert(ByteString& certDER, IssuerChecker& checker, bool& keepGoing)
  {
    Input derCert;
    Result rv = derCert.Init(certDER.data(), certDER.length());
    if (rv != Success) {
      return rv;
    }
    return checker.Check(derCert, nullptr/*additionalNameConstraints*/,
                         keepGoing);
  }

  Result FindIssuer(Input encodedIssuerName, IssuerChecker& checker, Time)
                    override
  {
    ByteString subjectDER(InputToByteString(encodedIssuerName));
    ByteString certDER(subjectDERToCertDER[subjectDER]);
    assert(!ENCODING_FAILED(certDER));
    bool keepGoing;
    Result rv = CheckCert(certDER, checker, keepGoing);
    if (rv != Success) {
      return rv;
    }
    // Also try the other intermediate.
    if (keepGoing) {
      rv = CheckCert(intermediateSignedByUntrustedRootCertDER, checker,
                     keepGoing);
      if (rv != Success) {
        return rv;
      }
    }
    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*,
                         /*optional*/ const Input*) override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
  {
    return Success;
  }

  std::map<ByteString, ByteString> subjectDERToCertDER;
  ByteString rootCACertDER;
  ByteString intermediateSignedByUntrustedRootCertDER;
};

TEST_F(pkixbuild, BadEmbeddedSCTWithMultiplePaths)
{
  MultiplePathTrustDomain trustDomain;
  trustDomain.SetUpCerts();

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
    // The contents of the OCTET STRING are supposed to consist of an OCTET
    // STRING of useful data. We're testing what happens if it isn't, so shove
    // some bogus (non-OCTET STRING) data in there.
    TLV(der::OCTET_STRING, BytesToByteString(dummySctList)));
  ByteString certDER(CreateCert("Intermediate", "Cert with bogus SCT list",
                                EndEntityOrCA::MustBeEndEntity,
                                nullptr, /*subjectDERToCertDER*/
                                &ctExtension));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certDERInput;
  ASSERT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
  ASSERT_EQ(Result::ERROR_BAD_DER,
            BuildCertChain(trustDomain, certDERInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// Same as a MultiplePathTrustDomain, but the end-entity is revoked.
class RevokedEndEntityTrustDomain final : public MultiplePathTrustDomain
{
public:
  Result CheckRevocation(EndEntityOrCA endEntityOrCA, const CertID&, Time,
                         Duration, /*optional*/ const Input*,
                         /*optional*/ const Input*) override
  {
    if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity) {
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
    return Success;
  }
};

TEST_F(pkixbuild, RevokedEndEntityWithMultiplePaths)
{
  RevokedEndEntityTrustDomain trustDomain;
  trustDomain.SetUpCerts();
  ByteString certDER(CreateCert("Intermediate", "RevokedEndEntity",
                                EndEntityOrCA::MustBeEndEntity));
  ASSERT_FALSE(ENCODING_FAILED(certDER));
  Input certDERInput;
  ASSERT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE,
            BuildCertChain(trustDomain, certDERInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy,
                           nullptr/*stapledOCSPResponse*/));
}

// This represents a collection of different certificates that all have the same
// subject and issuer distinguished name.
class SelfIssuedCertificatesTrustDomain final : public DefaultCryptoTrustDomain
{
public:
  void SetUpCerts(size_t totalCerts)
  {
    ASSERT_TRUE(totalCerts > 0);
    // First we generate a trust anchor.
    ScopedTestKeyPair rootKeyPair(GenerateKeyPair());
    rootCACertDER = CreateCert("DN", "DN", EndEntityOrCA::MustBeCA, nullptr,
                               nullptr, rootKeyPair.get(), rootKeyPair.get());
    ASSERT_FALSE(ENCODING_FAILED(rootCACertDER));
    certs.push_back(rootCACertDER);
    ScopedTestKeyPair issuerKeyPair(rootKeyPair.release());
    size_t subCAsGenerated;
    // Then we generate 6 sub-CAs (given that we were requested to generate at
    // least that many).
    for (subCAsGenerated = 0;
         subCAsGenerated < totalCerts - 1 && subCAsGenerated < 6;
         subCAsGenerated++) {
      // Each certificate has to have a unique SPKI (mozilla::pkix does loop
      // detection and stops searching if it encounters two certificates in a
      // path with the same subject and SPKI).
      ScopedTestKeyPair keyPair(GenerateKeyPair());
      ByteString cert(CreateCert("DN", "DN", EndEntityOrCA::MustBeCA, nullptr,
                                 nullptr, issuerKeyPair.get(), keyPair.get()));
      ASSERT_FALSE(ENCODING_FAILED(cert));
      certs.push_back(cert);
      issuerKeyPair.reset(keyPair.release());
    }
    // We set firstIssuerKey here because we can't end up with a path that has
    // more than 7 CAs in it (because mozilla::pkix limits the path length).
    firstIssuerKey.reset(issuerKeyPair.release());
    // For any more sub CAs we generate, it doesn't matter what their keys are
    // as long as they're different.
    for (; subCAsGenerated < totalCerts - 1; subCAsGenerated++) {
      ScopedTestKeyPair keyPair(GenerateKeyPair());
      ByteString cert(CreateCert("DN", "DN", EndEntityOrCA::MustBeCA, nullptr,
                                 nullptr, keyPair.get(), keyPair.get()));
      ASSERT_FALSE(ENCODING_FAILED(cert));
      certs.insert(certs.begin(), cert);
    }
  }

  const TestKeyPair* GetFirstIssuerKey()
  {
    return firstIssuerKey.get();
  }

private:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = InputEqualsByteString(candidateCert, rootCACertDER)
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result FindIssuer(Input, IssuerChecker& checker, Time) override
  {
    bool keepGoing;
    for (auto& cert: certs) {
      Input certInput;
      Result rv = certInput.Init(cert.data(), cert.length());
      if (rv != Success) {
        return rv;
      }
      rv = checker.Check(certInput, nullptr, keepGoing);
      if (rv != Success || !keepGoing) {
        return rv;
      }
    }
    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         /*optional*/ const Input*, /*optional*/ const Input*)
                         override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
  {
    return Success;
  }

  std::vector<ByteString> certs;
  ByteString rootCACertDER;
  ScopedTestKeyPair firstIssuerKey;
};

TEST_F(pkixbuild, AvoidUnboundedPathSearchingFailure)
{
  SelfIssuedCertificatesTrustDomain trustDomain;
  // This creates a few hundred million potential paths of length 8 (end entity
  // + 6 sub-CAs + root). It would be prohibitively expensive to enumerate all
  // of these, so we give mozilla::pkix a budget that is spent when searching
  // paths. If the budget is exhausted, it simply returns an unknown issuer
  // error. In the future it might be nice to return a specific error that would
  // give the front-end a hint that maybe it shouldn't have so many certificates
  // that all have the same subject and issuer DN but different SPKIs.
  trustDomain.SetUpCerts(18);
  ByteString certDER(CreateCert("DN", "DN", EndEntityOrCA::MustBeEndEntity,
                                nullptr, nullptr,
                                trustDomain.GetFirstIssuerKey()));
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

TEST_F(pkixbuild, AvoidUnboundedPathSearchingSuccess)
{
  SelfIssuedCertificatesTrustDomain trustDomain;
  // This creates a few hundred thousand possible potential paths of length 8
  // (end entity + 6 sub-CAs + root). This will nearly exhaust mozilla::pkix's
  // search budget, so this should succeed.
  trustDomain.SetUpCerts(10);
  ByteString certDER(CreateCert("DN", "DN", EndEntityOrCA::MustBeEndEntity,
                                nullptr, nullptr,
                                trustDomain.GetFirstIssuerKey()));
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
