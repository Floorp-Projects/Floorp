/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/system_wrappers/interface/clock.h"

using testing::_;
using testing::Return;

namespace webrtc {
namespace test {

static const int kTargetBitrate = 800;
static const float kPaceMultiplier = 1.5f;

class MockPacedSenderCallback : public PacedSender::Callback {
 public:
  MOCK_METHOD4(TimeToSendPacket,
               bool(uint32_t ssrc,
                    uint16_t sequence_number,
                    int64_t capture_time_ms,
                    bool retransmission));
  MOCK_METHOD1(TimeToSendPadding,
      int(int bytes));
};

class PacedSenderPadding : public PacedSender::Callback {
 public:
  PacedSenderPadding() : padding_sent_(0) {}

  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission) {
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

class PacedSenderProbing : public PacedSender::Callback {
 public:
  PacedSenderProbing(const std::list<int>& expected_deltas, Clock* clock)
      : prev_packet_time_ms_(-1),
        expected_deltas_(expected_deltas),
        packets_sent_(0),
        clock_(clock) {}

  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_time_ms,
                        bool retransmission) {
    ++packets_sent_;
    EXPECT_FALSE(expected_deltas_.empty());
    if (expected_deltas_.empty())
      return false;
    int64_t now_ms = clock_->TimeInMilliseconds();
    if (prev_packet_time_ms_ >= 0) {
      EXPECT_EQ(expected_deltas_.front(), now_ms - prev_packet_time_ms_);
      expected_deltas_.pop_front();
    }
    prev_packet_time_ms_ = now_ms;
    return true;
  }

  int TimeToSendPadding(int bytes) {
    EXPECT_TRUE(false);
    return bytes;
  }

  int packets_sent() const { return packets_sent_; }

 private:
  int64_t prev_packet_time_ms_;
  std::list<int> expected_deltas_;
  int packets_sent_;
  Clock* clock_;
};

class PacedSenderTest : public ::testing::Test {
 protected:
  PacedSenderTest() : clock_(123456) {
    srand(0);
    // Need to initialize PacedSender after we initialize clock.
    send_bucket_.reset(new PacedSender(&clock_,
                                       &callback_,
                                       kTargetBitrate,
                                       kPaceMultiplier * kTargetBitrate,
                                       0));
  }

  void SendAndExpectPacket(PacedSender::Priority priority,
                           uint32_t ssrc,
                           uint16_t sequence_number,
                           int64_t capture_time_ms,
                           int size,
                           bool retransmission) {
    EXPECT_FALSE(send_bucket_->SendPacket(priority, ssrc,
        sequence_number, capture_time_ms, size, retransmission));
    EXPECT_CALL(callback_,
                TimeToSendPacket(ssrc, sequence_number, capture_time_ms, false))
        .Times(1)
        .WillRepeatedly(Return(true));
  }

  SimulatedClock clock_;
  MockPacedSenderCallback callback_;
  scoped_ptr<PacedSender> send_bucket_;
};

TEST_F(PacedSenderTest, QueuePacket) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  int64_t queued_packet_timestamp = clock_.TimeInMilliseconds();
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number, queued_packet_timestamp, 250, false));
  send_bucket_->Process();
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  clock_.AdvanceTimeMilliseconds(4);
  EXPECT_EQ(1, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(1);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_CALL(
      callback_,
      TimeToSendPacket(ssrc, sequence_number++, queued_packet_timestamp, false))
      .Times(1)
      .WillRepeatedly(Return(true));
  send_bucket_->Process();
  sequence_number++;
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, clock_.TimeInMilliseconds(), 250, false));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, PaceQueuedPackets) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  for (int i = 0; i < 3; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority,
                        ssrc,
                        sequence_number++,
                        clock_.TimeInMilliseconds(),
                        250,
                        false);
  }
  for (int j = 0; j < 30; ++j) {
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, clock_.TimeInMilliseconds(), 250, false));
  }
  send_bucket_->Process();
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, _, false))
        .Times(3)
        .WillRepeatedly(Return(true));
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number, clock_.TimeInMilliseconds(), 250, false));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, PaceQueuedPacketsWithDuplicates) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  uint16_t queued_sequence_number;

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  for (int i = 0; i < 3; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority,
                        ssrc,
                        sequence_number++,
                        clock_.TimeInMilliseconds(),
                        250,
                        false);
  }
  queued_sequence_number = sequence_number;

  for (int j = 0; j < 30; ++j) {
    // Send in duplicate packets.
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number, clock_.TimeInMilliseconds(), 250, false));
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
        sequence_number++, clock_.TimeInMilliseconds(), 250, false));
  }
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  send_bucket_->Process();
  for (int k = 0; k < 10; ++k) {
    EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
    clock_.AdvanceTimeMilliseconds(5);

    for (int i = 0; i < 3; ++i) {
      EXPECT_CALL(callback_,
                  TimeToSendPacket(ssrc, queued_sequence_number++, _, false))
          .Times(1)
          .WillRepeatedly(Return(true));
   }
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
      sequence_number++, clock_.TimeInMilliseconds(), 250, false));
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, CanQueuePacketsWithSameSequenceNumberOnDifferentSsrcs) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);

  // Expect packet on second ssrc to be queued and sent as well.
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc + 1,
                      sequence_number,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);

  clock_.AdvanceTimeMilliseconds(1000);
  send_bucket_->Process();
}

TEST_F(PacedSenderTest, Padding) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;

  send_bucket_->UpdateBitrate(
      kTargetBitrate, kPaceMultiplier * kTargetBitrate, kTargetBitrate);
  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      clock_.TimeInMilliseconds(),
                      250,
                      false);
  // No padding is expected since we have sent too much already.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  // 5 milliseconds later we have enough budget to send some padding.
  EXPECT_CALL(callback_, TimeToSendPadding(250)).Times(1).
      WillOnce(Return(250));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, NoPaddingWhenDisabled) {
  send_bucket_->SetStatus(false);
  send_bucket_->UpdateBitrate(
      kTargetBitrate, kPaceMultiplier * kTargetBitrate, kTargetBitrate);
  // No padding is expected since the pacer is disabled.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, VerifyPaddingUpToBitrate) {
  uint32_t ssrc = 12345;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = 56789;
  const int kTimeStep = 5;
  const int64_t kBitrateWindow = 100;
  send_bucket_->UpdateBitrate(
      kTargetBitrate, kPaceMultiplier * kTargetBitrate, kTargetBitrate);
  int64_t start_time = clock_.TimeInMilliseconds();
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    SendAndExpectPacket(PacedSender::kNormalPriority,
                        ssrc,
                        sequence_number++,
                        capture_time_ms,
                        250,
                        false);
    clock_.AdvanceTimeMilliseconds(kTimeStep);
    EXPECT_CALL(callback_, TimeToSendPadding(250)).Times(1).
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
  send_bucket_.reset(new PacedSender(
      &clock_, &callback, kTargetBitrate, kPaceMultiplier * kTargetBitrate, 0));
  send_bucket_->UpdateBitrate(
      kTargetBitrate, kPaceMultiplier * kTargetBitrate, kTargetBitrate);
  int64_t start_time = clock_.TimeInMilliseconds();
  int media_bytes = 0;
  while (clock_.TimeInMilliseconds() - start_time < kBitrateWindow) {
    int media_payload = rand() % 100 + 200;  // [200, 300] bytes.
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority, ssrc,
                                          sequence_number++, capture_time_ms,
                                          media_payload, false));
    media_bytes += media_payload;
    clock_.AdvanceTimeMilliseconds(kTimeStep);
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
  SendAndExpectPacket(PacedSender::kLowPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  send_bucket_->Process();

  // Expect normal and low priority to be queued and high to pass through.
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, capture_time_ms_low_priority, 250,
      false));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kHighPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));

  // Expect all high and normal priority to be sent out first.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(ssrc, _, capture_time_ms, false))
      .Times(3)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_,
              TimeToSendPacket(
                  ssrc_low_priority, _, capture_time_ms_low_priority, false))
      .Times(1)
      .WillRepeatedly(Return(true));

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
}

TEST_F(PacedSenderTest, Pause) {
  uint32_t ssrc_low_priority = 12345;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();

  EXPECT_EQ(0, send_bucket_->QueueInMs());

  // Due to the multiplicative factor we can send 3 packets not 2 packets.
  SendAndExpectPacket(PacedSender::kLowPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number++,
                      capture_time_ms,
                      250,
                      false);
  send_bucket_->Process();

  send_bucket_->Pause();

  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kHighPriority,
      ssrc, sequence_number++, capture_time_ms, 250, false));

  clock_.AdvanceTimeMilliseconds(10000);
  int64_t second_capture_time_ms = clock_.TimeInMilliseconds();

  // Expect everything to be queued.
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kLowPriority,
      ssrc_low_priority, sequence_number++, second_capture_time_ms, 250,
      false));

  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Expect no packet to come out while paused.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, _, _)).Times(0);

  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
    EXPECT_EQ(0, send_bucket_->Process());
  }
  // Expect high prio packets to come out first followed by all packets in the
  // way they were added.
  EXPECT_CALL(callback_, TimeToSendPacket(_, _, capture_time_ms, false))
      .Times(3)
      .WillRepeatedly(Return(true));
  send_bucket_->Resume();

  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());

  EXPECT_CALL(callback_, TimeToSendPacket(_, _, second_capture_time_ms, false))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_EQ(5, send_bucket_->TimeUntilNextProcess());
  clock_.AdvanceTimeMilliseconds(5);
  EXPECT_EQ(0, send_bucket_->TimeUntilNextProcess());
  EXPECT_EQ(0, send_bucket_->Process());
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ResendPacket) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  int64_t capture_time_ms = clock_.TimeInMilliseconds();
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
                                        ssrc,
                                        sequence_number,
                                        capture_time_ms,
                                        250,
                                        false));
  clock_.AdvanceTimeMilliseconds(1);
  EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
                                        ssrc,
                                        sequence_number + 1,
                                        capture_time_ms + 1,
                                        250,
                                        false));
  clock_.AdvanceTimeMilliseconds(9999);
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());
  // Fails to send first packet so only one call.
  EXPECT_CALL(callback_,
              TimeToSendPacket(ssrc, sequence_number, capture_time_ms, false))
      .Times(1)
      .WillOnce(Return(false));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue remains unchanged.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms,
            send_bucket_->QueueInMs());

  // Fails to send second packet.
  EXPECT_CALL(callback_,
              TimeToSendPacket(ssrc, sequence_number, capture_time_ms, false))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(
      callback_,
      TimeToSendPacket(ssrc, sequence_number + 1, capture_time_ms + 1, false))
      .Times(1)
      .WillOnce(Return(false));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();

  // Queue is reduced by 1 packet.
  EXPECT_EQ(clock_.TimeInMilliseconds() - capture_time_ms - 1,
            send_bucket_->QueueInMs());

  // Send second packet and queue becomes empty.
  EXPECT_CALL(
      callback_,
      TimeToSendPacket(ssrc, sequence_number + 1, capture_time_ms + 1, false))
      .Times(1)
      .WillOnce(Return(true));
  clock_.AdvanceTimeMilliseconds(10000);
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

TEST_F(PacedSenderTest, ExpectedQueueTimeMs) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const int32_t kNumPackets = 60;
  const int32_t kPacketSize = 1200;
  const int32_t kMaxBitrate = kPaceMultiplier * 30;
  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  send_bucket_->UpdateBitrate(30, kMaxBitrate, 0);
  for (int i = 0; i < kNumPackets; ++i) {
    SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                        clock_.TimeInMilliseconds(), kPacketSize, false);
  }

  // Queue in ms = 1000 * (bytes in queue) / (kbit per second * 1000 / 8)
  int32_t queue_in_ms = kNumPackets * kPacketSize * 8 / kMaxBitrate;
  EXPECT_EQ(queue_in_ms, send_bucket_->ExpectedQueueTimeMs());

  int64_t time_start = clock_.TimeInMilliseconds();
  while (send_bucket_->QueueSizePackets() > 0) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    if (time_until_process <= 0) {
      send_bucket_->Process();
    } else {
      clock_.AdvanceTimeMilliseconds(time_until_process);
    }
  }
  int64_t duration = clock_.TimeInMilliseconds() - time_start;

  EXPECT_EQ(0, send_bucket_->ExpectedQueueTimeMs());

  // Allow for aliasing, duration should be in [expected(n - 1), expected(n)].
  EXPECT_LE(duration, queue_in_ms);
  EXPECT_GE(duration, queue_in_ms - (kPacketSize * 8 / kMaxBitrate));
}

TEST_F(PacedSenderTest, QueueTimeGrowsOverTime) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  EXPECT_EQ(0, send_bucket_->QueueInMs());

  send_bucket_->UpdateBitrate(30, kPaceMultiplier * 30, 0);
  SendAndExpectPacket(PacedSender::kNormalPriority,
                      ssrc,
                      sequence_number,
                      clock_.TimeInMilliseconds(),
                      1200,
                      false);

  clock_.AdvanceTimeMilliseconds(500);
  EXPECT_EQ(500, send_bucket_->QueueInMs());
  send_bucket_->Process();
  EXPECT_EQ(0, send_bucket_->QueueInMs());
}

class ProbingPacedSender : public PacedSender {
 public:
  ProbingPacedSender(Clock* clock,
                     Callback* callback,
                     int bitrate_kbps,
                     int max_bitrate_kbps,
                     int min_bitrate_kbps)
      : PacedSender(clock,
                    callback,
                    bitrate_kbps,
                    max_bitrate_kbps,
                    min_bitrate_kbps) {}

  virtual bool ProbingExperimentIsEnabled() const OVERRIDE { return true; }
};

TEST_F(PacedSenderTest, ProbingWithInitialFrame) {
  const int kNumPackets = 11;
  const int kNumDeltas = kNumPackets - 1;
  const int kPacketSize = 1200;
  const int kInitialBitrateKbps = 300;
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const int expected_deltas[kNumDeltas] = {
      10, 10, 10, 10, 10, 5, 5, 5, 5, 5};
  std::list<int> expected_deltas_list(expected_deltas,
                                      expected_deltas + kNumPackets - 1);
  PacedSenderProbing callback(expected_deltas_list, &clock_);
  send_bucket_.reset(
      new ProbingPacedSender(&clock_,
                             &callback,
                             kInitialBitrateKbps,
                             kPaceMultiplier * kInitialBitrateKbps,
                             0));
  for (int i = 0; i < kNumPackets; ++i) {
    EXPECT_FALSE(send_bucket_->SendPacket(PacedSender::kNormalPriority,
                                          ssrc,
                                          sequence_number++,
                                          clock_.TimeInMilliseconds(),
                                          kPacketSize,
                                          false));
  }
  while (callback.packets_sent() < kNumPackets) {
    int time_until_process = send_bucket_->TimeUntilNextProcess();
    if (time_until_process <= 0) {
      send_bucket_->Process();
    } else {
      clock_.AdvanceTimeMilliseconds(time_until_process);
    }
  }
}

TEST_F(PacedSenderTest, PriorityInversion) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const int32_t kPacketSize = 1200;

  EXPECT_FALSE(send_bucket_->SendPacket(
      PacedSender::kHighPriority, ssrc, sequence_number + 3,
      clock_.TimeInMilliseconds() + 33, kPacketSize, true));

  EXPECT_FALSE(send_bucket_->SendPacket(
      PacedSender::kHighPriority, ssrc, sequence_number + 2,
      clock_.TimeInMilliseconds() + 33, kPacketSize, true));

  EXPECT_FALSE(send_bucket_->SendPacket(
      PacedSender::kHighPriority, ssrc, sequence_number,
      clock_.TimeInMilliseconds(), kPacketSize, true));

  EXPECT_FALSE(send_bucket_->SendPacket(
      PacedSender::kHighPriority, ssrc, sequence_number + 1,
      clock_.TimeInMilliseconds(), kPacketSize, true));

  // Packets from earlier frames should be sent first.
  {
    ::testing::InSequence sequence;
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number,
                                            clock_.TimeInMilliseconds(), true))
        .WillOnce(Return(true));
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 1,
                                            clock_.TimeInMilliseconds(), true))
        .WillOnce(Return(true));
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 3,
                                            clock_.TimeInMilliseconds() + 33,
                                            true)).WillOnce(Return(true));
    EXPECT_CALL(callback_, TimeToSendPacket(ssrc, sequence_number + 2,
                                            clock_.TimeInMilliseconds() + 33,
                                            true)).WillOnce(Return(true));

    while (send_bucket_->QueueSizePackets() > 0) {
      int time_until_process = send_bucket_->TimeUntilNextProcess();
      if (time_until_process <= 0) {
        send_bucket_->Process();
      } else {
        clock_.AdvanceTimeMilliseconds(time_until_process);
      }
    }
  }
}

TEST_F(PacedSenderTest, PaddingOveruse) {
  uint32_t ssrc = 12346;
  uint16_t sequence_number = 1234;
  const int32_t kPacketSize = 1200;

  // Min bitrate 0 => no padding, padding budget will stay at 0.
  send_bucket_->UpdateBitrate(60, 90, 0);
  SendAndExpectPacket(PacedSender::kNormalPriority, ssrc, sequence_number++,
                      clock_.TimeInMilliseconds(), kPacketSize, false);
  send_bucket_->Process();

  // Add 30kbit padding. When increasing budget, media budget will increase from
  // negative (overuse) while padding budget will increase form 0.
  clock_.AdvanceTimeMilliseconds(5);
  send_bucket_->UpdateBitrate(60, 90, 30);

  EXPECT_FALSE(send_bucket_->SendPacket(
      PacedSender::kHighPriority, ssrc, sequence_number++,
      clock_.TimeInMilliseconds(), kPacketSize, false));

  // Don't send padding if queue is non-empty, even if padding budget > 0.
  EXPECT_CALL(callback_, TimeToSendPadding(_)).Times(0);
  send_bucket_->Process();
}

}  // namespace test
}  // namespace webrtc
