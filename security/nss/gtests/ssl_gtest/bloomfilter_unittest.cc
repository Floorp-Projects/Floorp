/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

extern "C" {
#include "sslbloom.h"
}

#include "gtest_utils.h"

namespace nss_test {

// Some random-ish inputs to test with.  These don't result in collisions in any
// of the configurations that are tested below.
static const uint8_t kHashes1[] = {
    0x79, 0x53, 0xb8, 0xdd, 0x6b, 0x98, 0xce, 0x00, 0xb7, 0xdc, 0xe8,
    0x03, 0x70, 0x8c, 0xe3, 0xac, 0x06, 0x8b, 0x22, 0xfd, 0x0e, 0x34,
    0x48, 0xe6, 0xe5, 0xe0, 0x8a, 0xd6, 0x16, 0x18, 0xe5, 0x48};
static const uint8_t kHashes2[] = {
    0xc6, 0xdd, 0x6e, 0xc4, 0x76, 0xb8, 0x55, 0xf2, 0xa4, 0xfc, 0x59,
    0x04, 0xa4, 0x90, 0xdc, 0xa7, 0xa7, 0x0d, 0x94, 0x8f, 0xc2, 0xdc,
    0x15, 0x6d, 0x48, 0x93, 0x9d, 0x05, 0xbb, 0x9a, 0xbc, 0xc1};

typedef struct {
  unsigned int k;
  unsigned int bits;
} BloomFilterConfig;

class BloomFilterTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<BloomFilterConfig> {
 public:
  BloomFilterTest() : filter_() {}

  void SetUp() { Init(); }

  void TearDown() { sslBloom_Destroy(&filter_); }

 protected:
  void Init() {
    if (filter_.filter) {
      sslBloom_Destroy(&filter_);
    }
    ASSERT_EQ(SECSuccess,
              sslBloom_Init(&filter_, GetParam().k, GetParam().bits));
  }

  bool Check(const uint8_t* hashes) {
    return sslBloom_Check(&filter_, hashes) ? true : false;
  }

  void Add(const uint8_t* hashes, bool expect_collision = false) {
    EXPECT_EQ(expect_collision, sslBloom_Add(&filter_, hashes) ? true : false);
    EXPECT_TRUE(Check(hashes));
  }

  sslBloomFilter filter_;
};

TEST_P(BloomFilterTest, InitOnly) {}

TEST_P(BloomFilterTest, AddToEmpty) {
  EXPECT_FALSE(Check(kHashes1));
  Add(kHashes1);
}

TEST_P(BloomFilterTest, AddTwo) {
  Add(kHashes1);
  Add(kHashes2);
}

TEST_P(BloomFilterTest, AddOneTwice) {
  Add(kHashes1);
  Add(kHashes1, true);
}

TEST_P(BloomFilterTest, Zero) {
  Add(kHashes1);
  sslBloom_Zero(&filter_);
  EXPECT_FALSE(Check(kHashes1));
  EXPECT_FALSE(Check(kHashes2));
}

TEST_P(BloomFilterTest, Fill) {
  sslBloom_Fill(&filter_);
  EXPECT_TRUE(Check(kHashes1));
  EXPECT_TRUE(Check(kHashes2));
}

static const BloomFilterConfig kBloomFilterConfigurations[] = {
    {1, 1},    // 1 hash, 1 bit input - high chance of collision.
    {1, 2},    // 1 hash, 2 bits - smaller than the basic unit size.
    {1, 3},    // 1 hash, 3 bits - same as basic unit size.
    {1, 4},    // 1 hash, 4 bits - 2 octets each.
    {3, 10},   // 3 hashes over a reasonable number of bits.
    {3, 3},    // Test that we can read multiple bits.
    {4, 15},   // A credible filter.
    {2, 18},   // A moderately large allocation.
    {16, 16},  // Insane, use all of the bits from the hashes.
    {16, 9},   // This also uses all of the bits from the hashes.
};

INSTANTIATE_TEST_SUITE_P(BloomFilterConfigurations, BloomFilterTest,
                         ::testing::ValuesIn(kBloomFilterConfigurations));

}  // namespace nss_test
