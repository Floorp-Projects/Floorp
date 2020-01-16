/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"
#include "secerr.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"

#include "testvectors/chachapoly-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

static const CK_MECHANISM_TYPE kMech = CKM_NSS_CHACHA20_POLY1305;
static const CK_MECHANISM_TYPE kMechXor = CKM_NSS_CHACHA20_CTR;
// Some test data for simple tests.
static const uint8_t kKeyData[32] = {'k'};
static const uint8_t kCtrNonce[16] = {'c', 0, 0, 0, 'n'};
static const uint8_t kData[16] = {'d'};

class Pkcs11ChaCha20Poly1305Test
    : public ::testing::TestWithParam<chaChaTestVector> {
 public:
  void EncryptDecrypt(const ScopedPK11SymKey& key, const bool invalid_iv,
                      const bool invalid_tag, const uint8_t* data,
                      size_t data_len, const uint8_t* aad, size_t aad_len,
                      const uint8_t* iv, size_t iv_len,
                      const uint8_t* ct = nullptr, size_t ct_len = 0) {
    // Prepare AEAD params.
    CK_NSS_AEAD_PARAMS aead_params;
    aead_params.pNonce = toUcharPtr(iv);
    aead_params.ulNonceLen = iv_len;
    aead_params.pAAD = toUcharPtr(aad);
    aead_params.ulAADLen = aad_len;
    aead_params.ulTagLen = 16;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&aead_params),
                      sizeof(aead_params)};

    // Encrypt with bad parameters.
    unsigned int encrypted_len = 0;
    std::vector<uint8_t> encrypted(data_len + aead_params.ulTagLen);
    aead_params.ulTagLen = 158072;
    SECStatus rv =
        PK11_Encrypt(key.get(), kMech, &params, encrypted.data(),
                     &encrypted_len, encrypted.size(), data, data_len);
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, encrypted_len);
    aead_params.ulTagLen = 16;

    // Encrypt.
    rv = PK11_Encrypt(key.get(), kMech, &params, encrypted.data(),
                      &encrypted_len, encrypted.size(), data, data_len);

    // Return if encryption failure was expected due to invalid IV.
    // Without valid ciphertext, all further tests can be skipped.
    if (invalid_iv) {
      EXPECT_EQ(rv, SECFailure);
      EXPECT_EQ(0U, encrypted_len)
          << "encrypted_len is unmodified after failure";
      return;
    }

    EXPECT_EQ(rv, SECSuccess);
    EXPECT_EQ(encrypted.size(), static_cast<size_t>(encrypted_len));

    // Check ciphertext and tag.
    if (ct) {
      ASSERT_EQ(ct_len, encrypted_len);
      EXPECT_TRUE(!memcmp(ct, encrypted.data(), encrypted.size() - 16));
      EXPECT_TRUE(!memcmp(ct, encrypted.data(), encrypted.size()) !=
                  invalid_tag);
    }

    // Get the *estimated* plaintext length. This value should
    // never be zero as it could lead to a NULL outPtr being
    // passed to a subsequent decryption call (for AEAD we
    // must authenticate even when the pt is zero-length).
    unsigned int decrypt_bytes_needed = 0;
    rv = PK11_Decrypt(key.get(), kMech, &params, nullptr, &decrypt_bytes_needed,
                      0, encrypted.data(), encrypted_len);
    EXPECT_EQ(rv, SECSuccess);
    EXPECT_GT(decrypt_bytes_needed, data_len);

    // Now decrypt it
    std::vector<uint8_t> decrypted(decrypt_bytes_needed);
    unsigned int decrypted_len = 0;
    rv = PK11_Decrypt(key.get(), kMech, &params, decrypted.data(),
                      &decrypted_len, decrypted.size(), encrypted.data(),
                      encrypted.size());
    EXPECT_EQ(rv, SECSuccess);

    // Check the plaintext.
    ASSERT_EQ(data_len, decrypted_len);
    EXPECT_TRUE(!memcmp(data, decrypted.data(), decrypted_len));

    // Decrypt with bogus data.
    // Skip if there's no data to modify.
    if (encrypted_len > 0) {
      decrypted_len = 0;
      std::vector<uint8_t> bogus_ciphertext(encrypted);
      bogus_ciphertext[0] ^= 0xff;
      rv = PK11_Decrypt(key.get(), kMech, &params, decrypted.data(),
                        &decrypted_len, decrypted.size(),
                        bogus_ciphertext.data(), encrypted_len);
      EXPECT_EQ(rv, SECFailure);
      EXPECT_EQ(0U, decrypted_len);
    }

    // Decrypt with bogus tag.
    // Skip if there's no tag to modify.
    if (encrypted_len > 0) {
      decrypted_len = 0;
      std::vector<uint8_t> bogus_tag(encrypted);
      bogus_tag[encrypted_len - 1] ^= 0xff;
      rv = PK11_Decrypt(key.get(), kMech, &params, decrypted.data(),
                        &decrypted_len, decrypted.size(), bogus_tag.data(),
                        encrypted_len);
      EXPECT_EQ(rv, SECFailure);
      EXPECT_EQ(0U, decrypted_len);
    }

    // Decrypt with bogus IV.
    // iv_len == 0 is invalid and should be caught earlier.
    // Still skip, if there's no IV to modify.
    if (iv_len != 0) {
      decrypted_len = 0;
      SECItem bogus_params(params);
      CK_NSS_AEAD_PARAMS bogusAeadParams(aead_params);
      bogus_params.data = reinterpret_cast<unsigned char*>(&bogusAeadParams);

      std::vector<uint8_t> bogusIV(iv, iv + iv_len);
      bogusAeadParams.pNonce = toUcharPtr(bogusIV.data());
      bogusIV[0] ^= 0xff;

      rv = PK11_Decrypt(key.get(), kMech, &bogus_params, decrypted.data(),
                        &decrypted_len, data_len, encrypted.data(),
                        encrypted.size());
      EXPECT_EQ(rv, SECFailure);
      EXPECT_EQ(0U, decrypted_len);
    }

    // Decrypt with bogus additional data.
    // Skip when AAD was empty and can't be modified.
    // Alternatively we could generate random aad.
    if (aad_len != 0) {
      decrypted_len = 0;
      SECItem bogus_params(params);
      CK_NSS_AEAD_PARAMS bogus_aead_params(aead_params);
      bogus_params.data = reinterpret_cast<unsigned char*>(&bogus_aead_params);

      std::vector<uint8_t> bogus_aad(aad, aad + aad_len);
      bogus_aead_params.pAAD = toUcharPtr(bogus_aad.data());
      bogus_aad[0] ^= 0xff;

      rv = PK11_Decrypt(key.get(), kMech, &bogus_params, decrypted.data(),
                        &decrypted_len, data_len, encrypted.data(),
                        encrypted.size());
      EXPECT_EQ(rv, SECFailure);
      EXPECT_EQ(0U, decrypted_len);
    }
  }

  void EncryptDecrypt(const chaChaTestVector testvector) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem keyItem = {siBuffer, toUcharPtr(testvector.Key.data()),
                       static_cast<unsigned int>(testvector.Key.size())};

    // Import key.
    ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), kMech, PK11_OriginUnwrap,
                                           CKA_ENCRYPT, &keyItem, nullptr));
    EXPECT_TRUE(!!key);

    // Check.
    EncryptDecrypt(key, testvector.invalidIV, testvector.invalidTag,
                   testvector.Data.data(), testvector.Data.size(),
                   testvector.AAD.data(), testvector.AAD.size(),
                   testvector.IV.data(), testvector.IV.size(),
                   testvector.CT.data(), testvector.CT.size());
  }

 protected:
};

TEST_F(Pkcs11ChaCha20Poly1305Test, GenerateEncryptDecrypt) {
  // Generate a random key.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(PK11_KeyGen(slot.get(), kMech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!key);

  // Generate random data.
  std::vector<uint8_t> input(512);
  SECStatus rv =
      PK11_GenerateRandomOnSlot(slot.get(), input.data(), input.size());
  EXPECT_EQ(rv, SECSuccess);

  // Generate random AAD.
  std::vector<uint8_t> aad(16);
  rv = PK11_GenerateRandomOnSlot(slot.get(), aad.data(), aad.size());
  EXPECT_EQ(rv, SECSuccess);

  // Generate random IV.
  std::vector<uint8_t> iv(12);
  rv = PK11_GenerateRandomOnSlot(slot.get(), iv.data(), iv.size());
  EXPECT_EQ(rv, SECSuccess);

  // Check.
  EncryptDecrypt(key, false, false, input.data(), input.size(), aad.data(),
                 aad.size(), iv.data(), iv.size());
}

TEST_F(Pkcs11ChaCha20Poly1305Test, Xor) {
  static const uint8_t kExpected[sizeof(kData)] = {
      0xd8, 0x15, 0xd3, 0xb3, 0xe9, 0x34, 0x3b, 0x7a,
      0x24, 0xf6, 0x5f, 0xd7, 0x95, 0x3d, 0xd3, 0x51};

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  SECItem keyItem = {siBuffer, toUcharPtr(kKeyData),
                     static_cast<unsigned int>(sizeof(kKeyData))};
  ScopedPK11SymKey key(PK11_ImportSymKey(
      slot.get(), kMechXor, PK11_OriginUnwrap, CKA_ENCRYPT, &keyItem, nullptr));
  EXPECT_TRUE(!!key);

  SECItem ctrNonceItem = {siBuffer, toUcharPtr(kCtrNonce),
                          static_cast<unsigned int>(sizeof(kCtrNonce))};
  uint8_t encrypted[sizeof(kData)];
  unsigned int encrypted_len = 88;  // This should be overwritten.
  SECStatus rv =
      PK11_Encrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                   &encrypted_len, sizeof(encrypted), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kExpected), static_cast<size_t>(encrypted_len));
  EXPECT_EQ(0, memcmp(kExpected, encrypted, sizeof(kExpected)));

  // Decrypting has the same effect.
  rv = PK11_Decrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                    &encrypted_len, sizeof(encrypted), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kData), static_cast<size_t>(encrypted_len));
  EXPECT_EQ(0, memcmp(kExpected, encrypted, sizeof(kExpected)));

  // Operating in reverse too.
  rv = PK11_Encrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                    &encrypted_len, sizeof(encrypted), kExpected,
                    sizeof(kExpected));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kExpected), static_cast<size_t>(encrypted_len));
  EXPECT_EQ(0, memcmp(kData, encrypted, sizeof(kData)));
}

// This test just ensures that a key can be generated for use with the XOR
// function.  The result is random and therefore cannot be checked.
TEST_F(Pkcs11ChaCha20Poly1305Test, GenerateXor) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(PK11_KeyGen(slot.get(), kMech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!key);

  std::vector<uint8_t> iv(16);
  SECStatus rv = PK11_GenerateRandomOnSlot(slot.get(), iv.data(), iv.size());
  EXPECT_EQ(SECSuccess, rv);

  SECItem ctrNonceItem = {siBuffer, toUcharPtr(iv.data()),
                          static_cast<unsigned int>(iv.size())};
  uint8_t encrypted[sizeof(kData)];
  unsigned int encrypted_len = 88;  // This should be overwritten.
  rv = PK11_Encrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                    &encrypted_len, sizeof(encrypted), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kData), static_cast<size_t>(encrypted_len));
}

TEST_F(Pkcs11ChaCha20Poly1305Test, XorInvalidParams) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(PK11_KeyGen(slot.get(), kMech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!key);

  SECItem ctrNonceItem = {siBuffer, toUcharPtr(kCtrNonce),
                          static_cast<unsigned int>(sizeof(kCtrNonce)) - 1};
  uint8_t encrypted[sizeof(kData)];
  unsigned int encrypted_len = 88;
  SECStatus rv =
      PK11_Encrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                   &encrypted_len, sizeof(encrypted), kData, sizeof(kData));
  EXPECT_EQ(SECFailure, rv);

  ctrNonceItem.data = nullptr;
  rv = PK11_Encrypt(key.get(), kMechXor, &ctrNonceItem, encrypted,
                    &encrypted_len, sizeof(encrypted), kData, sizeof(kData));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
}

TEST_P(Pkcs11ChaCha20Poly1305Test, TestVectors) { EncryptDecrypt(GetParam()); }

INSTANTIATE_TEST_CASE_P(NSSTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20Vectors));

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20WycheproofVectors));

}  // namespace nss_test
