/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "nss_scoped_ptrs.h"

#include "gtest/gtest.h"

namespace nss_test {

class Pkcs11DesTest : public ::testing::Test {
 protected:
  SECStatus EncryptWithIV(std::vector<uint8_t>& iv,
                          const CK_MECHANISM_TYPE mech) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), mech, nullptr, 8, nullptr));
    EXPECT_TRUE(!!sym_key);

    std::vector<uint8_t> data(16);
    std::vector<uint8_t> output(16);

    SECItem params = {siBuffer, iv.data(),
                      static_cast<unsigned int>(iv.size())};

    // Try to encrypt.
    unsigned int output_len = 0;
    return PK11_Encrypt(sym_key.get(), mech, &params, output.data(),
                        &output_len, output.size(), data.data(), data.size());
  }
};

TEST_F(Pkcs11DesTest, ZeroLengthIV) {
  std::vector<uint8_t> iv(0);
  EXPECT_EQ(SECFailure, EncryptWithIV(iv, CKM_DES_CBC));
  EXPECT_EQ(SECFailure, EncryptWithIV(iv, CKM_DES3_CBC));
}

TEST_F(Pkcs11DesTest, IVTooShort) {
  std::vector<uint8_t> iv(7);
  EXPECT_EQ(SECFailure, EncryptWithIV(iv, CKM_DES_CBC));
  EXPECT_EQ(SECFailure, EncryptWithIV(iv, CKM_DES3_CBC));
}

TEST_F(Pkcs11DesTest, WrongLengthIV) {
  // We tolerate IVs > 8
  std::vector<uint8_t> iv(15, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv, CKM_DES_CBC));
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv, CKM_DES3_CBC));
}

TEST_F(Pkcs11DesTest, AllGood) {
  std::vector<uint8_t> iv(8, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv, CKM_DES_CBC));
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv, CKM_DES3_CBC));
}

}  // namespace nss_test
