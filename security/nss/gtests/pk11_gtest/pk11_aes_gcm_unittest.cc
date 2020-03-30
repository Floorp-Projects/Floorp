/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"

#include "nss_scoped_ptrs.h"

#include "testvectors/gcm-vectors.h"
#include "gtest/gtest.h"
#include "util.h"

namespace nss_test {

class Pkcs11AesGcmTest : public ::testing::TestWithParam<AesGcmKatValue> {
 protected:
  void RunTest(const AesGcmKatValue vec) {
    std::vector<uint8_t> key = hex_string_to_bytes(vec.key);
    std::vector<uint8_t> iv = hex_string_to_bytes(vec.iv);
    std::vector<uint8_t> plaintext = hex_string_to_bytes(vec.plaintext);
    std::vector<uint8_t> aad = hex_string_to_bytes(vec.additional_data);
    std::vector<uint8_t> result = hex_string_to_bytes(vec.result);
    bool invalid_ct = vec.invalid_ct;
    bool invalid_iv = vec.invalid_iv;
    std::string msg = "Test #" + std::to_string(vec.id) + " failed";
    // Ignore GHASH-only vectors.
    if (key.empty()) {
      return;
    }

    // Prepare AEAD params.
    CK_GCM_PARAMS gcm_params;
    gcm_params.pIv = iv.data();
    gcm_params.ulIvLen = iv.size();
    gcm_params.pAAD = aad.data();
    gcm_params.ulAADLen = aad.size();
    gcm_params.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                      sizeof(gcm_params)};

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem key_item = {siBuffer, key.data(),
                        static_cast<unsigned int>(key.size())};

    // Import key.
    ScopedPK11SymKey sym_key(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_ENCRYPT, &key_item, nullptr));
    ASSERT_TRUE(!!sym_key) << msg;

    // Encrypt with bogus parameters.
    unsigned int output_len = 0;
    std::vector<uint8_t> output(plaintext.size() + gcm_params.ulTagBits / 8);
    // "maxout" must be at least "inlen + tagBytes", or, in this case:
    // "output.size()" must be at least "plaintext.size() + tagBytes"
    gcm_params.ulTagBits = 128;
    SECStatus rv =
        PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                     output.size() - 10, plaintext.data(), plaintext.size());
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, output_len);

    // The valid values for tag size in AES_GCM are:
    // 32, 64, 96, 104, 112, 120 and 128.
    gcm_params.ulTagBits = 110;
    rv = PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                      output.size(), plaintext.data(), plaintext.size());
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, output_len);

    // Encrypt.
    gcm_params.ulTagBits = 128;
    rv = PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                      output.size(), plaintext.data(), plaintext.size());
    if (invalid_iv) {
      EXPECT_EQ(SECFailure, rv) << msg;
      EXPECT_EQ(0U, output_len);
      return;
    }
    EXPECT_EQ(SECSuccess, rv) << msg;

    ASSERT_EQ(output_len, output.size()) << msg;

    // Check ciphertext and tag.
    if (invalid_ct) {
      EXPECT_NE(result, output) << msg;
    } else {
      EXPECT_EQ(result, output) << msg;
    }

    // Decrypt.
    unsigned int decrypted_len = 0;
    // The PK11 AES API is stupid, it expects an explicit IV and thus wants
    // a block more of available output memory.
    std::vector<uint8_t> decrypted(output.size());
    rv = PK11_Decrypt(sym_key.get(), mech, &params, decrypted.data(),
                      &decrypted_len, decrypted.size(), output.data(),
                      output_len);
    EXPECT_EQ(SECSuccess, rv) << msg;
    ASSERT_EQ(decrypted_len, plaintext.size()) << msg;

    // Check the plaintext.
    EXPECT_EQ(plaintext,
              std::vector<uint8_t>(decrypted.begin(),
                                   decrypted.begin() + decrypted_len))
        << msg;
  }

  SECStatus EncryptWithIV(std::vector<uint8_t>& iv) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), mech, nullptr, 16, nullptr));
    EXPECT_TRUE(!!sym_key);

    std::vector<uint8_t> data(17);
    std::vector<uint8_t> output(33);
    std::vector<uint8_t> aad(0);

    // Prepare AEAD params.
    CK_GCM_PARAMS gcm_params;
    gcm_params.pIv = iv.data();
    gcm_params.ulIvLen = iv.size();
    gcm_params.pAAD = aad.data();
    gcm_params.ulAADLen = aad.size();
    gcm_params.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                      sizeof(gcm_params)};

    // Try to encrypt.
    unsigned int output_len = 0;
    return PK11_Encrypt(sym_key.get(), mech, &params, output.data(),
                        &output_len, output.size(), data.data(), data.size());
  }

  const CK_MECHANISM_TYPE mech = CKM_AES_GCM;
};

TEST_P(Pkcs11AesGcmTest, TestVectors) { RunTest(GetParam()); }

INSTANTIATE_TEST_CASE_P(NISTTestVector, Pkcs11AesGcmTest,
                        ::testing::ValuesIn(kGcmKatValues));

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11AesGcmTest,
                        ::testing::ValuesIn(kGcmWycheproofVectors));

TEST_F(Pkcs11AesGcmTest, ZeroLengthIV) {
  std::vector<uint8_t> iv(0);
  EXPECT_EQ(SECFailure, EncryptWithIV(iv));
}

TEST_F(Pkcs11AesGcmTest, AllZeroIV) {
  std::vector<uint8_t> iv(16, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv));
}

TEST_F(Pkcs11AesGcmTest, TwelveByteZeroIV) {
  std::vector<uint8_t> iv(12, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv));
}

}  // namespace nss_test
