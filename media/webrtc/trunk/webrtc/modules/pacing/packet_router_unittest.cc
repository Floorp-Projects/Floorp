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

#include "webrtc/base/checks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/pacing/include/packet_router.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/base/scoped_ptr.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace webrtc {

class PacketRouterTest : public ::testing::Test {
 public:
  PacketRouterTest() : packet_router_(new PacketRouter()) {}
 protected:
  const rtc::scoped_ptr<PacketRouter> packet_router_;
};

TEST_F(PacketRouterTest, TimeToSendPacket) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddRtpModule(&rtp_1);
  packet_router_->AddRtpModule(&rtp_2);

  const uint16_t kSsrc1 = 1234;
  uint16_t sequence_number = 17;
  uint64_t timestamp = 7890;
  bool retransmission = false;

  // Send on the first module by letting rtp_1 be sending with correct ssrc.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_1, TimeToSendPacket(kSsrc1, sequence_number, timestamp,
                                      retransmission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission));

  // Send on the second module by letting rtp_2 be sending, but not rtp_1.
  ++sequence_number;
  timestamp += 30;
  retransmission = true;
  const uint16_t kSsrc2 = 4567;
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(kSsrc2, sequence_number, timestamp,
                                      retransmission))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc2, sequence_number,
                                               timestamp, retransmission));

  // No module is sending, hence no packet should be sent.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission));

  // Add a packet with incorrect ssrc and test it's dropped in the router.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, SSRC()).Times(1).WillOnce(Return(kSsrc1));
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_1, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1 + kSsrc2, sequence_number,
                                               timestamp, retransmission));

  packet_router_->RemoveRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending a packet on that ssrc and make sure
  // it is dropped as expected by not expecting any calls to rtp_1.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, SSRC()).Times(1).WillOnce(Return(kSsrc2));
  EXPECT_CALL(rtp_2, TimeToSendPacket(_, _, _, _)).Times(0);
  EXPECT_TRUE(packet_router_->TimeToSendPacket(kSsrc1, sequence_number,
                                               timestamp, retransmission));

  packet_router_->RemoveRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, TimeToSendPadding) {
  MockRtpRtcp rtp_1;
  MockRtpRtcp rtp_2;
  packet_router_->AddRtpModule(&rtp_1);
  packet_router_->AddRtpModule(&rtp_2);

  // Default configuration, sending padding on the first sending module.
  const size_t requested_padding_bytes = 1000;
  const size_t sent_padding_bytes = 890;
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(sent_padding_bytes,
            packet_router_->TimeToSendPadding(requested_padding_bytes));

  // Let only the second module be sending and verify the padding request is
  // routed there.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_))
      .Times(1)
      .WillOnce(Return(sent_padding_bytes));
  EXPECT_EQ(sent_padding_bytes,
            packet_router_->TimeToSendPadding(requested_padding_bytes));

  // No sending module at all.
  EXPECT_CALL(rtp_1, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_1, TimeToSendPadding(requested_padding_bytes)).Times(0);
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(false));
  EXPECT_CALL(rtp_2, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(static_cast<size_t>(0),
            packet_router_->TimeToSendPadding(requested_padding_bytes));

  packet_router_->RemoveRtpModule(&rtp_1);

  // rtp_1 has been removed, try sending padding and make sure rtp_1 isn't asked
  // to send by not expecting any calls. Instead verify rtp_2 is called.
  EXPECT_CALL(rtp_2, SendingMedia()).Times(1).WillOnce(Return(true));
  EXPECT_CALL(rtp_2, TimeToSendPadding(requested_padding_bytes)).Times(1);
  EXPECT_EQ(static_cast<size_t>(0),
            packet_router_->TimeToSendPadding(requested_padding_bytes));

  packet_router_->RemoveRtpModule(&rtp_2);
}
}  // namespace webrtc
