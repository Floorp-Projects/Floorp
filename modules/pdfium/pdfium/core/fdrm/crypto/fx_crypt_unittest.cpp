// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Originally from chromium's /src/base/md5_unittest.cc.

#include "core/fdrm/crypto/fx_crypt.h"

#include <memory>
#include <string>

#include "core/fxcrt/fx_basic.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string CRYPT_ToBase16(const uint8_t* digest) {
  static char const zEncode[] = "0123456789abcdef";
  std::string ret;
  ret.resize(32);
  for (int i = 0, j = 0; i < 16; i++, j += 2) {
    uint8_t a = digest[i];
    ret[j] = zEncode[(a >> 4) & 0xf];
    ret[j + 1] = zEncode[a & 0xf];
  }
  return ret;
}

std::string CRYPT_MD5String(const char* str) {
  uint8_t digest[16];
  CRYPT_MD5Generate(reinterpret_cast<const uint8_t*>(str), strlen(str), digest);
  return CRYPT_ToBase16(digest);
}

}  // namespace

TEST(FXCRYPT, CRYPT_ToBase16) {
  uint8_t data[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                    0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};

  std::string actual = CRYPT_ToBase16(data);
  std::string expected = "d41d8cd98f00b204e9800998ecf8427e";

  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5GenerateEmtpyData) {
  uint8_t digest[16];
  const char data[] = "";
  uint32_t length = static_cast<uint32_t>(strlen(data));

  CRYPT_MD5Generate(reinterpret_cast<const uint8_t*>(data), length, digest);

  uint8_t expected[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                        0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(expected[i], digest[i]);
}

TEST(FXCRYPT, MD5GenerateOneByteData) {
  uint8_t digest[16];
  const char data[] = "a";
  uint32_t length = static_cast<uint32_t>(strlen(data));

  CRYPT_MD5Generate(reinterpret_cast<const uint8_t*>(data), length, digest);

  uint8_t expected[] = {0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8,
                        0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61};

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(expected[i], digest[i]);
}

TEST(FXCRYPT, MD5GenerateLongData) {
  const uint32_t length = 10 * 1024 * 1024 + 1;
  std::unique_ptr<char[]> data(new char[length]);

  for (uint32_t i = 0; i < length; ++i)
    data[i] = i & 0xFF;

  uint8_t digest[16];
  CRYPT_MD5Generate(reinterpret_cast<const uint8_t*>(data.get()), length,
                    digest);

  uint8_t expected[] = {0x90, 0xbd, 0x6a, 0xd9, 0x0a, 0xce, 0xf5, 0xad,
                        0xaa, 0x92, 0x20, 0x3e, 0x21, 0xc7, 0xa1, 0x3e};

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(expected[i], digest[i]);
}

TEST(FXCRYPT, ContextWithEmptyData) {
  CRYPT_md5_context ctx;
  CRYPT_MD5Start(&ctx);

  uint8_t digest[16];
  CRYPT_MD5Finish(&ctx, digest);

  uint8_t expected[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                        0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(expected[i], digest[i]);
}

TEST(FXCRYPT, ContextWithLongData) {
  CRYPT_md5_context ctx;
  CRYPT_MD5Start(&ctx);

  const uint32_t length = 10 * 1024 * 1024 + 1;
  std::unique_ptr<uint8_t[]> data(new uint8_t[length]);

  for (uint32_t i = 0; i < length; ++i)
    data[i] = i & 0xFF;

  uint32_t total = 0;
  while (total < length) {
    uint32_t len = 4097;  // intentionally not 2^k.
    if (len > length - total)
      len = length - total;

    CRYPT_MD5Update(&ctx, data.get() + total, len);
    total += len;
  }

  EXPECT_EQ(length, total);

  uint8_t digest[16];
  CRYPT_MD5Finish(&ctx, digest);

  uint8_t expected[] = {0x90, 0xbd, 0x6a, 0xd9, 0x0a, 0xce, 0xf5, 0xad,
                        0xaa, 0x92, 0x20, 0x3e, 0x21, 0xc7, 0xa1, 0x3e};

  for (int i = 0; i < 16; ++i)
    EXPECT_EQ(expected[i], digest[i]);
}

// Example data from http://www.ietf.org/rfc/rfc1321.txt A.5 Test Suite
TEST(FXCRYPT, MD5StringTestSuite1) {
  std::string actual = CRYPT_MD5String("");
  std::string expected = "d41d8cd98f00b204e9800998ecf8427e";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite2) {
  std::string actual = CRYPT_MD5String("a");
  std::string expected = "0cc175b9c0f1b6a831c399e269772661";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite3) {
  std::string actual = CRYPT_MD5String("abc");
  std::string expected = "900150983cd24fb0d6963f7d28e17f72";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite4) {
  std::string actual = CRYPT_MD5String("message digest");
  std::string expected = "f96b697d7cb7938d525a2f31aaf161d0";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite5) {
  std::string actual = CRYPT_MD5String("abcdefghijklmnopqrstuvwxyz");
  std::string expected = "c3fcd3d76192e4007dfb496cca67e13b";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite6) {
  std::string actual = CRYPT_MD5String(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789");
  std::string expected = "d174ab98d277d9f5a5611c2c9f419d9f";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, MD5StringTestSuite7) {
  std::string actual = CRYPT_MD5String(
      "12345678901234567890"
      "12345678901234567890"
      "12345678901234567890"
      "12345678901234567890");
  std::string expected = "57edf4a22be3c955ac49da2e2107b67a";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, ContextWithStringData) {
  CRYPT_md5_context ctx;
  CRYPT_MD5Start(&ctx);
  CRYPT_MD5Update(&ctx, reinterpret_cast<const uint8_t*>("abc"), 3);

  uint8_t digest[16];
  CRYPT_MD5Finish(&ctx, digest);

  std::string actual = CRYPT_ToBase16(digest);
  std::string expected = "900150983cd24fb0d6963f7d28e17f72";
  EXPECT_EQ(expected, actual);
}

TEST(FXCRYPT, Sha256TestB1) {
  // Example B.1 from FIPS 180-2: one-block message.
  const char* input = "abc";
  const uint8_t expected[32] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  uint8_t actual[32];
  CRYPT_SHA256Generate(reinterpret_cast<const uint8_t*>(input), strlen(input),
                       actual);
  for (size_t i = 0; i < 32; ++i)
    EXPECT_EQ(expected[i], actual[i]) << " at byte " << i;
}

TEST(FXCRYPT, Sha256TestB2) {
  // Example B.2 from FIPS 180-2: multi-block message.
  const char* input =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const uint8_t expected[32] = {0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
                                0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
                                0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
                                0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1};
  uint8_t actual[32];
  CRYPT_SHA256Generate(reinterpret_cast<const uint8_t*>(input), strlen(input),
                       actual);
  for (size_t i = 0; i < 32; ++i)
    EXPECT_EQ(expected[i], actual[i]) << " at byte " << i;
}
