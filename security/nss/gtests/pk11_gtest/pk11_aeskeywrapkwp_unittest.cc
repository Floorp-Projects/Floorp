/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"

#include "testvectors/kw-vectors.h"
#include "testvectors/kwp-vectors.h"
#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

class Pkcs11AESKeyWrapKwpTest
    : public ::testing::TestWithParam<keywrap_vector> {
 protected:
  CK_MECHANISM_TYPE mechanism = CKM_AES_KEY_WRAP_KWP;

  void WrapUnwrap(unsigned char* kek_data, unsigned int kek_len,
                  unsigned char* key_data, unsigned int key_data_len,
                  unsigned char* expected_ciphertext,
                  unsigned int expected_ciphertext_len,
                  std::map<Action, Result> tests, uint32_t test_id) {
    std::vector<unsigned char> wrapped_key(PR_MAX(1U, expected_ciphertext_len));
    std::vector<unsigned char> unwrapped_key(PR_MAX(1U, key_data_len));
    std::vector<unsigned char> zeros(PR_MAX(1U, expected_ciphertext_len), 0);
    unsigned int wrapped_key_len = 0;
    unsigned int unwrapped_key_len = 0;
    SECStatus rv;

    std::stringstream s;
    s << "Test with original ID #" << test_id << " failed." << std::endl;
    std::string msg = s.str();

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ASSERT_NE(nullptr, slot) << msg;

    // Import encryption key.
    SECItem kek_item = {siBuffer, kek_data, kek_len};
    ScopedPK11SymKey kek(PK11_ImportSymKeyWithFlags(
        slot.get(), mechanism, PK11_OriginUnwrap, CKA_ENCRYPT, &kek_item,
        CKF_DECRYPT, PR_FALSE, nullptr));
    EXPECT_TRUE(!!kek) << msg;

    // Wrap key
    Action test = WRAP;
    if (tests.count(test)) {
      rv = PK11_Encrypt(kek.get(), mechanism, nullptr /* param */,
                        wrapped_key.data(), &wrapped_key_len,
                        wrapped_key.size(), key_data, key_data_len);
      ASSERT_EQ(rv, tests[test].expect_rv) << msg;

      // If we failed, check that output was not produced.
      if (rv == SECFailure) {
        EXPECT_TRUE(wrapped_key_len == 0);
        EXPECT_TRUE(!memcmp(wrapped_key.data(), zeros.data(), wrapped_key_len));
      }

      if (tests[test].output_match) {
        EXPECT_EQ(expected_ciphertext_len, wrapped_key_len) << msg;
        EXPECT_TRUE(!memcmp(expected_ciphertext, wrapped_key.data(),
                            expected_ciphertext_len))
            << msg;
      } else {
        // If we produced output, verify that it doesn't match the vector
        if (wrapped_key_len) {
          EXPECT_FALSE(wrapped_key_len == expected_ciphertext_len &&
                       !memcmp(wrapped_key.data(), expected_ciphertext,
                               expected_ciphertext_len))
              << msg;
        }
      }
    }

    // Unwrap key
    test = UNWRAP;
    if (tests.count(test)) {
      rv = PK11_Decrypt(kek.get(), mechanism, nullptr /* param */,
                        unwrapped_key.data(), &unwrapped_key_len,
                        unwrapped_key.size(), expected_ciphertext,
                        expected_ciphertext_len);
      ASSERT_EQ(rv, tests[test].expect_rv) << msg;

      // If we failed, check that output was not produced.
      if (rv == SECFailure) {
        EXPECT_TRUE(unwrapped_key_len == 0);
        EXPECT_TRUE(
            !memcmp(unwrapped_key.data(), zeros.data(), unwrapped_key_len));
      }

      if (tests[test].output_match) {
        EXPECT_EQ(unwrapped_key_len, key_data_len) << msg;
        EXPECT_TRUE(!memcmp(key_data, unwrapped_key.data(), key_data_len))
            << msg;
      } else {
        // If we produced output, verify that it doesn't match the vector
        if (unwrapped_key_len) {
          EXPECT_FALSE(
              unwrapped_key_len == expected_ciphertext_len &&
              !memcmp(unwrapped_key.data(), key_data, unwrapped_key_len))
              << msg;
        }
      }
    }
  }

  void WrapUnwrap(keywrap_vector testvector) {
    WrapUnwrap(testvector.key.data(), testvector.key.size(),
               testvector.msg.data(), testvector.msg.size(),
               testvector.ct.data(), testvector.ct.size(), testvector.tests,
               testvector.test_id);
  }
};

TEST_P(Pkcs11AESKeyWrapKwpTest, TestVectors) { WrapUnwrap(GetParam()); }

INSTANTIATE_TEST_SUITE_P(Pkcs11NistAESKWPTest, Pkcs11AESKeyWrapKwpTest,
                         ::testing::ValuesIn(kNistAesKWPVectors));
} /* nss_test */
