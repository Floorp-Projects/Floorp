/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include <memory>
#include "nssb64.h"

#include "gtest/gtest.h"
#include "scoped_ptrs_util.h"

namespace nss_test {

class B64EncodeDecodeTest : public ::testing::Test {
 public:
  void TestDecodeStr(const std::string &str) {
    ScopedSECItem tmp(
        NSSBase64_DecodeBuffer(nullptr, nullptr, str.c_str(), str.size()));
    ASSERT_TRUE(tmp);
    char *out = NSSBase64_EncodeItem(nullptr, nullptr, 0, tmp.get());
    ASSERT_TRUE(out);
    ASSERT_EQ(std::string(out), str);
    PORT_Free(out);
  }
  bool TestEncodeItem(SECItem *item) {
    bool rv = true;
    char *out = NSSBase64_EncodeItem(nullptr, nullptr, 0, item);
    rv = !!out;
    if (out) {
      ScopedSECItem tmp(
          NSSBase64_DecodeBuffer(nullptr, nullptr, out, strlen(out)));
      EXPECT_TRUE(tmp);
      EXPECT_EQ(SECEqual, SECITEM_CompareItem(item, tmp.get()));
      PORT_Free(out);
    }
    return rv;
  }
  bool TestFakeDecode(size_t str_len) {
    std::string str(str_len, 'A');
    ScopedSECItem tmp(
        NSSBase64_DecodeBuffer(nullptr, nullptr, str.c_str(), str.size()));
    return !!tmp;
  }
  bool TestFakeEncode(size_t len) {
    std::vector<uint8_t> data(len, 0x30);
    SECItem tmp = {siBuffer, data.data(),
                   static_cast<unsigned int>(data.size())};
    return TestEncodeItem(&tmp);
  }

 protected:
};

TEST_F(B64EncodeDecodeTest, DecEncTest) { TestDecodeStr("VGhpcyBpcyBOU1Mh"); }

TEST_F(B64EncodeDecodeTest, EncDecTest) {
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  SECItem tmp = {siBuffer, data, sizeof(data)};
  TestEncodeItem(&tmp);
}

TEST_F(B64EncodeDecodeTest, FakeDecTest) { EXPECT_TRUE(TestFakeDecode(100)); }

TEST_F(B64EncodeDecodeTest, FakeEncDecTest) {
  EXPECT_TRUE(TestFakeEncode(100));
}

// These takes a while ...
TEST_F(B64EncodeDecodeTest, DISABLED_LongFakeDecTest1) {
  EXPECT_TRUE(TestFakeDecode(0x66666666));
}
TEST_F(B64EncodeDecodeTest, DISABLED_LongFakeEncDecTest1) {
  TestFakeEncode(0x3fffffff);
}
TEST_F(B64EncodeDecodeTest, DISABLED_LongFakeEncDecTest2) {
  EXPECT_FALSE(TestFakeEncode(0x40000000));
}

}  // namespace nss_test
