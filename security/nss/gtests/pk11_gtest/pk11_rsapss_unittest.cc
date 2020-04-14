/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "cpputil.h"
#include "databuffer.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include "pk11_signature_test.h"
#include "pk11_rsapss_vectors.h"

#include "testvectors/rsa_pss_2048_sha256_mgf1_32-vectors.h"
#include "testvectors/rsa_pss_2048_sha1_mgf1_20-vectors.h"
#include "testvectors/rsa_pss_2048_sha256_mgf1_0-vectors.h"
#include "testvectors/rsa_pss_3072_sha256_mgf1_32-vectors.h"
#include "testvectors/rsa_pss_4096_sha256_mgf1_32-vectors.h"
#include "testvectors/rsa_pss_4096_sha512_mgf1_32-vectors.h"
#include "testvectors/rsa_pss_misc-vectors.h"

namespace nss_test {

class Pkcs11RsaPssTestWycheproof
    : public ::testing::TestWithParam<RsaPssTestVector> {
 protected:
  void TestPss(const RsaPssTestVector& vec) {
    SECItem spki_item = {siBuffer, toUcharPtr(vec.public_key.data()),
                         static_cast<unsigned int>(vec.public_key.size())};

    ScopedCERTSubjectPublicKeyInfo cert_spki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_item));
    ASSERT_TRUE(cert_spki);

    ScopedSECKEYPublicKey pub_key(SECKEY_ExtractPublicKey(cert_spki.get()));
    ASSERT_TRUE(pub_key);

    DataBuffer hash;
    hash.Allocate(static_cast<size_t>(HASH_ResultLenByOidTag(vec.hash_oid)));
    SECStatus rv = PK11_HashBuf(vec.hash_oid, toUcharPtr(hash.data()),
                                toUcharPtr(vec.msg.data()), vec.msg.size());
    ASSERT_EQ(rv, SECSuccess);

    // Verify.
    SECItem hash_item = {siBuffer, toUcharPtr(hash.data()),
                         static_cast<unsigned int>(hash.len())};
    SECItem sig_item = {siBuffer, toUcharPtr(vec.sig.data()),
                        static_cast<unsigned int>(vec.sig.size())};
    CK_MECHANISM_TYPE hash_mech = 0;
    switch (vec.hash_oid) {
      case SEC_OID_SHA1:
        hash_mech = CKM_SHA_1;
        break;
      case SEC_OID_SHA224:
        hash_mech = CKM_SHA224;
        break;
      case SEC_OID_SHA256:
        hash_mech = CKM_SHA256;
        break;
      case SEC_OID_SHA384:
        hash_mech = CKM_SHA384;
        break;
      case SEC_OID_SHA512:
        hash_mech = CKM_SHA512;
        break;
      default:
        ASSERT_TRUE(hash_mech);
        return;
    }

    CK_RSA_PKCS_PSS_PARAMS pss_params = {hash_mech, vec.mgf_hash, vec.sLen};
    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&pss_params),
                      sizeof(pss_params)};

    rv = PK11_VerifyWithMechanism(pub_key.get(), CKM_RSA_PKCS_PSS, &params,
                                  &sig_item, &hash_item, nullptr);
    EXPECT_EQ(vec.valid ? SECSuccess : SECFailure, rv);
  };
};

class Pkcs11RsaPssTest : public Pk11SignatureTest {
 public:
  Pkcs11RsaPssTest() : Pk11SignatureTest(CKM_RSA_PKCS_PSS, SEC_OID_SHA1) {
    pss_params_.hashAlg = CKM_SHA_1;
    pss_params_.mgf = CKG_MGF1_SHA1;
    pss_params_.sLen = HASH_ResultLenByOidTag(SEC_OID_SHA1);

    params_.type = siBuffer;
    params_.data = reinterpret_cast<unsigned char*>(&pss_params_);
    params_.len = sizeof(pss_params_);
  }

 protected:
  const SECItem* parameters() const { return &params_; }

 private:
  CK_RSA_PKCS_PSS_PARAMS pss_params_;
  SECItem params_;
};

TEST_F(Pkcs11RsaPssTest, GenerateAndSignAndVerify) {
  // Sign data with a 1024-bit RSA key, using PSS/SHA-256.
  SECOidTag hashOid = SEC_OID_SHA256;
  CK_MECHANISM_TYPE hash_mech = CKM_SHA256;
  CK_RSA_PKCS_MGF_TYPE mgf = CKG_MGF1_SHA256;
  PK11RSAGenParams rsaGenParams = {1024, 0x10001};

  // Generate RSA key pair.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  SECKEYPublicKey* pub_keyRaw = nullptr;
  ScopedSECKEYPrivateKey privKey(
      PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaGenParams,
                           &pub_keyRaw, false, false, nullptr));
  ASSERT_TRUE(!!privKey && pub_keyRaw);
  ScopedSECKEYPublicKey pub_key(pub_keyRaw);

  // Generate random data to sign.
  uint8_t dataBuf[50];
  SECItem data = {siBuffer, dataBuf, sizeof(dataBuf)};
  unsigned int hLen = HASH_ResultLenByOidTag(hashOid);
  SECStatus rv = PK11_GenerateRandomOnSlot(slot.get(), data.data, data.len);
  EXPECT_EQ(rv, SECSuccess);

  // Allocate memory for the signature.
  std::vector<uint8_t> sigBuf(PK11_SignatureLen(privKey.get()));
  SECItem sig = {siBuffer, &sigBuf[0],
                 static_cast<unsigned int>(sigBuf.size())};

  // Set up PSS parameters.
  CK_RSA_PKCS_PSS_PARAMS pss_params = {hash_mech, mgf, hLen};
  SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&pss_params),
                    sizeof(pss_params)};

  // Sign.
  rv = PK11_SignWithMechanism(privKey.get(), mechanism(), &params, &sig, &data);
  EXPECT_EQ(rv, SECSuccess);

  // Verify.
  rv = PK11_VerifyWithMechanism(pub_key.get(), mechanism(), &params, &sig,
                                &data, nullptr);
  EXPECT_EQ(rv, SECSuccess);

  // Verification with modified data must fail.
  data.data[0] ^= 0xff;
  rv = PK11_VerifyWithMechanism(pub_key.get(), mechanism(), &params, &sig,
                                &data, nullptr);
  EXPECT_EQ(rv, SECFailure);

  // Verification with original data but the wrong signature must fail.
  data.data[0] ^= 0xff;  // Revert previous changes.
  sig.data[0] ^= 0xff;
  rv = PK11_VerifyWithMechanism(pub_key.get(), mechanism(), &params, &sig,
                                &data, nullptr);
  EXPECT_EQ(rv, SECFailure);
}

TEST_F(Pkcs11RsaPssTest, NoLeakWithInvalidExponent) {
  // Attempt to generate an RSA key with a public exponent of 1. This should
  // fail, but it shouldn't leak memory.
  PK11RSAGenParams rsaGenParams = {1024, 0x01};

  // Generate RSA key pair.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  SECKEYPublicKey* pub_key = nullptr;
  SECKEYPrivateKey* privKey =
      PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaGenParams,
                           &pub_key, false, false, nullptr);
  EXPECT_FALSE(privKey);
  EXPECT_FALSE(pub_key);
}
class Pkcs11RsaPssVectorTest
    : public Pkcs11RsaPssTest,
      public ::testing::WithParamInterface<Pkcs11SignatureTestParams> {};

TEST_P(Pkcs11RsaPssVectorTest, Verify) { Verify(GetParam()); }

TEST_P(Pkcs11RsaPssVectorTest, SignAndVerify) { SignAndVerify(GetParam()); }

#define VECTOR(pkcs8, spki, data, sig)                                \
  {                                                                   \
    DataBuffer(pkcs8, sizeof(pkcs8)), DataBuffer(spki, sizeof(spki)), \
        DataBuffer(data, sizeof(data)), DataBuffer(sig, sizeof(sig))  \
  }
#define VECTOR_N(n)                                                         \
  VECTOR(kTestVector##n##Pkcs8, kTestVector##n##Spki, kTestVector##n##Data, \
         kTestVector##n##Sig)

static const Pkcs11SignatureTestParams kRsaPssVectors[] = {
    // RSA-PSS test vectors, pss-vect.txt, Example 1.1: A 1024-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(1),
    // RSA-PSS test vectors, pss-vect.txt, Example 2.1: A 1025-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(2),
    // RSA-PSS test vectors, pss-vect.txt, Example 3.1: A 1026-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(3),
    // RSA-PSS test vectors, pss-vect.txt, Example 4.1: A 1027-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(4),
    // RSA-PSS test vectors, pss-vect.txt, Example 5.1: A 1028-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(5),
    // RSA-PSS test vectors, pss-vect.txt, Example 6.1: A 1029-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(6),
    // RSA-PSS test vectors, pss-vect.txt, Example 7.1: A 1030-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(7),
    // RSA-PSS test vectors, pss-vect.txt, Example 8.1: A 1031-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(8),
    // RSA-PSS test vectors, pss-vect.txt, Example 9.1: A 1536-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(9),
    // RSA-PSS test vectors, pss-vect.txt, Example 10.1: A 2048-bit RSA Key Pair
    // <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
    VECTOR_N(10)};

INSTANTIATE_TEST_CASE_P(RsaPssSignVerify, Pkcs11RsaPssVectorTest,
                        ::testing::ValuesIn(kRsaPssVectors));

TEST_P(Pkcs11RsaPssTestWycheproof, Verify) { TestPss(GetParam()); }

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaPssSha120Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss2048Sha120WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaPssSha25632Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss2048Sha25632WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof2048RsaPssSha2560Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss2048Sha2560WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof3072RsaPssSha25632Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss3072Sha25632WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof4096RsaPssSha25632Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss4096Sha25632WycheproofVectors));

INSTANTIATE_TEST_CASE_P(
    Wycheproof4096RsaPssSha51232Test, Pkcs11RsaPssTestWycheproof,
    ::testing::ValuesIn(kRsaPss4096Sha51232WycheproofVectors));

INSTANTIATE_TEST_CASE_P(WycheproofRsaPssMiscTest, Pkcs11RsaPssTestWycheproof,
                        ::testing::ValuesIn(kRsaPssMiscWycheproofVectors));

}  // namespace nss_test
