/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "scoped_ptrs_util.h"

namespace nss_test {

class MemcmpZeroTest : public ::testing::Test {
 protected:
  unsigned int test_memcmp_zero(const std::vector<uint8_t> &mem) {
    return NSS_SecureMemcmpZero(mem.data(), mem.size());
  };
};

TEST_F(MemcmpZeroTest, TestMemcmpZeroTrue) {
  unsigned int rv = test_memcmp_zero(std::vector<uint8_t>(37, 0));
  EXPECT_EQ(0U, rv);
}

TEST_F(MemcmpZeroTest, TestMemcmpZeroFalse5) {
  std::vector<uint8_t> vec(37, 0);
  vec[5] = 1;
  unsigned int rv = test_memcmp_zero(vec);
  EXPECT_NE(0U, rv);
}

TEST_F(MemcmpZeroTest, TestMemcmpZeroFalse37) {
  std::vector<uint8_t> vec(37, 0);
  vec[vec.size() - 1] = 0xFF;
  unsigned int rv = test_memcmp_zero(vec);
  EXPECT_NE(0U, rv);
}

TEST_F(MemcmpZeroTest, TestMemcmpZeroFalse0) {
  std::vector<uint8_t> vec(37, 0);
  vec[0] = 1;
  unsigned int rv = test_memcmp_zero(vec);
  EXPECT_NE(0U, rv);
}

}  // namespace nss_test
