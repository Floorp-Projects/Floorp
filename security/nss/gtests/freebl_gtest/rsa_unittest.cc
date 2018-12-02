// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include <stdint.h>

#include "blapi.h"
#include "secitem.h"

template <class T>
struct ScopedDelete {
  void operator()(T* ptr) {
    if (ptr) {
      PORT_FreeArena(ptr->arena, PR_TRUE);
    }
  }
};

typedef std::unique_ptr<RSAPrivateKey, ScopedDelete<RSAPrivateKey>>
    ScopedRSAPrivateKey;

class RSATest : public ::testing::Test {
 protected:
  RSAPrivateKey* CreateKeyWithExponent(int keySizeInBits,
                                       unsigned char publicExponent) {
    SECItem exp = {siBuffer, 0, 0};
    unsigned char pubExp[1] = {publicExponent};
    exp.data = pubExp;
    exp.len = 1;

    return RSA_NewKey(keySizeInBits, &exp);
  }
};

TEST_F(RSATest, expOneTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x01));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSATest, expTwoTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x02));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSATest, expFourTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x04));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSATest, WrongKeysizeTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2047, 0x03));
  ASSERT_TRUE(key == nullptr);
}

TEST_F(RSATest, expThreeTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x03));
#ifdef NSS_FIPS_DISABLED
  ASSERT_TRUE(key != nullptr);
#else
  ASSERT_TRUE(key == nullptr);
#endif
}

TEST_F(RSATest, DecryptBlockTestErrors) {
  unsigned char pubExp[3] = {0x01, 0x00, 0x01};
  SECItem exp = {siBuffer, pubExp, 3};
  ScopedRSAPrivateKey key(RSA_NewKey(2048, &exp));
  ASSERT_TRUE(key);
  uint8_t out[10] = {0};
  uint8_t in_small[100] = {0};
  unsigned int outputLen = 0;
  unsigned int maxOutputLen = sizeof(out);

  // This should fail because input the same size as the modulus (256).
  SECStatus rv = RSA_DecryptBlock(key.get(), out, &outputLen, maxOutputLen,
                                  in_small, sizeof(in_small));
  EXPECT_EQ(SECFailure, rv);

  uint8_t in[256] = {0};
  // This should fail because the padding checks will fail.
  rv = RSA_DecryptBlock(key.get(), out, &outputLen, maxOutputLen, in,
                        sizeof(in));
  EXPECT_EQ(SECFailure, rv);
  // outputLen should be maxOutputLen.
  EXPECT_EQ(maxOutputLen, outputLen);

  // This should fail because the padding checks will fail.
  uint8_t out_long[260] = {0};
  maxOutputLen = sizeof(out_long);
  rv = RSA_DecryptBlock(key.get(), out_long, &outputLen, maxOutputLen, in,
                        sizeof(in));
  EXPECT_EQ(SECFailure, rv);
  // outputLen should <= 256-11=245.
  EXPECT_LE(outputLen, 245u);
  // Everything over 256 must be 0 in the output.
  uint8_t out_long_test[4] = {0};
  EXPECT_EQ(0, memcmp(out_long_test, &out_long[256], 4));
}
