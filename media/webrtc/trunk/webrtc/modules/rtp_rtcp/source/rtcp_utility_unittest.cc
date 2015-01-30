/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {

TEST(RtcpUtilityTest, MidNtp) {
  const uint32_t kNtpSec = 0x12345678;
  const uint32_t kNtpFrac = 0x23456789;
  const uint32_t kNtpMid = 0x56782345;
  EXPECT_EQ(kNtpMid, RTCPUtility::MidNtp(kNtpSec, kNtpFrac));
}

TEST(RtcpUtilityTest, NackRequests) {
  RTCPUtility::NackStats stats;
  EXPECT_EQ(0U, stats.unique_requests());
  EXPECT_EQ(0U, stats.requests());
  stats.ReportRequest(10);
  EXPECT_EQ(1U, stats.unique_requests());
  EXPECT_EQ(1U, stats.requests());

  stats.ReportRequest(10);
  EXPECT_EQ(1U, stats.unique_requests());
  stats.ReportRequest(11);
  EXPECT_EQ(2U, stats.unique_requests());

  stats.ReportRequest(11);
  EXPECT_EQ(2U, stats.unique_requests());
  stats.ReportRequest(13);
  EXPECT_EQ(3U, stats.unique_requests());

  stats.ReportRequest(11);
  EXPECT_EQ(3U, stats.unique_requests());
  EXPECT_EQ(6U, stats.requests());
}

TEST(RtcpUtilityTest, NackRequestsWithWrap) {
  RTCPUtility::NackStats stats;
  stats.ReportRequest(65534);
  EXPECT_EQ(1U, stats.unique_requests());

  stats.ReportRequest(65534);
  EXPECT_EQ(1U, stats.unique_requests());
  stats.ReportRequest(65535);
  EXPECT_EQ(2U, stats.unique_requests());

  stats.ReportRequest(65535);
  EXPECT_EQ(2U, stats.unique_requests());
  stats.ReportRequest(0);
  EXPECT_EQ(3U, stats.unique_requests());

  stats.ReportRequest(65535);
  EXPECT_EQ(3U, stats.unique_requests());
  stats.ReportRequest(0);
  EXPECT_EQ(3U, stats.unique_requests());
  stats.ReportRequest(1);
  EXPECT_EQ(4U, stats.unique_requests());
  EXPECT_EQ(8U, stats.requests());
}

}  // namespace webrtc

