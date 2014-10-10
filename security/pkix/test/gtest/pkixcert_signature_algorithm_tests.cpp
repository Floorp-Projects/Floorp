/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

#include "cert.h"
#include "nssgtest.h"
#include "pkix/pkix.h"
#include "pkixgtest.h"
#include "pkixtestutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

typedef ScopedPtr<CERTCertificate, CERT_DestroyCertificate>
          ScopedCERTCertificate;

static bool
CreateCert(PLArenaPool* arena, const char* issuerCN,
           const char* subjectCN, EndEntityOrCA endEntityOrCA,
           SECOidTag signatureAlgorithm,
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

  const SECItem* issuerDER(ASCIIToDERName(arena, issuerCN));
  if (!issuerDER) {
    return false;
  }
  const SECItem* subjectDER(ASCIIToDERName(arena, subjectCN));
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

  SECOidTag signatureHashAlgorithm = SEC_OID_UNKNOWN;
  if (signatureAlgorithm == SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION) {
    signatureHashAlgorithm = SEC_OID_SHA256;
  } else if (signatureAlgorithm == SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION) {
    signatureHashAlgorithm = SEC_OID_MD5;
  } else if (signatureAlgorithm == SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION) {
    signatureHashAlgorithm = SEC_OID_MD2;
  }

  SECItem* certDER(CreateEncodedCertificate(arena, v3, signatureAlgorithm,
                                            serialNumber, issuerDER,
                                            oneDayBeforeNow,
                                            oneDayAfterNow,
                                            subjectDER, extensions,
                                            issuerKey, signatureHashAlgorithm,
                                            subjectKey));
  if (!certDER) {
    return false;
  }
  subjectCert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(), certDER,
                                        nullptr, false, true);
  return subjectCert.get() != nullptr;
}

class AlgorithmTestsTrustDomain : public TrustDomain
{
public:
  AlgorithmTestsTrustDomain(CERTCertificate* rootCert,
                            CERTCertificate* intermediateCert)
    : rootCert(rootCert)
    , intermediateCert(intermediateCert)
  {
  }

private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              Input candidateCert,
                              /*out*/ TrustLevel& trustLevel)
  {
    Input rootDER;
    Result rv = rootDER.Init(rootCert->derCert.data,
                             rootCert->derCert.len);
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
    bool keepGoing;
    Input rootSubject;
    Result rv = rootSubject.Init(rootCert->derSubject.data,
                                 rootCert->derSubject.len);
    EXPECT_EQ(Success, rv);
    if (InputsAreEqual(encodedIssuerName, rootSubject)) {
      Input rootDER;
      rv = rootDER.Init(rootCert->derCert.data, rootCert->derCert.len);
      EXPECT_EQ(Success, rv);
      return checker.Check(rootDER, nullptr, keepGoing);
    }

    Input intermediateSubject;
    rv = intermediateSubject.Init(intermediateCert->derSubject.data,
                                  intermediateCert->derSubject.len);
    EXPECT_EQ(Success, rv);
    if (InputsAreEqual(encodedIssuerName, intermediateSubject)) {
      Input intermediateDER;
      rv = intermediateDER.Init(intermediateCert->derCert.data,
                                intermediateCert->derCert.len);
      EXPECT_EQ(Success, rv);
      return checker.Check(intermediateDER, nullptr, keepGoing);
    }

    // FindIssuer just returns success if it can't find a potential issuer.
    return Success;
  }

  virtual Result CheckRevocation(EndEntityOrCA, const CertID&, Time,
                                 /*optional*/ const Input*,
                                 /*optional*/ const Input*)
  {
    return Success;
  }

  virtual Result IsChainValid(const DERArray&)
  {
    return Success;
  }

  virtual Result VerifySignedData(const SignedDataWithSignature& signedData,
                                  Input subjectPublicKeyInfo)
  {
    EXPECT_NE(SignatureAlgorithm::unsupported_algorithm, signedData.algorithm);
    return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                             nullptr);
  }

  virtual Result DigestBuf(Input, uint8_t*, size_t)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckPublicKey(Input subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
  }

  ScopedCERTCertificate rootCert;
  ScopedCERTCertificate intermediateCert;
};

static const SECOidTag NO_INTERMEDIATE = SEC_OID_UNKNOWN;

struct ChainValidity
{
  // In general, a certificate is generated for each of these.  However, if
  // optionalIntermediateSignatureAlgorithm is NO_INTERMEDIATE, then only 2
  // certificates are generated.
  // The certificate generated for the given rootSignatureAlgorithm is the
  // trust anchor.
  SECOidTag endEntitySignatureAlgorithm;
  SECOidTag optionalIntermediateSignatureAlgorithm;
  SECOidTag rootSignatureAlgorithm;
  bool isValid;
};

static const ChainValidity CHAIN_VALIDITY[] =
{
  // The trust anchor may have a signature with an unsupported signature
  // algorithm.
  { SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    NO_INTERMEDIATE,
    SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    true
  },
  { SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    NO_INTERMEDIATE,
    SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
    true
  },

  // Certificates that are not trust anchors must not have a signature with an
  // unsupported signature algorithm.
  { SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    NO_INTERMEDIATE,
    SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    false
  },
  { SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
    NO_INTERMEDIATE,
    SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    false
  },
  { SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
    NO_INTERMEDIATE,
    SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    false
  },
  { SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    false
  },
  { SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    false
  },
  { SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION,
    SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    false
  },
};

class pkixcert_IsValidChainForAlgorithm
  : public NSSTest
  , public ::testing::WithParamInterface<ChainValidity>
{
public:
  static void SetUpTestCase()
  {
    NSSTest::SetUpTestCase();
  }
};

TEST_P(pkixcert_IsValidChainForAlgorithm, IsValidChainForAlgorithm)
{
  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  ASSERT_TRUE(arena.get());

  const ChainValidity& chainValidity(GetParam());
  const char* rootCN = "CN=Root";
  ScopedSECKEYPrivateKey rootKey;
  ScopedCERTCertificate rootCert;
  EXPECT_TRUE(CreateCert(arena.get(), rootCN, rootCN, EndEntityOrCA::MustBeCA,
                         chainValidity.rootSignatureAlgorithm, nullptr,
                         rootKey, rootCert));

  const char* issuerCN = rootCN;

  const char* intermediateCN = "CN=Intermediate";
  SECKEYPrivateKey* issuerKey = rootKey.get();
  ScopedSECKEYPrivateKey intermediateKey;
  ScopedCERTCertificate intermediateCert;
  SECOidTag intermediateSignatureAlgorithm =
    chainValidity.optionalIntermediateSignatureAlgorithm;
  if (intermediateSignatureAlgorithm != NO_INTERMEDIATE) {
    EXPECT_TRUE(CreateCert(arena.get(), rootCN, intermediateCN,
                           EndEntityOrCA::MustBeCA,
                           intermediateSignatureAlgorithm,
                           rootKey.get(), intermediateKey, intermediateCert));
    issuerCN = intermediateCN;
    issuerKey = intermediateKey.get();
  }

  AlgorithmTestsTrustDomain trustDomain(rootCert.release(),
                                        intermediateCert.release());

  const char* endEntityCN = "CN=End Entity";
  ScopedSECKEYPrivateKey unusedEndEntityKey;
  ScopedCERTCertificate endEntityCert;
  EXPECT_TRUE(CreateCert(arena.get(), issuerCN, endEntityCN,
                         EndEntityOrCA::MustBeEndEntity,
                         chainValidity.endEntitySignatureAlgorithm,
                         issuerKey, unusedEndEntityKey, endEntityCert));
  Result expectedResult = chainValidity.isValid
                        ? Success
                        : Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  Input endEntityDER;
  EXPECT_EQ(Success, endEntityDER.Init(endEntityCert->derCert.data,
                                       endEntityCert->derCert.len));
  ASSERT_EQ(expectedResult,
            BuildCertChain(trustDomain, endEntityDER, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           KeyPurposeId::id_kp_serverAuth,
                           CertPolicyId::anyPolicy, nullptr));
}

INSTANTIATE_TEST_CASE_P(pkixcert_IsValidChainForAlgorithm,
                        pkixcert_IsValidChainForAlgorithm,
                        testing::ValuesIn(CHAIN_VALIDITY));
