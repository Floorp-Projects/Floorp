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

#include "blapi.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"
#include "testvectors/cmac-vectors.h"
#include "util.h"

namespace nss_test {

class Pkcs11AesCmacTest : public ::testing::TestWithParam<AesCmacTestVector> {
 protected:
  ScopedPK11SymKey ImportKey(CK_MECHANISM_TYPE mech, SECItem *key_item) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Can't get slot";
      return nullptr;
    }

    ScopedPK11SymKey result(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_SIGN, key_item, nullptr));

    return result;
  }

  void RunTest(uint8_t *key, unsigned int key_len, uint8_t *data,
               unsigned int data_len, uint8_t *expected,
               unsigned int expected_len, CK_ULONG mechanism) {
    // Create SECItems for everything...
    std::vector<uint8_t> output(expected_len);
    SECItem key_item = {siBuffer, key, key_len};
    SECItem output_item = {siBuffer, output.data(), expected_len};
    SECItem data_item = {siBuffer, data, data_len};
    SECItem expected_item = {siBuffer, expected, expected_len};

    // Do the PKCS #11 stuff...
    ScopedPK11SymKey p11_key = ImportKey(mechanism, &key_item);
    ASSERT_NE(nullptr, p11_key.get());

    SECStatus ret = PK11_SignWithSymKey(p11_key.get(), CKM_AES_CMAC, NULL,
                                        &output_item, &data_item);

    // Verify the result...
    ASSERT_EQ(SECSuccess, ret);
    ASSERT_EQ(0, SECITEM_CompareItem(&output_item, &expected_item));
  }

  void RunTestVector(const AesCmacTestVector vec) {
    bool valid = !vec.invalid;
    std::string err = "Test #" + std::to_string(vec.id) + " failed";
    std::vector<uint8_t> key = hex_string_to_bytes(vec.key);
    std::vector<uint8_t> tag = hex_string_to_bytes(vec.tag);
    std::vector<uint8_t> msg = hex_string_to_bytes(vec.msg);

    std::vector<uint8_t> output(AES_BLOCK_SIZE);
    // Don't provide a null pointer, even if the input is empty.
    uint8_t tmp;
    SECItem key_item = {siBuffer, key.data() ? key.data() : &tmp,
                        static_cast<unsigned int>(key.size())};
    SECItem tag_item = {siBuffer, tag.data() ? tag.data() : &tmp,
                        static_cast<unsigned int>(tag.size())};
    SECItem msg_item = {siBuffer, msg.data() ? msg.data() : &tmp,
                        static_cast<unsigned int>(msg.size())};
    SECItem out_item = {siBuffer, output.data() ? output.data() : &tmp,
                        static_cast<unsigned int>(output.size())};

    ScopedPK11SymKey p11_key = ImportKey(CKM_AES_CMAC_GENERAL, &key_item);
    if (vec.comment == "invalid key size") {
      ASSERT_EQ(nullptr, p11_key.get()) << err;
      return;
    }

    ASSERT_NE(nullptr, p11_key.get()) << err;
    SECStatus rv = PK11_SignWithSymKey(p11_key.get(), CKM_AES_CMAC, NULL,
                                       &out_item, &msg_item);

    EXPECT_EQ(SECSuccess, rv) << err;
    EXPECT_EQ(valid, 0 == SECITEM_CompareItem(&out_item, &tag_item)) << err;
  }
};

TEST_P(Pkcs11AesCmacTest, TestVectors) { RunTestVector(GetParam()); }

INSTANTIATE_TEST_CASE_P(WycheproofTestVector, Pkcs11AesCmacTest,
                        ::testing::ValuesIn(kCmacWycheproofVectors));

// Sanity check of the PKCS #11 API only. Extensive tests for correctness of
// underling CMAC implementation conducted in the following file:
//      gtests/freebl_gtest/cmac_unittests.cc

TEST_F(Pkcs11AesCmacTest, Aes128NistExample1) {
  uint8_t key[AES_128_KEY_LENGTH] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE,
                                     0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88,
                                     0x09, 0xCF, 0x4F, 0x3C};
  uint8_t known[AES_BLOCK_SIZE] = {0xBB, 0x1D, 0x69, 0x29, 0xE9, 0x59,
                                   0x37, 0x28, 0x7F, 0xA3, 0x7D, 0x12,
                                   0x9B, 0x75, 0x67, 0x46};

  RunTest(key, AES_128_KEY_LENGTH, NULL, 0, known, AES_BLOCK_SIZE,
          CKM_AES_CMAC);
}

TEST_F(Pkcs11AesCmacTest, General) {
  uint8_t key[AES_128_KEY_LENGTH] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE,
                                     0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88,
                                     0x09, 0xCF, 0x4F, 0x3C};
  uint8_t known[4] = {0xBB, 0x1D, 0x69, 0x29};

  RunTest(key, AES_128_KEY_LENGTH, NULL, 0, known, 4, CKM_AES_CMAC_GENERAL);
}

TEST_F(Pkcs11AesCmacTest, InvalidKeySize) {
  uint8_t key[4] = {0x00, 0x00, 0x00, 0x00};
  SECItem key_item = {siBuffer, key, 4};

  ScopedPK11SymKey result = ImportKey(CKM_AES_CMAC, &key_item);
  ASSERT_EQ(nullptr, result.get());
}
}  // namespace nss_test
