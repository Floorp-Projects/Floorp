/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "webrtc/modules/pacing/include/paced_sender.h"

namespace {
  const int kTargetBitrate = 800;
};

namespace webrtc {

class MockPacedSenderCallback : public PacedSender::Callback {
 public:
  MOCK_METHOD3(TimeToSendPacket,
      void(uint32_t ssrc, uint16_t sequence_number, int64_t capture_time_ms));
  MOCK_METHOD1(TimeToSendPadding,
      void(int bytes));
};

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() {
    TickTime::UseFakeClock(123456);
    // Need to initialize PacedSender after we initialize clock.
    send_bucket_.reset(new PacedSender(&callback_, kTargetBitrate));
    send_bucket_->SetStatus(true);
  }
  MockPacedSenderCallback callback_;
  scoped_ptr<PacedSender> send_bucket_;
};

TEST_F(PacedSenderTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number, capture_time_ms, 250));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPadding(testing::_)).Times(0);
  EXPECT_CALL(callback_,
      TimeToSendPacket(ssrc, sequence_number, capture_time_ms)).Times(0);
  TickTime::AdvanceFakeClock(4);
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_,
      TimeToSendPacket(ssrc, sequence_number, capture_time_ms)).Times(1);
  TickTime::AdvanceFakeClock(1);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  sequence_number++;
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
}

TEST_F(PacedSenderTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, capture_time_ms, 250));
  }
  for (int j = 0; j < 30; ++j) {
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, capture_time_ms, 250));
  }
  EXPECT_CALL(callback_, TimeToSendPadding(testing::_)).Times(0);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    TickTime::AdvanceFakeClock(5);
    EXPECT_CALL(callback_,
        TimeToSendPacket(ssrc, testing::_, capture_time_ms)).Times(3);
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
}

TEST_F(PacedSenderTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  EXPECT_CALL(callback_, TimeToSendPadding(250)).Times(1);
  EXPECT_CALL(callback_,
      TimeToSendPacket(ssrc, sequence_number, capture_time_ms)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_, TimeToSendPadding(500)).Times(1);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, capture_time_ms_low_priority, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));

  // Expect normal and low priority to be queued and high to pass through.
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, capture_time_ms_low_priority, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_TRUE(send_bucket_->SendPacket(PacedSender::kHighPriority,
      ssrc, sequence_number++, capture_time_ms, 250));

  // Expect all normal priority to be sent out first.
  EXPECT_CALL(callback_, TimeToSendPadding(testing::_)).Times(0);
  EXPECT_CALL(callback_,
      TimeToSendPacket(ssrc, testing::_, capture_time_ms)).Times(2);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_, TimeToSendPacket(ssrc_low_priority,
      testing::_, capture_time_ms_low_priority)).Times(1);

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

}  // namespace webrtc
