// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include "blapi.h"
#include "nss_scoped_ptrs.h"
#include "kat/kyber768_kat.h"

namespace nss_test {

class Kyber768Test : public ::testing::Test {};

TEST(Kyber768Test, ConsistencyTest) {
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret2(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                              publicKey.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret2.get());
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(secret->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(secret2->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(0, memcmp(secret->data, secret2->data, KYBER_SHARED_SECRET_BYTES));
}

TEST(Kyber768Test, InvalidParameterTest) {
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber_invalid, nullptr, privateKey.get(),
                              publicKey.get());
  EXPECT_EQ(SECFailure, rv);

  rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                    publicKey.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Encapsulate(params_kyber_invalid, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECFailure, rv);

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Decapsulate(params_kyber_invalid, privateKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECFailure, rv);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);
}

TEST(Kyber768Test, InvalidPublicKeyTest) {
  ScopedSECItem shortBuffer(SECITEM_AllocItem(nullptr, nullptr, 7));
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                              shortBuffer.get());
  EXPECT_EQ(SECFailure, rv);  // short publicKey buffer
}

TEST(Kyber768Test, InvalidCiphertextTest) {
  ScopedSECItem shortBuffer(SECITEM_AllocItem(nullptr, nullptr, 7));
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret2(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                              publicKey.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         shortBuffer.get(), secret.get());
  EXPECT_EQ(SECFailure, rv);  // short ciphertext input

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);

  // Modify a random byte in the ciphertext
  size_t pos;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&pos, sizeof(pos));
  EXPECT_EQ(SECSuccess, rv);

  uint8_t byte;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&byte, sizeof(byte));
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(ciphertext->len, KYBER768_CIPHERTEXT_BYTES);
  ciphertext->data[pos % KYBER768_CIPHERTEXT_BYTES] ^= (byte | 1);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret2.get());
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(secret->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(secret2->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_NE(0, memcmp(secret->data, secret2->data, KYBER_SHARED_SECRET_BYTES));
}

TEST(Kyber768Test, InvalidPrivateKeyTest) {
  ScopedSECItem shortBuffer(SECITEM_AllocItem(nullptr, nullptr, 7));
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret2(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber768_round3, nullptr,
                              shortBuffer.get(), publicKey.get());
  EXPECT_EQ(SECFailure, rv);  // short privateKey buffer

  rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                    publicKey.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);

  // Modify a random byte in the private key
  size_t pos;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&pos, sizeof(pos));
  EXPECT_EQ(SECSuccess, rv);

  uint8_t byte;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&byte, sizeof(byte));
  EXPECT_EQ(SECSuccess, rv);

  // Modifying the implicit rejection key will not cause decapsulation failure.
  EXPECT_EQ(privateKey->len, KYBER768_PRIVATE_KEY_BYTES);
  privateKey
      ->data[pos % (KYBER768_PRIVATE_KEY_BYTES - KYBER_SHARED_SECRET_BYTES)] ^=
      (byte | 1);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret2.get());
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(secret->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(secret2->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_NE(0, memcmp(secret->data, secret2->data, KYBER_SHARED_SECRET_BYTES));
}

TEST(Kyber768Test, DecapsulationWithModifiedRejectionKeyTest) {
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret2(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret3(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv = Kyber_NewKey(params_kyber768_round3, nullptr, privateKey.get(),
                              publicKey.get());
  EXPECT_EQ(SECSuccess, rv);

  rv = Kyber_Encapsulate(params_kyber768_round3, nullptr, publicKey.get(),
                         ciphertext.get(), secret.get());
  EXPECT_EQ(SECSuccess, rv);

  // Modify a random byte in the ciphertext and decapsulate it
  size_t pos;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&pos, sizeof(pos));
  EXPECT_EQ(SECSuccess, rv);

  uint8_t byte;
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&byte, sizeof(byte));
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(ciphertext->len, KYBER768_CIPHERTEXT_BYTES);
  ciphertext->data[pos % KYBER768_CIPHERTEXT_BYTES] ^= (byte | 1);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret2.get());
  EXPECT_EQ(SECSuccess, rv);

  // Now, modify a random byte in the implicit rejection key and try
  // the decapsulation again. The result should be different.
  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&pos, sizeof(pos));
  EXPECT_EQ(SECSuccess, rv);

  rv = RNG_GenerateGlobalRandomBytes((uint8_t*)&byte, sizeof(byte));
  EXPECT_EQ(SECSuccess, rv);

  pos = (KYBER768_PRIVATE_KEY_BYTES - KYBER_SHARED_SECRET_BYTES) +
        (pos % KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(privateKey->len, KYBER768_PRIVATE_KEY_BYTES);
  privateKey->data[pos] ^= (byte | 1);

  rv = Kyber_Decapsulate(params_kyber768_round3, privateKey.get(),
                         ciphertext.get(), secret3.get());
  EXPECT_EQ(SECSuccess, rv);

  EXPECT_EQ(secret2->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_EQ(secret3->len, KYBER_SHARED_SECRET_BYTES);
  EXPECT_NE(0, memcmp(secret2->data, secret3->data, KYBER_SHARED_SECRET_BYTES));
}

TEST(Kyber768Test, KnownAnswersTest) {
  ScopedSECItem privateKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PRIVATE_KEY_BYTES));
  ScopedSECItem publicKey(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_PUBLIC_KEY_BYTES));
  ScopedSECItem ciphertext(
      SECITEM_AllocItem(nullptr, nullptr, KYBER768_CIPHERTEXT_BYTES));
  ScopedSECItem secret(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));
  ScopedSECItem secret2(
      SECITEM_AllocItem(nullptr, nullptr, KYBER_SHARED_SECRET_BYTES));

  SECStatus rv;
  uint8_t digest[SHA256_LENGTH];

  for (const auto& kat : KyberKATs) {
    SECItem keypair_seed = {siBuffer, (unsigned char*)kat.newKeySeed,
                            sizeof kat.newKeySeed};
    SECItem enc_seed = {siBuffer, (unsigned char*)kat.encapsSeed,
                        sizeof kat.encapsSeed};

    rv = Kyber_NewKey(kat.params, &keypair_seed, privateKey.get(),
                      publicKey.get());
    EXPECT_EQ(SECSuccess, rv);

    SHA256_HashBuf(digest, privateKey->data, privateKey->len);
    EXPECT_EQ(0, memcmp(kat.privateKeyDigest, digest, sizeof digest));

    SHA256_HashBuf(digest, publicKey->data, publicKey->len);
    EXPECT_EQ(0, memcmp(kat.publicKeyDigest, digest, sizeof digest));

    rv = Kyber_Encapsulate(kat.params, &enc_seed, publicKey.get(),
                           ciphertext.get(), secret.get());
    EXPECT_EQ(SECSuccess, rv);

    SHA256_HashBuf(digest, ciphertext->data, ciphertext->len);
    EXPECT_EQ(0, memcmp(kat.ciphertextDigest, digest, sizeof digest));

    EXPECT_EQ(secret->len, KYBER_SHARED_SECRET_BYTES);
    EXPECT_EQ(0, memcmp(kat.secret, secret->data, secret->len));

    rv = Kyber_Decapsulate(kat.params, privateKey.get(), ciphertext.get(),
                           secret2.get());
    EXPECT_EQ(SECSuccess, rv);
    EXPECT_EQ(secret2->len, KYBER_SHARED_SECRET_BYTES);
    EXPECT_EQ(0, memcmp(secret->data, secret2->data, secret2->len));
  }
}

}  // namespace nss_test
