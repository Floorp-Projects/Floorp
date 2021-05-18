/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "pkixgtest.h"

#include "mozpkix/pkixc.h"
#include "mozpkix/pkixder.h"
#include "mozpkix/pkixnss.h"
#include "secerr.h"
#include "sslerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

static ByteString CreateCert(
    const char* issuerCN, const char* subjectCN, EndEntityOrCA endEntityOrCA,
    /*optional*/ const ByteString* subjectAlternativeNameExtension = nullptr,
    /*optional*/ const ByteString* extendedKeyUsageExtension = nullptr) {
  EXPECT_TRUE(issuerCN);
  EXPECT_TRUE(subjectCN);
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString issuerDER(CNToDERName(issuerCN));
  ByteString subjectDER(CNToDERName(subjectCN));

  std::time_t notBefore = 1620000000;
  std::time_t notAfter = 1630000000;

  std::vector<ByteString> extensions;
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    ByteString basicConstraints =
        CreateEncodedBasicConstraints(true, nullptr, Critical::Yes);
    EXPECT_FALSE(ENCODING_FAILED(basicConstraints));
    extensions.push_back(basicConstraints);
  }
  if (subjectAlternativeNameExtension) {
    extensions.push_back(*subjectAlternativeNameExtension);
  }
  if (extendedKeyUsageExtension) {
    extensions.push_back(*extendedKeyUsageExtension);
  }
  extensions.push_back(ByteString());  // marks the end of the list

  ScopedTestKeyPair reusedKey(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(
      v3, sha256WithRSAEncryption(), serialNumber, issuerDER, notBefore,
      notAfter, subjectDER, *reusedKey, extensions.data(), *reusedKey,
      sha256WithRSAEncryption()));
  EXPECT_FALSE(ENCODING_FAILED(certDER));

  return certDER;
}

class pkixc_tests : public ::testing::Test {};

TEST_F(pkixc_tests, Valid_VerifyCodeSigningCertificateChain) {
  ByteString root(CreateCert("CA", "CA", EndEntityOrCA::MustBeCA));
  ByteString intermediate(
      CreateCert("CA", "intermediate", EndEntityOrCA::MustBeCA));
  ByteString subjectAltNameExtension =
      CreateEncodedSubjectAltName(DNSName("example.com"));
  ByteString endEntity(CreateCert("intermediate", "end-entity",
                                  EndEntityOrCA::MustBeEndEntity,
                                  &subjectAltNameExtension));
  const uint8_t* certificates[] = {endEntity.data(), intermediate.data(),
                                   root.data()};
  const uint16_t certificateLengths[] = {
      static_cast<uint16_t>(endEntity.length()),
      static_cast<uint16_t>(intermediate.length()),
      static_cast<uint16_t>(root.length())};
  const size_t numCertificates = 3;
  const uint64_t secondsSinceEpoch = 1625000000;
  uint8_t rootSHA256Digest[32] = {0};
  Input rootInput;
  Result rv = rootInput.Init(root.data(), root.length());
  ASSERT_EQ(rv, Success);
  rv = DigestBufNSS(rootInput, DigestAlgorithm::sha256, rootSHA256Digest,
                    sizeof(rootSHA256Digest));
  ASSERT_EQ(rv, Success);
  const uint8_t hostname[] = {"example.com"};
  size_t hostnameLength = strlen("example.com");
  PRErrorCode error = 0;
  ASSERT_TRUE(VerifyCodeSigningCertificateChain(
      &certificates[0], &certificateLengths[0], numCertificates,
      secondsSinceEpoch, &rootSHA256Digest[0], &hostname[0], hostnameLength,
      &error));

  // If the extended key usage extension is present, it must have the code
  // signing usage.
  ByteString extendedKeyUsageExtension(
      CreateEKUExtension(BytesToByteString(tlv_id_kp_codeSigning)));
  ByteString endEntityWithEKU(
      CreateCert("intermediate", "end-entity", EndEntityOrCA::MustBeEndEntity,
                 &subjectAltNameExtension, &extendedKeyUsageExtension));
  const uint8_t* certificatesWithEKU[] = {endEntityWithEKU.data(),
                                          intermediate.data(), root.data()};
  const uint16_t certificateLengthsWithEKU[] = {
      static_cast<uint16_t>(endEntityWithEKU.length()),
      static_cast<uint16_t>(intermediate.length()),
      static_cast<uint16_t>(root.length())};
  ASSERT_TRUE(VerifyCodeSigningCertificateChain(
      &certificatesWithEKU[0], &certificateLengthsWithEKU[0], numCertificates,
      secondsSinceEpoch, &rootSHA256Digest[0], &hostname[0], hostnameLength,
      &error));
}

TEST_F(pkixc_tests, Invalid_VerifyCodeSigningCertificateChain) {
  ByteString root(CreateCert("CA", "CA", EndEntityOrCA::MustBeCA));
  ByteString subjectAltNameExtension =
      CreateEncodedSubjectAltName(DNSName("example.com"));
  ByteString endEntity(CreateCert("CA", "end-entity",
                                  EndEntityOrCA::MustBeEndEntity,
                                  &subjectAltNameExtension));
  const uint8_t* certificates[] = {endEntity.data(), root.data()};
  const uint16_t certificateLengths[] = {
      static_cast<uint16_t>(endEntity.length()),
      static_cast<uint16_t>(root.length())};
  const size_t numCertificates = 2;
  const uint64_t secondsSinceEpoch = 1625000000;
  uint8_t rootSHA256Digest[32] = {0};
  Input rootInput;
  Result rv = rootInput.Init(root.data(), root.length());
  ASSERT_EQ(rv, Success);
  rv = DigestBufNSS(rootInput, DigestAlgorithm::sha256, rootSHA256Digest,
                    sizeof(rootSHA256Digest));
  ASSERT_EQ(rv, Success);
  const uint8_t hostname[] = {"example.com"};
  size_t hostnameLength = strlen("example.com");
  PRErrorCode error = 0;
  // Consistency check first to ensure these tests are meaningful.
  ASSERT_TRUE(VerifyCodeSigningCertificateChain(
      &certificates[0], &certificateLengths[0], numCertificates,
      secondsSinceEpoch, &rootSHA256Digest[0], &hostname[0], hostnameLength,
      &error));
  ASSERT_EQ(error, 0);

  // Test with "now" after the certificates have expired.
  ASSERT_FALSE(VerifyCodeSigningCertificateChain(
      &certificates[0], &certificateLengths[0], numCertificates,
      secondsSinceEpoch + 10000000, &rootSHA256Digest[0], &hostname[0],
      hostnameLength, &error));
  ASSERT_EQ(error, SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE);

  // Test with a different root digest.
  uint8_t wrongRootSHA256Digest[32] = {1};
  ASSERT_FALSE(VerifyCodeSigningCertificateChain(
      &certificates[0], &certificateLengths[0], numCertificates,
      secondsSinceEpoch, &wrongRootSHA256Digest[0], &hostname[0],
      hostnameLength, &error));
  ASSERT_EQ(error, SEC_ERROR_UNKNOWN_ISSUER);

  // Test with a different host name.
  const uint8_t wrongHostname[] = "example.org";
  size_t wrongHostnameLength = strlen("example.org");
  ASSERT_FALSE(VerifyCodeSigningCertificateChain(
      &certificates[0], &certificateLengths[0], numCertificates,
      secondsSinceEpoch, &rootSHA256Digest[0], &wrongHostname[0],
      wrongHostnameLength, &error));
  ASSERT_EQ(error, SSL_ERROR_BAD_CERT_DOMAIN);

  // Test with a certificate with an extended key usage that doesn't include
  // code signing.
  ByteString extendedKeyUsageExtension(
      CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)));
  ByteString endEntityWithEKU(
      CreateCert("CA", "end-entity", EndEntityOrCA::MustBeEndEntity,
                 &subjectAltNameExtension, &extendedKeyUsageExtension));
  const uint8_t* certificatesWithEKU[] = {endEntityWithEKU.data(), root.data()};
  const uint16_t certificateLengthsWithEKU[] = {
      static_cast<uint16_t>(endEntityWithEKU.length()),
      static_cast<uint16_t>(root.length())};
  ASSERT_FALSE(VerifyCodeSigningCertificateChain(
      &certificatesWithEKU[0], &certificateLengthsWithEKU[0], numCertificates,
      secondsSinceEpoch, &rootSHA256Digest[0], &hostname[0], hostnameLength,
      &error));
  ASSERT_EQ(error, SEC_ERROR_INADEQUATE_CERT_TYPE);
}
