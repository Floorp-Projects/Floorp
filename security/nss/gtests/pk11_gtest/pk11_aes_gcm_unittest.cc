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

class Pkcs11AesGcmTest : public ::testing::TestWithParam<gcm_kat_value> {
 protected:
  void RunTest(const gcm_kat_value val) {
    std::vector<uint8_t> key = hex_string_to_bytes(val.key);
    std::vector<uint8_t> iv = hex_string_to_bytes(val.iv);
    std::vector<uint8_t> plaintext = hex_string_to_bytes(val.plaintext);
    std::vector<uint8_t> aad = hex_string_to_bytes(val.additional_data);
    std::vector<uint8_t> result = hex_string_to_bytes(val.result);
    bool invalid_ct = val.invalid_ct;
    bool invalid_iv = val.invalid_iv;
    std::stringstream s;
    s << "Test #" << val.test_id << " failed.";
    std::string msg = s.str();
    // Ignore GHASH-only vectors.
    if (key.empty()) {
      return;
    }

    // Prepare AEAD params.
    CK_GCM_PARAMS gcmParams;
    gcmParams.pIv = iv.data();
    gcmParams.ulIvLen = iv.size();
    gcmParams.pAAD = aad.data();
    gcmParams.ulAADLen = aad.size();
    gcmParams.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcmParams),
                      sizeof(gcmParams)};

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem keyItem = {siBuffer, key.data(),
                       static_cast<unsigned int>(key.size())};

    // Import key.
    ScopedPK11SymKey symKey(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_ENCRYPT, &keyItem, nullptr));
    ASSERT_TRUE(!!symKey) << msg;

    // Encrypt with bogus parameters.
    unsigned int outputLen = 0;
    std::vector<uint8_t> output(plaintext.size() + gcmParams.ulTagBits / 8);
    gcmParams.ulTagBits = 159082344;
    SECStatus rv =
        PK11_Encrypt(symKey.get(), mech, &params, output.data(), &outputLen,
                     output.size(), plaintext.data(), plaintext.size());
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, outputLen);
    gcmParams.ulTagBits = 128;

    // Encrypt.
    rv = PK11_Encrypt(symKey.get(), mech, &params, output.data(), &outputLen,
                      output.size(), plaintext.data(), plaintext.size());
    if (invalid_iv) {
      EXPECT_EQ(rv, SECFailure) << msg;
      EXPECT_EQ(0U, outputLen);
      return;
    }
    EXPECT_EQ(rv, SECSuccess) << msg;

    ASSERT_EQ(outputLen, output.size()) << msg;

    // Check ciphertext and tag.
    if (invalid_ct) {
      EXPECT_NE(result, output) << msg;
    } else {
      EXPECT_EQ(result, output) << msg;
    }

    // Decrypt.
    unsigned int decryptedLen = 0;
    // The PK11 AES API is stupid, it expects an explicit IV and thus wants
    // a block more of available output memory.
    std::vector<uint8_t> decrypted(output.size());
    rv =
        PK11_Decrypt(symKey.get(), mech, &params, decrypted.data(),
                     &decryptedLen, decrypted.size(), output.data(), outputLen);
    EXPECT_EQ(rv, SECSuccess) << msg;
    ASSERT_EQ(decryptedLen, plaintext.size()) << msg;

    // Check the plaintext.
    EXPECT_EQ(plaintext, std::vector<uint8_t>(decrypted.begin(),
                                              decrypted.begin() + decryptedLen))
        << msg;
  }

  SECStatus EncryptWithIV(std::vector<uint8_t>& iv) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey symKey(
        PK11_KeyGen(slot.get(), mech, nullptr, 16, nullptr));
    EXPECT_TRUE(!!symKey);

    std::vector<uint8_t> data(17);
    std::vector<uint8_t> output(33);
    std::vector<uint8_t> aad(0);

    // Prepare AEAD params.
    CK_GCM_PARAMS gcmParams;
    gcmParams.pIv = iv.data();
    gcmParams.ulIvLen = iv.size();
    gcmParams.pAAD = aad.data();
    gcmParams.ulAADLen = aad.size();
    gcmParams.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcmParams),
                      sizeof(gcmParams)};

    // Try to encrypt.
    unsigned int outputLen = 0;
    return PK11_Encrypt(symKey.get(), mech, &params, output.data(), &outputLen,
                        output.size(), data.data(), data.size());
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
  EXPECT_EQ(EncryptWithIV(iv), SECFailure);
}

TEST_F(Pkcs11AesGcmTest, AllZeroIV) {
  std::vector<uint8_t> iv(16, 0);
  EXPECT_EQ(EncryptWithIV(iv), SECSuccess);
}

TEST_F(Pkcs11AesGcmTest, TwelveByteZeroIV) {
  std::vector<uint8_t> iv(12, 0);
  EXPECT_EQ(EncryptWithIV(iv), SECSuccess);
}

}  // namespace nss_test
