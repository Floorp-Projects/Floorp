/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "gtest/gtest.h"
#include "scoped_ptrs.h"

#include "pk11_rsapss_vectors.h"

namespace nss_test {

static unsigned char* toUcharPtr(const uint8_t* v) {
  return const_cast<unsigned char*>(static_cast<const unsigned char*>(v));
}

class Pkcs11RsaPssTest : public ::testing::Test {};

class Pkcs11RsaPssVectorTest : public Pkcs11RsaPssTest {
 public:
  void Verify(const uint8_t* spki, size_t spki_len, const uint8_t* data,
              size_t data_len, const uint8_t* sig, size_t sig_len) {
    // Verify data signed with PSS/SHA-1.
    SECOidTag hashOid = SEC_OID_SHA1;
    CK_MECHANISM_TYPE hashMech = CKM_SHA_1;
    CK_RSA_PKCS_MGF_TYPE mgf = CKG_MGF1_SHA1;

    // Set up PSS parameters.
    unsigned int hLen = HASH_ResultLenByOidTag(hashOid);
    CK_RSA_PKCS_PSS_PARAMS rsaPssParams = {hashMech, mgf, hLen};
    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&rsaPssParams),
                      sizeof(rsaPssParams)};

    // Import public key.
    SECItem spkiItem = {siBuffer, toUcharPtr(spki),
                        static_cast<unsigned int>(spki_len)};
    ScopedCERTSubjectPublicKeyInfo certSpki(
        SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));
    ScopedSECKEYPublicKey pubKey(SECKEY_ExtractPublicKey(certSpki.get()));

    // Hash the data.
    std::vector<uint8_t> hashBuf(hLen);
    SECItem hash = {siBuffer, hashBuf.data(),
                    static_cast<unsigned int>(hashBuf.size())};
    SECStatus rv = PK11_HashBuf(hashOid, hash.data, toUcharPtr(data), data_len);
    EXPECT_EQ(rv, SECSuccess);

    // Verify.
    CK_MECHANISM_TYPE mech = CKM_RSA_PKCS_PSS;
    SECItem sigItem = {siBuffer, toUcharPtr(sig),
                       static_cast<unsigned int>(sig_len)};
    rv = PK11_VerifyWithMechanism(pubKey.get(), mech, &params, &sigItem, &hash,
                                  nullptr);
    EXPECT_EQ(rv, SECSuccess);
  }

  void SignAndVerify(const uint8_t* pkcs8, size_t pkcs8_len,
                     const uint8_t* spki, size_t spki_len, const uint8_t* data,
                     size_t data_len) {
    // Sign with PSS/SHA-1.
    SECOidTag hashOid = SEC_OID_SHA1;
    CK_MECHANISM_TYPE hashMech = CKM_SHA_1;
    CK_RSA_PKCS_MGF_TYPE mgf = CKG_MGF1_SHA1;

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_TRUE(slot);

    SECItem pkcs8Item = {siBuffer, toUcharPtr(pkcs8),
                         static_cast<unsigned int>(pkcs8_len)};

    // Import PKCS #8.
    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &pkcs8Item, nullptr, nullptr, false, false, KU_ALL, &key,
        nullptr);
    EXPECT_TRUE(rv == SECSuccess && !!key);
    ScopedSECKEYPrivateKey privKey(key);

    // Set up PSS parameters.
    unsigned int hLen = HASH_ResultLenByOidTag(hashOid);
    CK_RSA_PKCS_PSS_PARAMS rsaPssParams = {hashMech, mgf, hLen};
    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&rsaPssParams),
                      sizeof(rsaPssParams)};

    // Hash the data.
    std::vector<uint8_t> hashBuf(hLen);
    SECItem hash = {siBuffer, hashBuf.data(),
                    static_cast<unsigned int>(hashBuf.size())};
    rv = PK11_HashBuf(hashOid, hash.data, toUcharPtr(data), data_len);
    EXPECT_EQ(rv, SECSuccess);

    // Prepare signature buffer.
    uint32_t len = PK11_SignatureLen(privKey.get());
    std::vector<uint8_t> sigBuf(len);
    SECItem sig = {siBuffer, sigBuf.data(),
                   static_cast<unsigned int>(sigBuf.size())};

    CK_MECHANISM_TYPE mech = CKM_RSA_PKCS_PSS;
    rv = PK11_SignWithMechanism(privKey.get(), mech, &params, &sig, &hash);
    EXPECT_EQ(rv, SECSuccess);

    // Verify.
    Verify(spki, spki_len, data, data_len, sig.data, sig.len);
  }
};

#define PSS_TEST_VECTOR_VERIFY(spki, data, sig) \
  Verify(spki, sizeof(spki), data, sizeof(data), sig, sizeof(sig));

#define PSS_TEST_VECTOR_SIGN_VERIFY(pkcs8, spki, data) \
  SignAndVerify(pkcs8, sizeof(pkcs8), spki, sizeof(spki), data, sizeof(data));

TEST_F(Pkcs11RsaPssTest, GenerateAndSignAndVerify) {
  // Sign data with a 1024-bit RSA key, using PSS/SHA-256.
  SECOidTag hashOid = SEC_OID_SHA256;
  CK_MECHANISM_TYPE hashMech = CKM_SHA256;
  CK_RSA_PKCS_MGF_TYPE mgf = CKG_MGF1_SHA256;
  PK11RSAGenParams rsaGenParams = {1024, 0x10001};

  // Generate RSA key pair.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  SECKEYPublicKey* pubKeyRaw = nullptr;
  ScopedSECKEYPrivateKey privKey(
      PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaGenParams,
                           &pubKeyRaw, false, false, nullptr));
  ASSERT_TRUE(!!privKey && pubKeyRaw);
  ScopedSECKEYPublicKey pubKey(pubKeyRaw);

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
  CK_RSA_PKCS_PSS_PARAMS rsaPssParams = {hashMech, mgf, hLen};
  SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&rsaPssParams),
                    sizeof(rsaPssParams)};

  // Sign.
  CK_MECHANISM_TYPE mech = CKM_RSA_PKCS_PSS;
  rv = PK11_SignWithMechanism(privKey.get(), mech, &params, &sig, &data);
  EXPECT_EQ(rv, SECSuccess);

  // Verify.
  rv = PK11_VerifyWithMechanism(pubKey.get(), mech, &params, &sig, &data,
                                nullptr);
  EXPECT_EQ(rv, SECSuccess);

  // Verification with modified data must fail.
  data.data[0] ^= 0xff;
  rv = PK11_VerifyWithMechanism(pubKey.get(), mech, &params, &sig, &data,
                                nullptr);
  EXPECT_EQ(rv, SECFailure);

  // Verification with original data but the wrong signature must fail.
  data.data[0] ^= 0xff;  // Revert previous changes.
  sig.data[0] ^= 0xff;
  rv = PK11_VerifyWithMechanism(pubKey.get(), mech, &params, &sig, &data,
                                nullptr);
  EXPECT_EQ(rv, SECFailure);
}

// RSA-PSS test vectors, pss-vect.txt, Example 1.1: A 1024-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature1) {
  PSS_TEST_VECTOR_VERIFY(kTestVector1Spki, kTestVector1Data, kTestVector1Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify1) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector1Pkcs8, kTestVector1Spki,
                              kTestVector1Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 2.1: A 1025-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature2) {
  PSS_TEST_VECTOR_VERIFY(kTestVector2Spki, kTestVector2Data, kTestVector2Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify2) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector2Pkcs8, kTestVector2Spki,
                              kTestVector2Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 3.1: A 1026-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature3) {
  PSS_TEST_VECTOR_VERIFY(kTestVector3Spki, kTestVector3Data, kTestVector3Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify3) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector3Pkcs8, kTestVector3Spki,
                              kTestVector3Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 4.1: A 1027-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature4) {
  PSS_TEST_VECTOR_VERIFY(kTestVector4Spki, kTestVector4Data, kTestVector4Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify4) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector4Pkcs8, kTestVector4Spki,
                              kTestVector4Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 5.1: A 1028-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature5) {
  PSS_TEST_VECTOR_VERIFY(kTestVector5Spki, kTestVector5Data, kTestVector5Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify5) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector5Pkcs8, kTestVector5Spki,
                              kTestVector5Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 6.1: A 1029-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature6) {
  PSS_TEST_VECTOR_VERIFY(kTestVector6Spki, kTestVector6Data, kTestVector6Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify6) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector6Pkcs8, kTestVector6Spki,
                              kTestVector6Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 7.1: A 1030-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature7) {
  PSS_TEST_VECTOR_VERIFY(kTestVector7Spki, kTestVector7Data, kTestVector7Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify7) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector7Pkcs8, kTestVector7Spki,
                              kTestVector7Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 8.1: A 1031-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature8) {
  PSS_TEST_VECTOR_VERIFY(kTestVector8Spki, kTestVector8Data, kTestVector8Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify8) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector8Pkcs8, kTestVector8Spki,
                              kTestVector8Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 9.1: A 1536-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature9) {
  PSS_TEST_VECTOR_VERIFY(kTestVector9Spki, kTestVector9Data, kTestVector9Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify9) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector9Pkcs8, kTestVector9Spki,
                              kTestVector9Data);
}

// RSA-PSS test vectors, pss-vect.txt, Example 10.1: A 2048-bit RSA Key Pair
// <ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1-vec.zip>
TEST_F(Pkcs11RsaPssVectorTest, VerifyKnownSignature10) {
  PSS_TEST_VECTOR_VERIFY(kTestVector10Spki, kTestVector10Data,
                         kTestVector10Sig);
}
TEST_F(Pkcs11RsaPssVectorTest, SignAndVerify10) {
  PSS_TEST_VECTOR_SIGN_VERIFY(kTestVector10Pkcs8, kTestVector10Spki,
                              kTestVector10Data);
}

}  // namespace nss_test
