/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpHeaderExtensionTest : public ::testing::Test {
 protected:
  RtpHeaderExtensionTest() {}
  ~RtpHeaderExtensionTest() {}

  RtpHeaderExtensionMap map_;
  static const uint8_t kId;
};

const uint8_t RtpHeaderExtensionTest::kId = 3;

TEST_F(RtpHeaderExtensionTest, Register) {
  EXPECT_EQ(0, map_.Size());
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_TRUE(map_.IsRegistered(kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(1, map_.Size());
  EXPECT_EQ(0, map_.Deregister(kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, map_.Size());

  EXPECT_EQ(0, map_.RegisterInactive(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0, map_.Size());
  EXPECT_TRUE(map_.IsRegistered(kRtpExtensionTransmissionTimeOffset));
  EXPECT_TRUE(map_.SetActive(kRtpExtensionTransmissionTimeOffset, true));
  EXPECT_EQ(1, map_.Size());
}

TEST_F(RtpHeaderExtensionTest, RegisterIllegalArg) {
  // Valid range for id: [1-14].
  EXPECT_EQ(-1, map_.Register(kRtpExtensionTransmissionTimeOffset, 0));
  EXPECT_EQ(-1, map_.Register(kRtpExtensionTransmissionTimeOffset, 15));
}

TEST_F(RtpHeaderExtensionTest, Idempotent) {
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0, map_.Deregister(kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, map_.Deregister(kRtpExtensionTransmissionTimeOffset));
}

TEST_F(RtpHeaderExtensionTest, NonUniqueId) {
  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(-1, map_.Register(kRtpExtensionAudioLevel, kId));
  EXPECT_EQ(-1, map_.RegisterInactive(kRtpExtensionAudioLevel, kId));
}

TEST_F(RtpHeaderExtensionTest, GetTotalLength) {
  EXPECT_EQ(0u, map_.GetTotalLengthInBytes());
  EXPECT_EQ(0, map_.RegisterInactive(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(0u, map_.GetTotalLengthInBytes());

  EXPECT_EQ(0, map_.Register(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(kRtpOneByteHeaderLength + kTransmissionTimeOffsetLength,
            map_.GetTotalLengthInBytes());
}

TEST_F(RtpHeaderExtensionTest, GetLengthUntilBlockStart) {
  EXPECT_EQ(-1, map_.GetLengthUntilBlockStartInBytes(
      kRtpExtensionTransmissionTimeOffset));
  EXPECT_EQ(0, map_.RegisterInactive(kRtpExtensionTransmissionTimeOffset, kId));
  EXPECT_EQ(-1, map_.GetLengthUntilBlockStartInBytes(
                    kRtpExtensionTransmissionTimeOffset));

  EXPECT_TRUE(map_.SetActive(kRtpExtensionTransmissionTimeOffset, true));
  EXPECT_EQ(static_cast<int>(kRtpOneByteHeaderLength),
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

  EXPECT_EQ(0, map_.RegisterInactive(kRtpExtensionTransmissionTimeOffset, kId));

  EXPECT_EQ(kRtpExtensionNone, map_.First());

  EXPECT_TRUE(map_.SetActive(kRtpExtensionTransmissionTimeOffset, true));

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
