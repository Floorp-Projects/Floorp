/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "scoped_ptrs_util.h"

namespace nss_test {

class SelectTest : public ::testing::Test {
 protected:
  void test_select(std::vector<uint8_t> &dest, const std::vector<uint8_t> &src0,
                   const std::vector<uint8_t> &src1, unsigned char b) {
    EXPECT_EQ(src0.size(), src1.size());
    EXPECT_GE(dest.size(), src0.size());
    return NSS_SecureSelect(dest.data(), src0.data(), src1.data(), src0.size(),
                            b);
  };
};

TEST_F(SelectTest, TestSelectZero) {
  std::vector<uint8_t> dest(32, 255);
  std::vector<uint8_t> src0(32, 0);
  std::vector<uint8_t> src1(32, 1);
  test_select(dest, src0, src1, 0);
  EXPECT_EQ(dest, src0);
}

TEST_F(SelectTest, TestSelectOne) {
  std::vector<uint8_t> dest(32, 255);
  std::vector<uint8_t> src0(32, 0);
  std::vector<uint8_t> src1(32, 1);
  test_select(dest, src0, src1, 1);
  EXPECT_EQ(dest, src1);
}

TEST_F(SelectTest, TestSelect170) {
  std::vector<uint8_t> dest(32, 255);
  std::vector<uint8_t> src0(32, 0);
  std::vector<uint8_t> src1(32, 1);
  test_select(dest, src0, src1, 170);
  EXPECT_EQ(dest, src1);
}

TEST_F(SelectTest, TestSelect255) {
  std::vector<uint8_t> dest(32, 255);
  std::vector<uint8_t> src0(32, 0);
  std::vector<uint8_t> src1(32, 1);
  test_select(dest, src0, src1, 255);
  EXPECT_EQ(dest, src1);
}

}  // namespace nss_test
