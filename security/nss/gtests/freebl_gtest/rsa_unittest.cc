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

class RSANewKeyTest : public ::testing::Test {
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

TEST_F(RSANewKeyTest, expOneTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x01));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSANewKeyTest, expTwoTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x02));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSANewKeyTest, expFourTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x04));
  ASSERT_TRUE(key == nullptr);
}
TEST_F(RSANewKeyTest, WrongKeysizeTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2047, 0x03));
  ASSERT_TRUE(key == nullptr);
}

TEST_F(RSANewKeyTest, expThreeTest) {
  ScopedRSAPrivateKey key(CreateKeyWithExponent(2048, 0x03));
#ifdef NSS_FIPS_DISABLED
  ASSERT_TRUE(key != nullptr);
#else
  ASSERT_TRUE(key == nullptr);
#endif
}
