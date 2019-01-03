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
#include "nss_scoped_ptrs.h"

#include "testvectors/chachapoly-vectors.h"
#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11ChaCha20Poly1305Test
    : public ::testing::TestWithParam<chacha_testvector> {
 public:
  void EncryptDecrypt(PK11SymKey* symKey, const bool invalid_iv,
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
    SECStatus rv = PK11_Encrypt(symKey, mech, &params, output.data(),
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
    rv = PK11_Decrypt(symKey, mech, &params, decrypted.data(), &decryptedLen,
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
      rv = PK11_Decrypt(symKey, mech, &params, decrypted.data(), &decryptedLen,
                        decrypted.size(), bogusCiphertext.data(), outputLen);
      EXPECT_NE(rv, SECSuccess);
    }

    // Decrypt with bogus tag.
    // Skip if there's no tag to modify.
    if (outputLen != 0) {
      std::vector<uint8_t> bogusTag(output);
      bogusTag[outputLen - 1] ^= 0xff;
      rv = PK11_Decrypt(symKey, mech, &params, decrypted.data(), &decryptedLen,
                        decrypted.size(), bogusTag.data(), outputLen);
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

      rv = PK11_Decrypt(symKey, mech, &bogusParams, decrypted.data(),
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

      rv = PK11_Decrypt(symKey, mech, &bogusParams, decrypted.data(),
                        &decryptedLen, data_len, output.data(), outputLen);
      EXPECT_NE(rv, SECSuccess);
    }
  }

  void EncryptDecrypt(const chacha_testvector testvector) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem keyItem = {siBuffer, toUcharPtr(testvector.Key.data()),
                       static_cast<unsigned int>(testvector.Key.size())};

    // Import key.
    ScopedPK11SymKey symKey(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_ENCRYPT, &keyItem, nullptr));
    EXPECT_TRUE(!!symKey);

    // Check.
    EncryptDecrypt(symKey.get(), testvector.invalid_iv, testvector.invalid_tag,
                   testvector.Data.data(), testvector.Data.size(),
                   testvector.AAD.data(), testvector.AAD.size(),
                   testvector.IV.data(), testvector.IV.size(),
                   testvector.CT.data(), testvector.CT.size());
  }

 protected:
  CK_MECHANISM_TYPE mech = CKM_NSS_CHACHA20_POLY1305;
};

TEST_F(Pkcs11ChaCha20Poly1305Test, GenerateEncryptDecrypt) {
  // Generate a random key.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey symKey(PK11_KeyGen(slot.get(), mech, nullptr, 32, nullptr));
  EXPECT_TRUE(!!symKey);

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
  EncryptDecrypt(symKey.get(), false, false, data.data(), data.size(),
                 aad.data(), aad.size(), iv.data(), iv.size());
}

TEST_P(Pkcs11ChaCha20Poly1305Test, TestVectors) { EncryptDecrypt(GetParam()); }

INSTANTIATE_TEST_CASE_P(NSSTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20Vectors));

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11ChaCha20Poly1305Test,
                        ::testing::ValuesIn(kChaCha20WycheproofVectors));

}  // namespace nss_test
