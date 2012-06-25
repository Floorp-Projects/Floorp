/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RtpHeaderExtensionMap.
 */

#include <gtest/gtest.h>

#include "rtp_header_extension.h"
#include "rtp_rtcp_defines.h"
#include "typedefs.h"

namespace webrtc {

class RtpHeaderExtensionTest : public ::testing::Test {
 protected:
  RtpHeaderExtensionTest() {}
  ~RtpHeaderExtensionTest() {}

  RtpHeaderExtensionMap map_;
  enum {kId = 3};
};

TEST_F(RtpHeaderExtensionTest, Register) {
  EXPECT_EQ(0, map_.Size());
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(1, map_.Size());
  EXPECT_EQ(0, map_.Deregister(kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, map_.Size());
}

TEST_F(RtpHeaderExtensionTest, RegisterIllegalArg) {
  // Valid range for id: [1-14].
  EXPECT_EQ(-1, map_.Register(kRtpExtensionTransmissionTimeOffset, 0));
  EXPECT_EQ(-1, map_.Register(kRtpExtensionTransmissionTimeOffset, 15));
}

TEST_F(RtpHeaderExtensionTest, DeregisterIllegalArg) {
  // Not registered.
  EXPECT_EQ(-1, map_.Deregister(kRtpExtensionTransmissionTimeOffset));
}

TEST_F(RtpHeaderExtensionTest, NonUniqueId) {
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(-1, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
}

TEST_F(RtpHeaderExtensionTest, GetTotalLength) {
  EXPECT_EQ(0, map_.GetTotalLengthInBytes());
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES +
            TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES,
            map_.GetTotalLengthInBytes());
}

TEST_F(RtpHeaderExtensionTest, GetLengthUntilBlockStart) {
  EXPECT_EQ(-1, map_.GetLengthUntilBlockStartInBytes(
      kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES,
      map_.GetLengthUntilBlockStartInBytes(
      kRtpExtensionTransmissionTimeOffset));
}

TEST_F(RtpHeaderExtensionTest, GetType) {
  RTPExtensionType typeOut;
  EXPECT_EQ(-1, map_.GetType(kId, &typeOut));

  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0, map_.GetType(kId, &typeOut));
  EXPECT_EQ(kRtpExtensionTransmissionTimeOffset, typeOut);
}

TEST_F(RtpHeaderExtensionTest, GetId) {
  uint8_t idOut;
  EXPECT_EQ(-1, map_.GetId(kRtpExtensionTransmissionTimeOffset, &idOut));

  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0, map_.GetId(kRtpExtensionTransmissionTimeOffset, &idOut));
  EXPECT_EQ(kId, idOut);
}

TEST_F(RtpHeaderExtensionTest, IterateTypes) {
  EXPECT_EQ(kRtpExtensionNone, map_.First());
  EXPECT_EQ(kRtpExtensionNone, map_.Next(kRtpExtensionTransmissionTimeOffset));

  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));

  EXPECT_EQ(kRtpExtensionTransmissionTimeOffset, map_.First());
  EXPECT_EQ(kRtpExtensionNone, map_.Next(kRtpExtensionTransmissionTimeOffset));
}

TEST_F(RtpHeaderExtensionTest, GetCopy) {
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));

  RtpHeaderExtensionMap mapOut;
  map_.GetCopy(&mapOut);
  EXPECT_EQ(1, mapOut.Size());
  EXPECT_EQ(kRtpExtensionTransmissionTimeOffset, mapOut.First());
}

TEST_F(RtpHeaderExtensionTest, Erase) {
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(1, map_.Size());
  map_.Erase();
  EXPECT_EQ(0, map_.Size());
}
}  // namespace webrtc
