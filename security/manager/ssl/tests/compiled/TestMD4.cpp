/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "md4.h"
#include "mozilla/Casting.h"

// The md4 implementation isn't built as a separate library. This is the easiest
// way to expose the symbols necessary to test the implementation.
#include "md4.c"

struct rfc1320_test_value {
  const char* data;
  const uint8_t md4[16];
};

nsresult
TestMD4()
{
  rfc1320_test_value rfc1320_test_values[] = {
    {
      "",
      { 0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31, 0xb7, 0x3c, 0x59, 0xd7, 0xe0, 0xc0, 0x89, 0xc0 }
    },
    {
      "a",
      { 0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46, 0x24, 0x5e, 0x05, 0xfb, 0xdb, 0xd6, 0xfb, 0x24 }
    },
    {
      "abc",
      { 0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52, 0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d }
    },
    {
      "message digest",
      { 0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8, 0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b }
    },
    {
      "abcdefghijklmnopqrstuvwxyz",
      { 0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd, 0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9 },
    },
    {
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
      { 0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35, 0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4 },
    },
    {
      "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
      { 0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19, 0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36 },
    }
  };

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(rfc1320_test_values); i++) {
    uint8_t md4_result[16];
    md4sum(mozilla::BitwiseCast<const uint8_t*, const char*>(
             rfc1320_test_values[i].data),
           strlen(rfc1320_test_values[i].data), md4_result);
    if (memcmp(md4_result, rfc1320_test_values[i].md4, 16) != 0) {
      fail("MD4 comparison test value #%d from RFC1320 failed", i + 1);
      return NS_ERROR_FAILURE;
    }
    passed("MD4 comparison test value #%d from RFC1320 passed", i + 1);
  }
  return NS_OK;
}

int
main(int argc, char* argv[])
{
  return NS_FAILED(TestMD4()) ? 1 : 0;
}
