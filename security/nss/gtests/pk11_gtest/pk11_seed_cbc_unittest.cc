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
class Pkcs11SeedCbcTest : public ::testing::Test {
 protected:
  enum class Action { Encrypt, Decrypt };

  SECStatus EncryptDecryptSeed(Action action, unsigned int input_size,
                               unsigned int output_size) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), kMech, nullptr, 16, nullptr));
    EXPECT_TRUE(!!sym_key);

    std::vector<uint8_t> data(input_size);
    std::vector<uint8_t> init_vector(16);
    std::vector<uint8_t> output(output_size);
    SECItem params = {siBuffer, init_vector.data(),
                      (unsigned int)init_vector.size()};

    // Try to encrypt/decrypt.
    unsigned int output_len = 0;
    if (action == Action::Encrypt) {
      return PK11_Encrypt(sym_key.get(), kMech, &params, output.data(),
                          &output_len, output_size, data.data(), data.size());
    } else {
      return PK11_Decrypt(sym_key.get(), kMech, &params, output.data(),
                          &output_len, output_size, data.data(), data.size());
    }
  }
  const CK_MECHANISM_TYPE kMech = CKM_SEED_CBC;
};

// The intention here is to test the arguments of these functions
// The resulted content is already tested in EncryptDeriveTests.
// SEED_CBC needs an IV of 16 bytes.
// The input data size must be multiple of 16.
// If not, some padding should be added.
// The output size must be at least the size of input data.
TEST_F(Pkcs11SeedCbcTest, SeedCBC_ValidArgs) {
  EXPECT_EQ(SECSuccess, EncryptDecryptSeed(Action::Encrypt, 16, 16));
  EXPECT_EQ(SECSuccess, EncryptDecryptSeed(Action::Decrypt, 16, 16));
  // No problem if maxLen is bigger than input data.
  EXPECT_EQ(SECSuccess, EncryptDecryptSeed(Action::Encrypt, 16, 32));
  EXPECT_EQ(SECSuccess, EncryptDecryptSeed(Action::Decrypt, 16, 32));
}

TEST_F(Pkcs11SeedCbcTest, SeedCBC_InvalidArgs) {
  // maxLen lower than input data.
  EXPECT_EQ(SECFailure, EncryptDecryptSeed(Action::Encrypt, 16, 10));
  EXPECT_EQ(SECFailure, EncryptDecryptSeed(Action::Decrypt, 16, 10));
  // input data not multiple of SEED_BLOCK_SIZE (16)
  EXPECT_EQ(SECFailure, EncryptDecryptSeed(Action::Encrypt, 17, 32));
  EXPECT_EQ(SECFailure, EncryptDecryptSeed(Action::Decrypt, 17, 32));
}

}  // namespace nss_test