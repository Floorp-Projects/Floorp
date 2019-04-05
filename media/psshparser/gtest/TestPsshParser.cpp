/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include <algorithm>
#include <stdint.h>
#include <vector>

#include "psshparser/PsshParser.h"
#include "mozilla/ArrayUtils.h"

using namespace std;

// This is the CENC initData from Google's web-platform tests.
// https://github.com/w3c/web-platform-tests/blob/master/encrypted-media/Google/encrypted-media-utils.js#L50
const uint8_t gGoogleWPTCencInitData[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x00,                          // size = 0
  0x70, 0x73, 0x73, 0x68,                          // 'pssh'
  0x01,                                            // version = 1
  0x00, 0x00, 0x00,                                // flags
  0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
  0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
  0x00, 0x00, 0x00, 0x01,                          // key count
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // key
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x00, 0x00, 0x00, 0x00                           // datasize
    // clang-format on
};

// Example CENC initData from the EME spec format registry:
// https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html
const uint8_t gW3SpecExampleCencInitData[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x44, 0x70, 0x73, 0x73, 0x68, // BMFF box header (68 bytes, 'pssh')
  0x01, 0x00, 0x00, 0x00,                         // Full box header (version = 1, flags = 0)
  0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, // SystemID
  0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
  0x00, 0x00, 0x00, 0x02,                         // KID_count (2)
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // First KID ("0123456789012345")
  0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, // Second KID ("ABCDEFGHIJKLMNOP")
  0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x00, 0x00, 0x00, 0x00                         // Size of Data (0)
    // clang-format on
};

// Invalid box size, would overflow if used.
const uint8_t gOverflowBoxSize[] = {
    0xff, 0xff, 0xff, 0xff,  // size = UINT32_MAX
};

// Valid box size, but key count too large.
const uint8_t gTooLargeKeyCountInitData[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x34,                          // size = too big a number
  0x70, 0x73, 0x73, 0x68,                          // 'pssh'
  0x01,                                            // version = 1
  0xff, 0xff, 0xff,                                // flags
  0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
  0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
  0xff, 0xff, 0xff, 0xff,                          // key count = UINT32_MAX
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // key
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff                           // datasize
    // clang-format on
};

// Non common SystemID PSSH.
// No keys Ids can be extracted, but don't consider the box invalid.
const uint8_t gNonCencInitData[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x5c,                          // size = 92
  0x70, 0x73, 0x73, 0x68,                          // 'pssh'
  0x01,                                            // version = 1
  0x00, 0x00, 0x00,                                // flags
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Invalid SystemID
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Some data to pad out the box.
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    // clang-format on
};

const uint8_t gNonPSSHBoxZeroSize[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x00,                          // size = 0
  0xff, 0xff, 0xff, 0xff,                          // something other than 'pssh'
    // clang-format on
};

// Two lots of the google init data. To ensure we handle
// multiple boxes with size 0.
const uint8_t g2xGoogleWPTCencInitData[] = {
    // clang-format off
  0x00, 0x00, 0x00, 0x00,                          // size = 0
  0x70, 0x73, 0x73, 0x68,                          // 'pssh'
  0x01,                                            // version = 1
  0x00, 0x00, 0x00,                                // flags
  0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
  0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
  0x00, 0x00, 0x00, 0x01,                          // key count
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // key
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x00, 0x00, 0x00, 0x00,                          // datasize

  0x00, 0x00, 0x00, 0x00,                          // size = 0
  0x70, 0x73, 0x73, 0x68,                          // 'pssh'
  0x01,                                            // version = 1
  0x00, 0x00, 0x00,                                // flags
  0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02,  // Common SystemID
  0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B,
  0x00, 0x00, 0x00, 0x01,                          // key count
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // key
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  0x00, 0x00, 0x00, 0x00                           // datasize
    // clang-format on
};

TEST(PsshParser, ParseCencInitData)
{
  std::vector<std::vector<uint8_t>> keyIds;
  bool rv;

  rv = ParseCENCInitData(gGoogleWPTCencInitData,
                         MOZ_ARRAY_LENGTH(gGoogleWPTCencInitData), keyIds);
  EXPECT_TRUE(rv);
  EXPECT_EQ(1u, keyIds.size());
  EXPECT_EQ(16u, keyIds[0].size());
  EXPECT_EQ(0, memcmp(&keyIds[0].front(), &gGoogleWPTCencInitData[32], 16));

  rv = ParseCENCInitData(gW3SpecExampleCencInitData,
                         MOZ_ARRAY_LENGTH(gW3SpecExampleCencInitData), keyIds);
  EXPECT_TRUE(rv);
  EXPECT_EQ(2u, keyIds.size());
  EXPECT_EQ(16u, keyIds[0].size());
  EXPECT_EQ(0, memcmp(&keyIds[0].front(), &gW3SpecExampleCencInitData[32], 16));
  EXPECT_EQ(0, memcmp(&keyIds[1].front(), &gW3SpecExampleCencInitData[48], 16));

  rv = ParseCENCInitData(gOverflowBoxSize, MOZ_ARRAY_LENGTH(gOverflowBoxSize),
                         keyIds);
  EXPECT_FALSE(rv);
  EXPECT_EQ(0u, keyIds.size());

  rv = ParseCENCInitData(gTooLargeKeyCountInitData,
                         MOZ_ARRAY_LENGTH(gTooLargeKeyCountInitData), keyIds);
  EXPECT_FALSE(rv);
  EXPECT_EQ(0u, keyIds.size());

  rv = ParseCENCInitData(gNonCencInitData, MOZ_ARRAY_LENGTH(gNonCencInitData),
                         keyIds);
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, keyIds.size());

  rv = ParseCENCInitData(gNonPSSHBoxZeroSize,
                         MOZ_ARRAY_LENGTH(gNonPSSHBoxZeroSize), keyIds);
  EXPECT_FALSE(rv);
  EXPECT_EQ(0u, keyIds.size());

  rv = ParseCENCInitData(g2xGoogleWPTCencInitData,
                         MOZ_ARRAY_LENGTH(g2xGoogleWPTCencInitData), keyIds);
  EXPECT_TRUE(rv);
  EXPECT_EQ(2u, keyIds.size());
  EXPECT_EQ(16u, keyIds[0].size());
  EXPECT_EQ(16u, keyIds[1].size());
  EXPECT_EQ(0, memcmp(&keyIds[0].front(), &g2xGoogleWPTCencInitData[32], 16));
  EXPECT_EQ(0, memcmp(&keyIds[1].front(), &g2xGoogleWPTCencInitData[84], 16));
}
