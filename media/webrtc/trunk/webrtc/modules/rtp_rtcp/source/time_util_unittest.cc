/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/rtp_rtcp/source/time_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

TEST(TimeUtilTest, CompactNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const NtpTime kNtp(kNtpSec, kNtpFrac);
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, CompactNtp(kNtp));
}

TEST(TimeUtilTest, CompactNtpToMs) {
  const NtpTime ntp1(0x12345, 0x23456);
  const NtpTime ntp2(0x12654, 0x64335);
  uint32_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);

  uint32_t ntp_to_ms_diff = CompactNtpIntervalToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpToMsWithWrap) {
  const NtpTime ntp1(0x1ffff, 0x23456);
  const NtpTime ntp2(0x20000, 0x64335);
  uint32_t ms_diff = ntp2.ToMs() - ntp1.ToMs();

  // While ntp2 > ntp1, there compact ntp presentation happen to be opposite.
  // That shouldn't be a problem as long as unsigned arithmetic is used.
  ASSERT_GT(ntp2.ToMs(), ntp1.ToMs());
  ASSERT_LT(CompactNtp(ntp2), CompactNtp(ntp1));

  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  uint32_t ntp_to_ms_diff = CompactNtpIntervalToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}

TEST(TimeUtilTest, CompactNtpToMsLarge) {
  const NtpTime ntp1(0x10000, 0x23456);
  const NtpTime ntp2(0x1ffff, 0x64335);
  uint32_t ms_diff = ntp2.ToMs() - ntp1.ToMs();
  // Ntp difference close to maximum of ~18 hours should convert correctly too.
  ASSERT_GT(ms_diff, 18u * 3600 * 1000);
  uint32_t ntp_diff = CompactNtp(ntp2) - CompactNtp(ntp1);
  uint32_t ntp_to_ms_diff = CompactNtpIntervalToMs(ntp_diff);

  EXPECT_NEAR(ms_diff, ntp_to_ms_diff, 1);
}
}  // namespace webrtc
