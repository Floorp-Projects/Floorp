/*
 * shake_unittest.cc - unittests for SHAKE-128 and SHAKE-256 XOFs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "blapi.h"
#include "nspr.h"
#include "nss.h"
#include "secerr.h"

#include <cstdlib>
#include <iostream>
#include <memory>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

template <class T>
struct ScopedDelete128 {
  void operator()(T* ptr) {
    if (ptr) {
      SHAKE_128_DestroyContext(ptr, PR_TRUE);
    }
  }
};

template <class T>
struct ScopedDelete256 {
  void operator()(T* ptr) {
    if (ptr) {
      SHAKE_256_DestroyContext(ptr, PR_TRUE);
    }
  }
};

typedef std::unique_ptr<SHAKE_128Context, ScopedDelete128<SHAKE_128Context>>
    ScopedSHAKE_128Context;

typedef std::unique_ptr<SHAKE_256Context, ScopedDelete256<SHAKE_256Context>>
    ScopedSHAKE_256Context;

class SHAKE_128Tests : public ::testing::Test {};
class SHAKE_256Tests : public ::testing::Test {};

TEST_F(SHAKE_128Tests, TestVector1) {
  ScopedSHAKE_128Context ctx(SHAKE_128_NewContext());
  ASSERT_TRUE(ctx) << "SHAKE_128_NewContext failed!";

  std::vector<uint8_t> digest(16);
  SECStatus rv = SHAKE_128_Hash(digest.data(), 16, "abc");
  std::vector<uint8_t> expected = {0x58, 0x81, 0x09, 0x2d, 0xd8, 0x18,
                                   0xbf, 0x5c, 0xf8, 0xa3, 0xdd, 0xb7,
                                   0x93, 0xfb, 0xcb, 0xa7};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}

TEST_F(SHAKE_128Tests, TestVector2) {
  ScopedSHAKE_128Context ctx(SHAKE_128_NewContext());
  ASSERT_TRUE(ctx) << "SHAKE_128_NewContext failed!";

  std::vector<uint8_t> digest(8);
  SECStatus rv = SHAKE_128_Hash(digest.data(), 8, "hello123");
  std::vector<uint8_t> expected = {0x1b, 0x85, 0x86, 0x15,
                                   0x10, 0xbc, 0x4d, 0x8e};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}

TEST_F(SHAKE_256Tests, TestVector1) {
  ScopedSHAKE_256Context ctx(SHAKE_256_NewContext());
  ASSERT_TRUE(ctx) << "SHAKE_256_NewContext failed!";

  std::vector<uint8_t> digest(16);
  SECStatus rv = SHAKE_256_Hash(digest.data(), 16, "abc");
  std::vector<uint8_t> expected = {0x48, 0x33, 0x66, 0x60, 0x13, 0x60,
                                   0xa8, 0x77, 0x1c, 0x68, 0x63, 0x08,
                                   0x0c, 0xc4, 0x11, 0x4d};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}

TEST_F(SHAKE_256Tests, TestVector2) {
  ScopedSHAKE_256Context ctx(SHAKE_256_NewContext());
  ASSERT_TRUE(ctx) << "SHAKE_256_NewContext failed!";

  std::vector<uint8_t> digest(8);
  SECStatus rv = SHAKE_256_Hash(digest.data(), 8, "hello123");
  std::vector<uint8_t> expected = {0xad, 0xe6, 0x12, 0xba,
                                   0x26, 0x5f, 0x92, 0xde};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}
