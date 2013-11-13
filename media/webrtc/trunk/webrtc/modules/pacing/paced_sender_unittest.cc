/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/pacing/include/paced_sender.h"

using testing::_;
using testing::Return;

namespace webrtc {
namespace test {

static const int kTargetBitrate = 800;
static const float kPaceMultiplier = 1.5f;

class MockPacedSenderCallback : public PacedSender::Callback {
 public:
  MOCK_METHOD3(TimeToSendPacket,
      bool(uint32_t ssrc, uint16_t sequence_number, int64_t capture_time_ms));
  MOCK_METHOD1(TimeToSendPadding,
      int(int bytes));
};

class PacedSenderPadding : public PacedSender::Callback {
 public:
  PacedSenderPadding() : padding_sent_(0) {}

  bool TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                        int64_t capture_time_ms) {
    return true;
  }

  int TimeToSendPadding(int bytes) {
    const int kPaddingPacketSize = 224;
    int num_packets = (bytes + kPaddingPacketSize - 1) / kPaddingPacketSize;
    padding_sent_ += kPaddingPacketSize * num_packets;
    return kPaddingPacketSize * num_packets;
  }

  int padding_sent() { return padding_sent_; }

 private:
  int padding_sent_;
};

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() {
    srand(0);
    TickTime::UseFakeClock(123456);
    // Need to initialize PacedSender after we initialize clock.
    send_bucket_.reset(new PacedSender(&callback_, kTargetBitrate,
                                       kPaceMultiplier));
    send_bucket_->SetStatus(true);
  }

  void SendAndExpectPacket(PacedSender::Priority priority,
                           uint32_t ssrc, uint16_t sequence_number,
                           int64_t capture_time_ms, int size) {
    EXPECT_FALSE(send_bucket_->SendPacket(priority, ssrc,
        sequence_number, capture_time_ms, size));
    EXPECT_CALL(callback_, TimeToSendPacket(
        ssrc, sequence_number, capture_time_ms))
        .Times(1)
        .WillRepeatedly(Return(true));
  }

  MockPacedSenderCallback callback_;
  scoped_ptr<PacedSender> send_bucket_;
};

TEST_F(PacedSenderTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number, capture_time_ms, 250));
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_CALL(callback_,
      TimeToSendPacket(ssrc, sequence_number, capture_time_ms)).Times(0);
  TickTime::AdvanceFakeClock(4);
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(1);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc, sequence_number++, capture_time_ms))
      .Times(1)
      .WillRepeatedly(Return(true));
  send_bucket_->Process();
  sequence_number++;
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  for (int i = 0; i < 3; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        capture_time_ms, 250);
  }
  for (int j = 0; j < 30; ++j) {
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, capture_time_ms, 250));
  }
  send_bucket_->Process();
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    TickTime::AdvanceFakeClock(5);
    EXPECT_CALL(callback_,
        TimeToSendPacket(ssrc, _, capture_time_ms))
        .Times(3)
        .WillRepeatedly(Return(true));
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number, capture_time_ms, 250));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, PaceQueuedPacketsWithDuplicates) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  uint16_t queued_sequence_number;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  for (int i = 0; i < 3; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        capture_time_ms, 250);
  }
  queued_sequence_number = sequence_number;

  for (int j = 0; j < 30; ++j) {
    // Send in duplicate packets.
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number, capture_time_ms, 250));
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, capture_time_ms, 250));
  }
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  send_bucket_->Process();
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    TickTime::AdvanceFakeClock(5);

    for (int i = 0; i < 3; ++i) {
      EXPECT_CALL(callback_, TimeToSendPacket(ssrc, queued_sequence_number++,
                                              capture_time_ms))
          .Times(1)
          .WillRepeatedly(Return(true));
   }
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, capture_time_ms, 250));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;

  send_bucket_->UpdateBitrate(kTargetBitrate, kTargetBitrate, kTargetBitrate);
  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  // No padding is expected since we have sent too much already.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  // 5 milliseconds later we have enough budget to send some padding.
  EXPECT_CALL(callback_, TimeToSendPadding(250)).Times(1).
      WillOnce(Return(250));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, NoPaddingWhenDisabled) {
  send_bucket_->SetStatus(false);
  send_bucket_->UpdateBitrate(kTargetBitrate, kTargetBitrate, kTargetBitrate);
  // No padding is expected since the pacer is disabled.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, VerifyPaddingUpToBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  send_bucket_->UpdateBitrate(kTargetBitrate, kTargetBitrate, kTargetBitrate);
  int64_t start_time = TickTime::MillisecondTimestamp();
  while (TickTime::MillisecondTimestamp() - start_time < kBitrateWindow) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        capture_time_ms, 250);
    TickTime::AdvanceFakeClock(kTimeStep);
    EXPECT_CALL(callback_, TimeToSendPadding(250)).Times(1).
        WillOnce(Return(250));
    send_bucket_->Process();
  }
}

TEST_F(PacedSenderTest, VerifyMaxPaddingBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  const int kTargetBitrate = 1500;
  const int kMaxPaddingBitrate = 800;
  send_bucket_->UpdateBitrate(kTargetBitrate, kMaxPaddingBitrate,
                              kTargetBitrate);
  int64_t start_time = TickTime::MillisecondTimestamp();
  while (TickTime::MillisecondTimestamp() - start_time < kBitrateWindow) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        capture_time_ms, 250);
    TickTime::AdvanceFakeClock(kTimeStep);
    EXPECT_CALL(callback_, TimeToSendPadding(500)).Times(1).
        WillOnce(Return(250));
    send_bucket_->Process();
  }
}

TEST_F(PacedSenderTest, VerifyAverageBitrateVaryingMediaPayload) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 10000;
  PacedSenderPadding callback;
  send_bucket_.reset(new PacedSender(&callback, kTargetBitrate,
                                     kPaceMultiplier));
  send_bucket_->SetStatus(true);
  send_bucket_->UpdateBitrate(kTargetBitrate, kTargetBitrate, kTargetBitrate);
  int64_t start_time = TickTime::MillisecondTimestamp();
  int media_bytes = 0;
  while (TickTime::MillisecondTimestamp() - start_time < kBitrateWindow) {
    int media_payload = rand() % 100 + 200;  // [200, 300] bytes.
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
                                          sequence_number++, capture_time_ms,
                                          media_payload));
    media_bytes += media_payload;
    TickTime::AdvanceFakeClock(kTimeStep);
    send_bucket_->Process();
  }
  EXPECT_NEAR(kTargetBitrate, 8 * (media_bytes + callback.padding_sent()) /
              kBitrateWindow, 1);
}

TEST_F(PacedSenderTest, Priority) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  int64_t capture_time_ms_low_priority = 1234567;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kLowPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  send_bucket_->Process();

  // Expect normal and low priority to be queued and high to pass through.
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, capture_time_ms_low_priority, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kHighPriority,
      ssrc, sequence_number++, capture_time_ms, 250));

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, capture_time_ms))
      .Times(3)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc_low_priority, _, capture_time_ms_low_priority))
      .Times(1)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = TickTime::MillisecondTimestamp();
  TickTime::AdvanceFakeClock(10000);
  int64_t second_capture_time_ms = TickTime::MillisecondTimestamp();

  EXPECT_EQ(0, send_bucket_->QueueInMs());

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kLowPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      capture_time_ms, 250);
  send_bucket_->Process();

  send_bucket_->Pause();

  // Expect everything to be queued.
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, second_capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kHighPriority,
      ssrc, sequence_number++, capture_time_ms, 250));

  EXPECT_EQ(TickTime::MillisecondTimestamp() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Expect no packet to come out while paused.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _)).Times(0);

  for (int i = 0; i < 10; ++i) {
    TickTime::AdvanceFakeClock(5);
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  // Expect high prio packets to come out first followed by all packets in the
  // way they were added.
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, capture_time_ms))
      .Times(3)
      .WillRepeatedly(Return(true));
  send_bucket_->Resume();

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_, TimeToSendPacket(_, _, second_capture_time_ms))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  TickTime::AdvanceFakeClock(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ResendPacket) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = TickTime::MillisecondTimestamp();
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
                                        ssrc,
                                        sequence_number,
                                        capture_time_ms,
                                        250));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
                                        ssrc,
                                        sequence_number + 1,
                                        capture_time_ms + 1,
                                        250));
  TickTime::AdvanceFakeClock(10000);
  EXPECT_EQ(TickTime::MillisecondTimestamp() - capture_time_ms,
            send_bucket_->QueueInMs());
  // Fails to send first packet so only one call.
  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc, sequence_number, capture_time_ms))
      .Times(1)
      .WillOnce(Return(false));
  TickTime::AdvanceFakeClock(10000);
  send_bucket_->Process();

  // Queue remains unchanged.
  EXPECT_EQ(TickTime::MillisecondTimestamp() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Fails to send second packet.
  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc, sequence_number, capture_time_ms))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc, sequence_number + 1, capture_time_ms + 1))
      .Times(1)
      .WillOnce(Return(false));
  TickTime::AdvanceFakeClock(10000);
  send_bucket_->Process();

  // Queue is reduced by 1 packet.
  EXPECT_EQ(TickTime::MillisecondTimestamp() - capture_time_ms - 1,
            send_bucket_->QueueInMs());

  // Send second packet and queue becomes empty.
  EXPECT_CALL(callback_, TimeToSendPacket(
      ssrc, sequence_number + 1, capture_time_ms + 1))
      .Times(1)
      .WillOnce(Return(true));
  TickTime::AdvanceFakeClock(10000);
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

}  // namespace test
}  // namespace webrtc
