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
    : public ::testing::TestWithParam<chacha_testvector> {
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

    // Encrypt.
    unsigned int outputLen = 0;
    std::vector<uint8_t> output(data_len + aead_params.ulTagLen);
    SECStatus rv = PK11_Encrypt(key.get(), kMech, &params, output.data(),
                                &outputLen, output.size(), data, data_len);

    // Return if encryption failure was expected due to invalid IV.
    // Without valid ciphertext, all further tests can be skipped.
    if (invalid_iv) {
      EXPECT_EQ(rv, SECFailure);
      return;
    } else {
      EXPECT_EQ(rv, SECSuccess);
    }

    // Check ciphertext and tag.
    if (ct) {
      ASSERT_EQ(ct_len, outputLen);
      EXPECT_TRUE(!memcmp(ct, output.data(), outputLen) != invalid_tag);
    }

    // Decrypt.
    unsigned int decryptedLen = 0;
    std::vector<uint8_t> decrypted(data_len);
    rv =
        PK11_Decrypt(key.get(), kMech, &params, decrypted.data(), &decryptedLen,
                     decrypted.size(), output.data(), outputLen);
    EXPECT_EQ(rv, SECSuccess);

    // Check the plaintext.
    ASSERT_EQ(data_len, decryptedLen);
    EXPECT_TRUE(!memcmp(data, decrypted.data(), decryptedLen));

    // Decrypt with bogus data.
    // Skip if there's no data to modify.
    if (outputLen != 0) {
      std::vector<uint8_t> bogusCiphertext(output);
      bogusCiphertext[0] ^= 0xff;
      rv = PK11_Decrypt(key.get(), kMech, &params, decrypted.data(),
                        &decryptedLen, decrypted.size(), bogusCiphertext.data(),
                        outputLen);
      EXPECT_NE(rv, SECSuccess);
    }

    // Decrypt with bogus tag.
    // Skip if there's no tag to modify.
    if (outputLen != 0) {
      std::vector<uint8_t> bogusTag(output);
      bogusTag[outputLen - 1] ^= 0xff;
      rv = PK11_Decrypt(key.get(), kMech, &params, decrypted.data(),
                        &decryptedLen, decrypted.size(), bogusTag.data(),
                        outputLen);
      EXPECT_NE(rv, SECSuccess);
    }

    // Decrypt with bogus IV.
    // iv_len == 0 is invalid and should be caught earlier.
    // Still skip, if there's no IV to modify.
    if (iv_len != 0) {
      SECItem bogusParams(params);
      CK_NSS_AEAD_PARAMS bogusAeadParams(aead_params);
      bogusParams.data = reinterpret_cast<unsigned char*>(&bogusAeadParams);

      std::vector<uint8_t> bogusIV(iv, iv + iv_len);
      bogusAeadParams.pNonce = toUcharPtr(bogusIV.data());
      bogusIV[0] ^= 0xff;

      rv = PK11_Decrypt(key.get(), kMech, &bogusParams, decrypted.data(),
                        &decryptedLen, data_len, output.data(), outputLen);
      EXPECT_NE(rv, SECSuccess);
    }

    // Decrypt with bogus additional data.
    // Skip when AAD was empty and can't be modified.
    // Alternatively we could generate random aad.
    if (aad_len != 0) {
      SECItem bogusParams(params);
      CK_NSS_AEAD_PARAMS bogusAeadParams(aead_params);
      bogusParams.data = reinterpret_cast<unsigned char*>(&bogusAeadParams);

      std::vector<uint8_t> bogusAAD(aad, aad + aad_len);
      bogusAeadParams.pAAD = toUcharPtr(bogusAAD.data());
      bogusAAD[0] ^= 0xff;

      rv = PK11_Decrypt(key.get(), kMech, &bogusParams, decrypted.data(),
                        &decryptedLen, data_len, output.data(), outputLen);
      EXPECT_NE(rv, SECSuccess);
    }
  }

  void EncryptDecrypt(const chacha_testvector testvector) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem key_item = {siBuffer, toUcharPtr(testvector.Key.data()),
                        static_cast<unsigned int>(testvector.Key.size())};

    // Import key.
    ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), kMech, PK11_OriginUnwrap,
                                           CKA_ENCRYPT, &key_item, nullptr));
    EXPECT_TRUE(!!key);

    // Check.
    EncryptDecrypt(key, testvector.invalid_iv, testvector.invalid_tag,
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
  std::vector<uint8_t> data(512);
  SECStatus rv =
      PK11_GenerateRandomOnSlot(slot.get(), data.data(), data.size());
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
  EncryptDecrypt(key, false, false, data.data(), data.size(), aad.data(),
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

  SECItem ctr_nonce_item = {siBuffer, toUcharPtr(kCtrNonce),
                            static_cast<unsigned int>(sizeof(kCtrNonce))};
  uint8_t output[sizeof(kData)];
  unsigned int output_len = 88;  // This should be overwritten.
  SECStatus rv =
      PK11_Encrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                   sizeof(output), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kExpected), static_cast<size_t>(output_len));
  EXPECT_EQ(0, memcmp(kExpected, output, sizeof(kExpected)));

  // Decrypting has the same effect.
  rv = PK11_Decrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                    sizeof(output), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kData), static_cast<size_t>(output_len));
  EXPECT_EQ(0, memcmp(kExpected, output, sizeof(kExpected)));

  // Operating in reverse too.
  rv = PK11_Encrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                    sizeof(output), kExpected, sizeof(kExpected));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kExpected), static_cast<size_t>(output_len));
  EXPECT_EQ(0, memcmp(kData, output, sizeof(kData)));
}

// This test just ensures that a key can be generated for use with the XOR
// function.  The result is random and therefore cannot be checked.
TEST_F(Pkcs11ChaCha20Poly1305Test, GenerateXor) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(PK11_KeyGen(slot.get(), kMech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!key);

  SECItem ctr_nonce_item = {siBuffer, toUcharPtr(kCtrNonce),
                            static_cast<unsigned int>(sizeof(kCtrNonce))};
  uint8_t output[sizeof(kData)];
  unsigned int output_len = 88;  // This should be overwritten.
  SECStatus rv =
      PK11_Encrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                   sizeof(output), kData, sizeof(kData));
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(sizeof(kData), static_cast<size_t>(output_len));
}

TEST_F(Pkcs11ChaCha20Poly1305Test, XorInvalidParams) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(PK11_KeyGen(slot.get(), kMech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!key);

  SECItem ctr_nonce_item = {siBuffer, toUcharPtr(kCtrNonce),
                            static_cast<unsigned int>(sizeof(kCtrNonce)) - 1};
  uint8_t output[sizeof(kData)];
  unsigned int output_len = 88;
  SECStatus rv =
      PK11_Encrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                   sizeof(output), kData, sizeof(kData));
  EXPECT_EQ(SECFailure, rv);

  ctr_nonce_item.data = nullptr;
  rv = PK11_Encrypt(key.get(), kMechXor, &ctr_nonce_item, output, &output_len,
                    sizeof(output), kData, sizeof(kData));
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
}

TEST_P(Pkcs11ChaCha20Poly1305Test, TestVectors) { EncryptDecrypt(GetParam()); }

INSTANTIATE_TEST_CASE_P(NSSTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20Vectors));

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20WycheproofVectors));

}  // namespace nss_test
