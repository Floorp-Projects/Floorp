/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "pkixgtest.h"

#include "mozpkix/pkixder.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

static ByteString
CreateCert(const char* issuerCN,
           const char* subjectCN,
           EndEntityOrCA endEntityOrCA,
           const TestSignatureAlgorithm& signatureAlgorithm,
           /*out*/ ByteString& subjectDER)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString issuerDER(CNToDERName(issuerCN));
  EXPECT_FALSE(ENCODING_FAILED(issuerDER));
  subjectDER = CNToDERName(subjectCN);
  EXPECT_FALSE(ENCODING_FAILED(subjectDER));

  ByteString extensions[2];
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    extensions[0] =
      CreateEncodedBasicConstraints(true, nullptr, Critical::Yes);
    EXPECT_FALSE(ENCODING_FAILED(extensions[0]));
  }

  ScopedTestKeyPair reusedKey(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(v3, signatureAlgorithm,
                                              serialNumber, issuerDER,
                                              oneDayBeforeNow, oneDayAfterNow,
                                              subjectDER, *reusedKey,
                                              extensions, *reusedKey,
                                              signatureAlgorithm));
  EXPECT_FALSE(ENCODING_FAILED(certDER));
  return certDER;
}

class AlgorithmTestsTrustDomain final : public DefaultCryptoTrustDomain
{
public:
  AlgorithmTestsTrustDomain(const ByteString& aRootDER,
                            const ByteString& aRootSubjectDER,
               /*optional*/ const ByteString& aIntDER,
               /*optional*/ const ByteString& aIntSubjectDER)
    : rootDER(aRootDER)
    , rootSubjectDER(aRootSubjectDER)
    , intDER(aIntDER)
    , intSubjectDER(aIntSubjectDER)
  {
  }

private:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      /*out*/ TrustLevel& trustLevel) override
  {
    if (InputEqualsByteString(candidateCert, rootDER)) {
      trustLevel = TrustLevel::TrustAnchor;
    } else {
      trustLevel = TrustLevel::InheritsTrust;
    }
    return Success;
  }

  Result FindIssuer(Input encodedIssuerName, IssuerChecker& checker, Time)
                    override
  {
    ByteString* issuerDER = nullptr;
    if (InputEqualsByteString(encodedIssuerName, rootSubjectDER)) {
      issuerDER = &rootDER;
    } else if (InputEqualsByteString(encodedIssuerName, intSubjectDER)) {
      issuerDER = &intDER;
    } else {
      // FindIssuer just returns success if it can't find a potential issuer.
      return Success;
    }
    Input issuerCert;
    Result rv = issuerCert.Init(issuerDER->data(), issuerDER->length());
    if (rv != Success) {
      return rv;
    }
    bool keepGoing;
    return checker.Check(issuerCert, nullptr, keepGoing);
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         const Input*, const Input*) override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
  {
    return Success;
  }

  ByteString rootDER;
  ByteString rootSubjectDER;
  ByteString intDER;
  ByteString intSubjectDER;
};

static const TestSignatureAlgorithm NO_INTERMEDIATE
{
  TestPublicKeyAlgorithm(ByteString()),
  TestDigestAlgorithmID::MD2,
  ByteString(),
  false
};

struct ChainValidity final
{
  ChainValidity(const TestSignatureAlgorithm& aEndEntitySignatureAlgorithm,
                const TestSignatureAlgorithm& aOptionalIntSignatureAlgorithm,
                const TestSignatureAlgorithm& aRootSignatureAlgorithm,
                bool aIsValid)
    : endEntitySignatureAlgorithm(aEndEntitySignatureAlgorithm)
    , optionalIntermediateSignatureAlgorithm(aOptionalIntSignatureAlgorithm)
    , rootSignatureAlgorithm(aRootSignatureAlgorithm)
    , isValid(aIsValid)
  { }

  // In general, a certificate is generated for each of these.  However, if
  // optionalIntermediateSignatureAlgorithm is NO_INTERMEDIATE, then only 2
  // certificates are generated.
  // The certificate generated for the given rootSignatureAlgorithm is the
  // trust anchor.
  TestSignatureAlgorithm endEntitySignatureAlgorithm;
  TestSignatureAlgorithm optionalIntermediateSignatureAlgorithm;
  TestSignatureAlgorithm rootSignatureAlgorithm;
  bool isValid;
};

static const ChainValidity CHAIN_VALIDITY[] =
{
  // The trust anchor may have a signature with an unsupported signature
  // algorithm.
  ChainValidity(sha256WithRSAEncryption(),
                NO_INTERMEDIATE,
                md5WithRSAEncryption(),
                true),
  ChainValidity(sha256WithRSAEncryption(),
                NO_INTERMEDIATE,
                md2WithRSAEncryption(),
                true),

  // Certificates that are not trust anchors must not have a signature with an
  // unsupported signature algorithm.
  ChainValidity(md5WithRSAEncryption(),
                NO_INTERMEDIATE,
                sha256WithRSAEncryption(),
                false),
  ChainValidity(md2WithRSAEncryption(),
                NO_INTERMEDIATE,
                sha256WithRSAEncryption(),
                false),
  ChainValidity(md2WithRSAEncryption(),
                NO_INTERMEDIATE,
                md5WithRSAEncryption(),
                false),
  ChainValidity(sha256WithRSAEncryption(),
                md5WithRSAEncryption(),
                sha256WithRSAEncryption(),
                false),
  ChainValidity(sha256WithRSAEncryption(),
                md2WithRSAEncryption(),
                sha256WithRSAEncryption(),
                false),
  ChainValidity(sha256WithRSAEncryption(),
                md2WithRSAEncryption(),
                md5WithRSAEncryption(),
                false),
};

class pkixcert_IsValidChainForAlgorithm
  : public ::testing::Test
  , public ::testing::WithParamInterface<ChainValidity>
{
};

::std::ostream& operator<<(::std::ostream& os,
                           const pkixcert_IsValidChainForAlgorithm&)
{
  return os << "TODO (bug 1318770)";
}

::std::ostream& operator<<(::std::ostream& os, const ChainValidity&)
{
  return os << "TODO (bug 1318770)";
}

TEST_P(pkixcert_IsValidChainForAlgorithm, IsValidChainForAlgorithm)
{
  const ChainValidity& chainValidity(GetParam());
  const char* rootCN = "CN=Root";
  ByteString rootSubjectDER;
  ByteString rootEncoded(
    CreateCert(rootCN, rootCN, EndEntityOrCA::MustBeCA,
               chainValidity.rootSignatureAlgorithm, rootSubjectDER));
  EXPECT_FALSE(ENCODING_FAILED(rootEncoded));
  EXPECT_FALSE(ENCODING_FAILED(rootSubjectDER));

  const char* issuerCN = rootCN;

  const char* intermediateCN = "CN=Intermediate";
  ByteString intermediateSubjectDER;
  ByteString intermediateEncoded;

  // If the the algorithmIdentifier is empty, then it's NO_INTERMEDIATE.
  if (!chainValidity.optionalIntermediateSignatureAlgorithm
                    .algorithmIdentifier.empty()) {
    intermediateEncoded =
      CreateCert(rootCN, intermediateCN, EndEntityOrCA::MustBeCA,
                 chainValidity.optionalIntermediateSignatureAlgorithm,
                 intermediateSubjectDER);
    EXPECT_FALSE(ENCODING_FAILED(intermediateEncoded));
    EXPECT_FALSE(ENCODING_FAILED(intermediateSubjectDER));
    issuerCN = intermediateCN;
  }

  AlgorithmTestsTrustDomain trustDomain(rootEncoded, rootSubjectDER,
                                        intermediateEncoded,
                                        intermediateSubjectDER);

  const char* endEntityCN = "CN=End Entity";
  ByteString endEntitySubjectDER;
  ByteString endEntityEncoded(
    CreateCert(issuerCN, endEntityCN, EndEntityOrCA::MustBeEndEntity,
               chainValidity.endEntitySignatureAlgorithm,
               endEntitySubjectDER));
  EXPECT_FALSE(ENCODING_FAILED(endEntityEncoded));
  EXPECT_FALSE(ENCODING_FAILED(endEntitySubjectDER));

  Input endEntity;
  ASSERT_EQ(Success, endEntity.Init(endEntityEncoded.data(),
                                    endEntityEncoded.length()));
  Result expectedResult = chainValidity.isValid
                        ? Success
                        : Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  ASSERT_EQ(expectedResult,
            BuildCertChain(trustDomain, endEntity, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy, nullptr));
}

INSTANTIATE_TEST_CASE_P(pkixcert_IsValidChainForAlgorithm,
                        pkixcert_IsValidChainForAlgorithm,
                        testing::ValuesIn(CHAIN_VALIDITY));
