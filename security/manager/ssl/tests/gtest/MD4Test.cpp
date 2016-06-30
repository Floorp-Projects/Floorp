/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the md4.c implementation.

#include "gtest/gtest.h"
#include "md4.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"

struct RFC1320TestParams
{
  const char* data;
  const uint8_t expectedHash[16];
};

static const RFC1320TestParams RFC1320_TEST_PARAMS[] =
{
  {
    "",
    { 0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31,
      0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0 }
  },
  {
    "a",
    { 0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46,
      0x24, 0x5e, 0x05, 0xfb, 0xdb, 0xd6, 0xfb, 0x24 }
  },
  {
    "abc",
    { 0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52,
      0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d }
  },
  {
    "message digest",
    { 0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8,
      0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b }
  },
  {
    "abcdefghijklmnopqrstuvwxyz",
    { 0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd,
      0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9 },
  },
  {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    { 0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35,
      0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4 },
  },
  {
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
    { 0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19,
      0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36 },
  }
};

class psm_MD4
  : public ::testing::Test
  , public ::testing::WithParamInterface<RFC1320TestParams>
{
};

TEST_P(psm_MD4, RFC1320TestValues)
{
  const RFC1320TestParams& params(GetParam());
  uint8_t actualHash[16];
  md4sum(mozilla::BitwiseCast<const uint8_t*, const char*>(params.data),
         strlen(params.data), actualHash);
  EXPECT_TRUE(mozilla::PodEqual(actualHash, params.expectedHash))
    << "MD4 hashes aren't equal for input: '" << params.data << "'";
}

INSTANTIATE_TEST_CASE_P(psm_MD4, psm_MD4,
                        testing::ValuesIn(RFC1320_TEST_PARAMS));
