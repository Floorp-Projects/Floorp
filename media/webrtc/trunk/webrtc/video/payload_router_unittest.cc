/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <list>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/video/payload_router.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class PayloadRouterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    payload_router_.reset(new PayloadRouter());
  }
  rtc::scoped_ptr<PayloadRouter> payload_router_;
};

TEST_F(PayloadRouterTest, SendOnOneModule) {
  MockRtpRtcp rtp;
  std::list<RtpRtcp*> modules(1, &rtp);

  payload_router_->SetSendingRtpModules(modules);

  uint8_t payload = 'a';
  FrameType frame_type = kVideoFrameKey;
  int8_t payload_type = 96;

  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));

  payload_router_->set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(1);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                            &payload, 1, NULL, NULL));

  payload_router_->set_active(false);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));

  payload_router_->set_active(true);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(1);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                            &payload, 1, NULL, NULL));

  modules.clear();
  payload_router_->SetSendingRtpModules(modules);
  EXPECT_CALL(rtp, SendOutgoingData(frame_type, payload_type, 0, 0, _, 1, NULL,
                                    NULL))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type, payload_type, 0, 0,
                                             &payload, 1, NULL, NULL));
}

TEST_F(PayloadRouterTest, SendSimulcast) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);

  payload_router_->SetSendingRtpModules(modules);

  uint8_t payload_1 = 'a';
  FrameType frame_type_1 = kVideoFrameKey;
  int8_t payload_type_1 = 96;
  RTPVideoHeader rtp_hdr_1;
  rtp_hdr_1.simulcastIdx = 0;

  payload_router_->set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(frame_type_1, payload_type_1, 0, 0, _, 1,
                                      NULL, &rtp_hdr_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                            &payload_1, 1, NULL, &rtp_hdr_1));

  uint8_t payload_2 = 'b';
  FrameType frame_type_2 = kVideoFrameDelta;
  int8_t payload_type_2 = 97;
  RTPVideoHeader rtp_hdr_2;
  rtp_hdr_2.simulcastIdx = 1;
  EXPECT_CALL(rtp_2, SendOutgoingData(frame_type_2, payload_type_2, 0, 0, _, 1,
                                      NULL, &rtp_hdr_2))
      .Times(1);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_TRUE(payload_router_->RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                            &payload_2, 1, NULL, &rtp_hdr_2));

  // Inactive.
  payload_router_->set_active(false);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                             &payload_1, 1, NULL, &rtp_hdr_1));
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_2, payload_type_2, 0, 0,
                                             &payload_2, 1, NULL, &rtp_hdr_2));

  // Invalid simulcast index.
  payload_router_->set_active(true);
  EXPECT_CALL(rtp_1, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(rtp_2, SendOutgoingData(_, _, _, _, _, _, _, _))
      .Times(0);
  rtp_hdr_1.simulcastIdx = 2;
  EXPECT_FALSE(payload_router_->RoutePayload(frame_type_1, payload_type_1, 0, 0,
                                             &payload_1, 1, NULL, &rtp_hdr_1));
}

TEST_F(PayloadRouterTest, MaxPayloadLength) {
  // Without any limitations from the modules, verify we get the max payload
  // length for IP/UDP/SRTP with a MTU of 150 bytes.
  const size_t kDefaultMaxLength = 1500 - 20 - 8 - 12 - 4;
  EXPECT_EQ(kDefaultMaxLength, payload_router_->DefaultMaxPayloadLength());
  EXPECT_EQ(kDefaultMaxLength, payload_router_->MaxPayloadLength());

  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  // Modules return a higher length than the default value.
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kDefaultMaxLength + 10));
  EXPECT_EQ(kDefaultMaxLength, payload_router_->MaxPayloadLength());

  // The modules return a value lower than default.
  const size_t kTestMinPayloadLength = 1001;
  EXPECT_CALL(rtp_1, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength + 10));
  EXPECT_CALL(rtp_2, MaxDataPayloadLength())
      .Times(1)
      .WillOnce(Return(kTestMinPayloadLength));
  EXPECT_EQ(kTestMinPayloadLength, payload_router_->MaxPayloadLength());
}

TEST_F(PayloadRouterTest, SetTargetSendBitrates) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  std::list<RtpRtcp*> modules;
  modules.push_back(&rtp_1);
  modules.push_back(&rtp_2);
  payload_router_->SetSendingRtpModules(modules);

  const uint32_t bitrate_1 = 10000;
  const uint32_t bitrate_2 = 76543;
  std::vector<uint32_t> bitrates(2, bitrate_1);
  bitrates[1] = bitrate_2;
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(1);
  payload_router_->SetTargetSendBitrates(bitrates);

  bitrates.resize(1);
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(0);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(0);
  payload_router_->SetTargetSendBitrates(bitrates);

  bitrates.resize(3);
  bitrates[1] = bitrate_2;
  bitrates[2] = bitrate_1 + bitrate_2;
  EXPECT_CALL(rtp_1, SetTargetSendBitrate(bitrate_1))
      .Times(1);
  EXPECT_CALL(rtp_2, SetTargetSendBitrate(bitrate_2))
      .Times(1);
  payload_router_->SetTargetSendBitrates(bitrates);
}
}  // namespace webrtc
