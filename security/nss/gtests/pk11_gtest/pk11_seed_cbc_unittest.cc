/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"

#include "nss_scoped_ptrs.h"
#include "gtest/gtest.h"
#include "util.h"

namespace nss_test {
class Pkcs11SeedTest : public ::testing::Test {
 protected:
  void EncryptDecryptSeed(SECStatus expected, unsigned int input_size,
                          unsigned int output_size,
                          CK_MECHANISM_TYPE mech = CKM_SEED_CBC) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), mech, nullptr, 16, nullptr));
    EXPECT_TRUE(!!sym_key);

    std::vector<uint8_t> plaintext(input_size, 0xFF);
    std::vector<uint8_t> init_vector(16);
    std::vector<uint8_t> ciphertext(output_size, 0);
    SECItem iv_param = {siBuffer, init_vector.data(),
                        (unsigned int)init_vector.size()};
    std::vector<uint8_t> decrypted(output_size, 0);

    // Try to encrypt, decrypt if positive test.
    unsigned int output_len = 0;
    EXPECT_EQ(expected,
              PK11_Encrypt(sym_key.get(), mech, &iv_param, ciphertext.data(),
                           &output_len, output_size, plaintext.data(),
                           plaintext.size()));

    if (expected == SECSuccess) {
      EXPECT_EQ(expected,
                PK11_Decrypt(sym_key.get(), mech, &iv_param, decrypted.data(),
                             &output_len, output_size, ciphertext.data(),
                             output_len));
      decrypted.resize(output_len);
      EXPECT_EQ(plaintext, decrypted);
    }
  }
};

// The intention here is to test the arguments of these functions
// The resulted content is already tested in EncryptDeriveTests.
// SEED_CBC needs an IV of 16 bytes.
// The input data size must be multiple of 16.
// If not, some padding should be added.
// The output size must be at least the size of input data.
TEST_F(Pkcs11SeedTest, CBC_ValidArgs) {
  EncryptDecryptSeed(SECSuccess, 16, 16);
  // No problem if maxLen is bigger than input data.
  EncryptDecryptSeed(SECSuccess, 16, 32);
}

TEST_F(Pkcs11SeedTest, CBC_InvalidArgs) {
  // maxLen lower than input data.
  EncryptDecryptSeed(SECFailure, 16, 10);
  // input data not multiple of SEED_BLOCK_SIZE (16)
  EncryptDecryptSeed(SECFailure, 17, 32);
}

TEST_F(Pkcs11SeedTest, ECB_Singleblock) {
  EncryptDecryptSeed(SECSuccess, 16, 16, CKM_SEED_ECB);
}

TEST_F(Pkcs11SeedTest, ECB_Multiblock) {
  EncryptDecryptSeed(SECSuccess, 64, 64, CKM_SEED_ECB);
}

}  // namespace nss_test