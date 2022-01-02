/*
 * blake2b_unittest.cc - unittests for blake2b hash function
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

#include "kat/blake2b_kat.h"

template <class T>
struct ScopedDelete {
  void operator()(T* ptr) {
    if (ptr) {
      BLAKE2B_DestroyContext(ptr, PR_TRUE);
    }
  }
};

typedef std::unique_ptr<BLAKE2BContext, ScopedDelete<BLAKE2BContext>>
    ScopedBLAKE2BContext;

class Blake2BTests : public ::testing::Test {};

class Blake2BKAT
    : public ::testing::TestWithParam<std::pair<int, std::vector<uint8_t>>> {};

class Blake2BKATUnkeyed : public Blake2BKAT {};
class Blake2BKATKeyed : public Blake2BKAT {};

TEST_P(Blake2BKATUnkeyed, Unkeyed) {
  std::vector<uint8_t> values(BLAKE2B512_LENGTH);
  SECStatus rv =
      BLAKE2B_HashBuf(values.data(), kat_data.data(), std::get<0>(GetParam()));
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(values, std::get<1>(GetParam()));
}

TEST_P(Blake2BKATKeyed, Keyed) {
  std::vector<uint8_t> values(BLAKE2B512_LENGTH);
  SECStatus rv = BLAKE2B_MAC_HashBuf(values.data(), kat_data.data(),
                                     std::get<0>(GetParam()), kat_key.data(),
                                     BLAKE2B_KEY_SIZE);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(values, std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(UnkeyedKAT, Blake2BKATUnkeyed,
                         ::testing::ValuesIn(TestcasesUnkeyed));
INSTANTIATE_TEST_SUITE_P(KeyedKAT, Blake2BKATKeyed,
                         ::testing::ValuesIn(TestcasesKeyed));

TEST_F(Blake2BTests, ContextTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  SECStatus rv = BLAKE2B_Begin(ctx.get());
  ASSERT_EQ(SECSuccess, rv);

  size_t src_length = 252;
  const size_t quarter = 63;

  for (int i = 0; i < 4 && src_length > 0; i++) {
    rv = BLAKE2B_Update(ctx.get(), kat_data.data() + i * quarter,
                        PR_MIN(quarter, src_length));
    ASSERT_EQ(SECSuccess, rv);

    size_t len = BLAKE2B_FlattenSize(ctx.get());
    std::vector<unsigned char> ctxbytes(len);
    rv = BLAKE2B_Flatten(ctx.get(), ctxbytes.data());
    ASSERT_EQ(SECSuccess, rv);
    ScopedBLAKE2BContext ctx_cpy(BLAKE2B_Resurrect(ctxbytes.data(), NULL));
    ASSERT_TRUE(ctx_cpy) << "BLAKE2B_Resurrect failed!";
    ASSERT_EQ(SECSuccess, PORT_Memcmp(ctx.get(), ctx_cpy.get(), len));
    src_length -= quarter;
  }
  ASSERT_EQ(0U, src_length);

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  rv = BLAKE2B_End(ctx.get(), digest.data(), nullptr, BLAKE2B512_LENGTH);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(std::get<1>(TestcasesUnkeyed[252]), digest)
      << "BLAKE2B_End failed!";
}

TEST_F(Blake2BTests, ContextTest2) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  SECStatus rv = BLAKE2B_Begin(ctx.get());
  ASSERT_EQ(SECSuccess, rv);

  rv = BLAKE2B_Update(ctx.get(), kat_data.data(), 128);
  ASSERT_EQ(SECSuccess, rv);
  rv = BLAKE2B_Update(ctx.get(), kat_data.data() + 128, 127);
  ASSERT_EQ(SECSuccess, rv);

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  rv = BLAKE2B_End(ctx.get(), digest.data(), nullptr, BLAKE2B512_LENGTH);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(std::get<1>(TestcasesUnkeyed[255]), digest)
      << "BLAKE2B_End failed!";
}

TEST_F(Blake2BTests, NullContextTest) {
  SECStatus rv = BLAKE2B_Begin(nullptr);
  ASSERT_EQ(SECFailure, rv);

  rv = BLAKE2B_Update(nullptr, kat_data.data(), 128);
  ASSERT_EQ(SECFailure, rv);

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  rv = BLAKE2B_End(nullptr, digest.data(), nullptr, BLAKE2B512_LENGTH);
  ASSERT_EQ(SECFailure, rv);
}

TEST_F(Blake2BTests, CloneTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ScopedBLAKE2BContext cloned_ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";
  ASSERT_TRUE(cloned_ctx) << "BLAKE2B_NewContext failed!";

  SECStatus rv = BLAKE2B_Begin(ctx.get());
  ASSERT_EQ(SECSuccess, rv);
  rv = BLAKE2B_Update(ctx.get(), kat_data.data(), 255);
  ASSERT_EQ(SECSuccess, rv);
  BLAKE2B_Clone(cloned_ctx.get(), ctx.get());

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  rv = BLAKE2B_End(cloned_ctx.get(), digest.data(), nullptr, BLAKE2B512_LENGTH);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(std::get<1>(TestcasesUnkeyed[255]), digest)
      << "BLAKE2B_End failed!";
}

TEST_F(Blake2BTests, NullTest) {
  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  SECStatus rv = BLAKE2B_HashBuf(digest.data(), nullptr, 0);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(std::get<1>(TestcasesUnkeyed[0]), digest);

  digest = std::vector<uint8_t>(BLAKE2B512_LENGTH);
  rv = BLAKE2B_MAC_HashBuf(digest.data(), nullptr, 0, kat_key.data(),
                           BLAKE2B_KEY_SIZE);
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(std::get<1>(TestcasesKeyed[0]), digest);
}

TEST_F(Blake2BTests, HashTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  SECStatus rv = BLAKE2B_Hash(digest.data(), "abc");
  std::vector<uint8_t> expected = {
      0xba, 0x80, 0xa5, 0x3f, 0x98, 0x1c, 0x4d, 0x0d, 0x6a, 0x27, 0x97,
      0xb6, 0x9f, 0x12, 0xf6, 0xe9, 0x4c, 0x21, 0x2f, 0x14, 0x68, 0x5a,
      0xc4, 0xb7, 0x4b, 0x12, 0xbb, 0x6f, 0xdb, 0xff, 0xa2, 0xd1, 0x7d,
      0x87, 0xc5, 0x39, 0x2a, 0xab, 0x79, 0x2d, 0xc2, 0x52, 0xd5, 0xde,
      0x45, 0x33, 0xcc, 0x95, 0x18, 0xd3, 0x8a, 0xa8, 0xdb, 0xf1, 0x92,
      0x5a, 0xb9, 0x23, 0x86, 0xed, 0xd4, 0x00, 0x99, 0x23};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}

TEST_F(Blake2BTests, LongHashTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  SECStatus rv = BLAKE2B_Hash(
      digest.data(),
      "qwertzuiopasdfghjklyxcvbnm123456789qwertzuiopasdfghjklyxcvbnm123456789qw"
      "ertzuiopasdfghjklyxcvbnm123456789qwertzuiopasdfghjklyxcvbnm123456789qwer"
      "tzuiopasdfghjklyxcvbnm123456789qwertzuiopasdfghjklyxcvbnm123456789qwertz"
      "uiopasdfghjklyxcvbnm123456789qwertzuiopasdfghjklyxcvbnm123456789");
  std::vector<uint8_t> expected = {
      0x1f, 0x9e, 0xe6, 0x5a, 0xa0, 0x36, 0x05, 0xfc, 0x41, 0x0e, 0x2f,
      0x55, 0x96, 0xfd, 0xb5, 0x9d, 0x85, 0x95, 0x5e, 0x24, 0x37, 0xe7,
      0x0d, 0xe4, 0xa0, 0x22, 0x4a, 0xe1, 0x59, 0x1f, 0x97, 0x03, 0x57,
      0x54, 0xf0, 0xca, 0x92, 0x75, 0x2f, 0x9e, 0x86, 0xeb, 0x82, 0x4f,
      0x9c, 0xf4, 0x02, 0x17, 0x7f, 0x76, 0x56, 0x26, 0x46, 0xf4, 0x07,
      0xfd, 0x1f, 0x78, 0xdb, 0x7b, 0x0d, 0x24, 0x43, 0xf0};
  ASSERT_EQ(SECSuccess, rv);
  EXPECT_EQ(expected, digest);
}

TEST_F(Blake2BTests, TruncatedHashTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  SECStatus rv = BLAKE2B_Begin(ctx.get());
  ASSERT_EQ(SECSuccess, rv);

  rv = BLAKE2B_Update(ctx.get(), kat_data.data(), 128);
  ASSERT_EQ(SECSuccess, rv);
  rv = BLAKE2B_Update(ctx.get(), kat_data.data() + 128, 127);
  ASSERT_EQ(SECSuccess, rv);

  size_t max_digest_len = BLAKE2B512_LENGTH - 5;
  std::vector<uint8_t> digest(max_digest_len);
  unsigned int digest_len;
  rv = BLAKE2B_End(ctx.get(), digest.data(), &digest_len, max_digest_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(digest.size(), digest_len);
  ASSERT_EQ(0, memcmp(std::get<1>(TestcasesUnkeyed[255]).data(), digest.data(),
                      max_digest_len))
      << "BLAKE2B_End failed!";
}

TEST_F(Blake2BTests, TruncatedHashTest2) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  SECStatus rv = BLAKE2B_Begin(ctx.get());
  ASSERT_EQ(SECSuccess, rv);

  rv = BLAKE2B_Update(ctx.get(), kat_data.data(), 128);
  ASSERT_EQ(SECSuccess, rv);
  rv = BLAKE2B_Update(ctx.get(), kat_data.data() + 128, 127);
  ASSERT_EQ(SECSuccess, rv);

  size_t max_digest_len = BLAKE2B512_LENGTH - 60;
  std::vector<uint8_t> digest(max_digest_len);
  unsigned int digest_len;
  rv = BLAKE2B_End(ctx.get(), digest.data(), &digest_len, max_digest_len);
  ASSERT_EQ(SECSuccess, rv);
  ASSERT_EQ(digest.size(), digest_len);
}

TEST_F(Blake2BTests, OverlongKeyTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  std::vector<uint8_t> key = {
      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
      0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32,
      0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33,
      0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34,
      0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
      0x36, 0x37, 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
  std::vector<uint8_t> data = {0x61, 0x62, 0x63};

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  SECStatus rv =
      BLAKE2B_MAC_HashBuf(digest.data(), data.data(), 3, key.data(), 65);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(Blake2BTests, EmptyKeyTest) {
  ScopedBLAKE2BContext ctx(BLAKE2B_NewContext());
  ASSERT_TRUE(ctx) << "BLAKE2B_NewContext failed!";

  uint8_t key[1];  // A vector.data() would give us a nullptr.
  std::vector<uint8_t> data = {0x61, 0x62, 0x63};

  std::vector<uint8_t> digest(BLAKE2B512_LENGTH);
  SECStatus rv = BLAKE2B_MAC_HashBuf(digest.data(), data.data(), 3, key, 0);
  EXPECT_EQ(SECFailure, rv);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}
