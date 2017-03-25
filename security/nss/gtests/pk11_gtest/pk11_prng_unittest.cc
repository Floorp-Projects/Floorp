/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "blapi.h"
#include "pk11pub.h"

#include "gtest/gtest.h"

namespace nss_test {

class PK11PrngTest : public ::testing::Test {};

#ifdef UNSAFE_FUZZER_MODE

// Test that two consecutive calls to the RNG return two distinct values.
TEST_F(PK11PrngTest, Fuzz_DetPRNG) {
  std::vector<uint8_t> rnd1(2048, 0);
  std::vector<uint8_t> rnd2(2048, 0);

  SECStatus rv = PK11_GenerateRandom(rnd1.data(), rnd1.size());
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd2.data(), rnd2.size());
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_NE(rnd1, rnd2);
}

// Test that two consecutive calls to the RNG return two equal values
// when the RNG's internal state is reset before each call.
TEST_F(PK11PrngTest, Fuzz_DetPRNG_Reset) {
  std::vector<uint8_t> rnd1(2048, 0);
  std::vector<uint8_t> rnd2(2048, 0);

  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));

  SECStatus rv = PK11_GenerateRandom(rnd1.data(), rnd1.size());
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));

  rv = PK11_GenerateRandom(rnd2.data(), rnd2.size());
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_EQ(rnd1, rnd2);
}

// Test that the RNG's internal state progresses in a consistent manner.
TEST_F(PK11PrngTest, Fuzz_DetPRNG_StatefulReset) {
  std::vector<uint8_t> rnd1(2048, 0);
  std::vector<uint8_t> rnd2(2048, 0);

  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));

  SECStatus rv = PK11_GenerateRandom(rnd1.data(), rnd1.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd1.data() + 1024, rnd1.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_EQ(SECSuccess, RNG_RandomUpdate(NULL, 0));

  rv = PK11_GenerateRandom(rnd2.data(), rnd2.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd2.data() + 1024, rnd2.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_EQ(rnd1, rnd2);
}

TEST_F(PK11PrngTest, Fuzz_DetPRNG_Seed) {
  std::vector<uint8_t> rnd1(2048, 0);
  std::vector<uint8_t> rnd2(2048, 0);
  std::vector<uint8_t> seed = {0x01, 0x22, 0xAA, 0x45};

  SECStatus rv = PK11_RandomUpdate(seed.data(), seed.size());
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd1.data(), rnd1.size());
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd2.data(), rnd2.size());
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_NE(rnd1, rnd2);
}

TEST_F(PK11PrngTest, Fuzz_DetPRNG_StatefulReset_Seed) {
  std::vector<uint8_t> rnd1(2048, 0);
  std::vector<uint8_t> rnd2(2048, 0);
  std::vector<uint8_t> seed = {0x01, 0x22, 0xAA, 0x45};

  SECStatus rv = PK11_RandomUpdate(seed.data(), seed.size());
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd1.data(), rnd1.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd1.data() + 1024, rnd1.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_RandomUpdate(seed.data(), seed.size());
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd2.data(), rnd2.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  rv = PK11_GenerateRandom(rnd2.data() + 1024, rnd2.size() - 1024);
  EXPECT_EQ(rv, SECSuccess);

  EXPECT_EQ(rnd1, rnd2);
}

#endif

}  // namespace nss_test
