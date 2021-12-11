/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <memory>

#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/nack_module.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
class TestNackModule : public ::testing::Test,
                       public NackSender,
                       public KeyFrameRequestSender {
 protected:
  TestNackModule()
      : clock_(new SimulatedClock(0)),
        nack_module_(clock_.get(), this, this),
        keyframes_requested_(0) {}

  void SendNack(const std::vector<uint16_t>& sequence_numbers) override {
    sent_nacks_.insert(sent_nacks_.end(), sequence_numbers.begin(),
                       sequence_numbers.end());
  }

  void RequestKeyFrame() override { ++keyframes_requested_; }

  std::unique_ptr<SimulatedClock> clock_;
  NackModule nack_module_;
  std::vector<uint16_t> sent_nacks_;
  int keyframes_requested_;
};

TEST_F(TestNackModule, NackOnePacket) {
  VCMPacket packet;
  packet.seqNum = 1;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 3;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);
}

TEST_F(TestNackModule, WrappingSeqNum) {
  VCMPacket packet;
  packet.seqNum = 0xfffe;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 1;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);
}

TEST_F(TestNackModule, WrappingSeqNumClearToKeyframe) {
  VCMPacket packet;
  packet.seqNum = 0xfffe;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 1;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);

  sent_nacks_.clear();
  packet.frameType = kVideoFrameKey;
  packet.is_first_packet_in_frame = true;
  packet.seqNum = 2;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(0u, sent_nacks_.size());

  packet.seqNum = 501;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(498u, sent_nacks_.size());
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);

  sent_nacks_.clear();
  packet.frameType = kVideoFrameDelta;
  packet.seqNum = 1001;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(499u, sent_nacks_.size());
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  EXPECT_EQ(999u, sent_nacks_.size());
  EXPECT_EQ(0xffff, sent_nacks_[0]);
  EXPECT_EQ(0, sent_nacks_[1]);
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 1]);
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 2]);

  // Adding packet 1004 will cause the nack list to reach it's max limit.
  // It will then clear all nacks up to the next keyframe (seq num 2),
  // thus removing 0xffff and 0 from the nack list.
  sent_nacks_.clear();
  packet.seqNum = 1004;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(2u, sent_nacks_.size());
  EXPECT_EQ(1002, sent_nacks_[0]);
  EXPECT_EQ(1003, sent_nacks_[1]);

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  EXPECT_EQ(999u, sent_nacks_.size());
  for (int seq_num = 3; seq_num < 501; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 3]);
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 4]);

  // Adding packet 1007 will cause the nack module to overflow again, thus
  // clearing everything up to 501 which is the next keyframe.
  packet.seqNum = 1007;
  nack_module_.OnReceivedPacket(packet);
  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  EXPECT_EQ(503u, sent_nacks_.size());
  for (int seq_num = 502; seq_num < 1001; ++seq_num)
    EXPECT_EQ(seq_num, sent_nacks_[seq_num - 502]);
  EXPECT_EQ(1005, sent_nacks_[501]);
  EXPECT_EQ(1006, sent_nacks_[502]);
}

TEST_F(TestNackModule, DontBurstOnTimeSkip) {
  nack_module_.Process();
  clock_->AdvanceTimeMilliseconds(20);
  EXPECT_EQ(0, nack_module_.TimeUntilNextProcess());
  nack_module_.Process();

  clock_->AdvanceTimeMilliseconds(100);
  EXPECT_EQ(0, nack_module_.TimeUntilNextProcess());
  nack_module_.Process();
  EXPECT_EQ(20, nack_module_.TimeUntilNextProcess());

  clock_->AdvanceTimeMilliseconds(19);
  EXPECT_EQ(1, nack_module_.TimeUntilNextProcess());
  clock_->AdvanceTimeMilliseconds(2);
  nack_module_.Process();
  EXPECT_EQ(19, nack_module_.TimeUntilNextProcess());

  clock_->AdvanceTimeMilliseconds(19);
  EXPECT_EQ(0, nack_module_.TimeUntilNextProcess());
  nack_module_.Process();

  clock_->AdvanceTimeMilliseconds(21);
  EXPECT_EQ(0, nack_module_.TimeUntilNextProcess());
  nack_module_.Process();
  EXPECT_EQ(19, nack_module_.TimeUntilNextProcess());
}

TEST_F(TestNackModule, ResendNack) {
  VCMPacket packet;
  packet.seqNum = 1;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 3;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);

  // Default RTT is 100
  clock_->AdvanceTimeMilliseconds(99);
  nack_module_.Process();
  EXPECT_EQ(1u, sent_nacks_.size());

  clock_->AdvanceTimeMilliseconds(1);
  nack_module_.Process();
  EXPECT_EQ(2u, sent_nacks_.size());

  nack_module_.UpdateRtt(50);
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  EXPECT_EQ(3u, sent_nacks_.size());

  clock_->AdvanceTimeMilliseconds(50);
  nack_module_.Process();
  EXPECT_EQ(4u, sent_nacks_.size());

  packet.seqNum = 2;
  nack_module_.OnReceivedPacket(packet);
  clock_->AdvanceTimeMilliseconds(50);
  nack_module_.Process();
  EXPECT_EQ(4u, sent_nacks_.size());
}

TEST_F(TestNackModule, ResendPacketMaxRetries) {
  VCMPacket packet;
  packet.seqNum = 1;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 3;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1u, sent_nacks_.size());
  EXPECT_EQ(2, sent_nacks_[0]);

  for (size_t retries = 1; retries < 10; ++retries) {
    clock_->AdvanceTimeMilliseconds(100);
    nack_module_.Process();
    EXPECT_EQ(retries + 1, sent_nacks_.size());
  }

  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  EXPECT_EQ(10u, sent_nacks_.size());
}

TEST_F(TestNackModule, TooLargeNackList) {
  VCMPacket packet;
  packet.seqNum = 0;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 1001;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  packet.seqNum = 1003;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
  packet.seqNum = 1004;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
}

TEST_F(TestNackModule, TooLargeNackListWithKeyFrame) {
  VCMPacket packet;
  packet.seqNum = 0;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 1;
  packet.is_first_packet_in_frame = true;
  packet.frameType = kVideoFrameKey;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 1001;
  packet.is_first_packet_in_frame = false;
  packet.frameType = kVideoFrameKey;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(999u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  packet.seqNum = 1003;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(0, keyframes_requested_);
  packet.seqNum = 1005;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(1000u, sent_nacks_.size());
  EXPECT_EQ(1, keyframes_requested_);
}

TEST_F(TestNackModule, ClearUpTo) {
  VCMPacket packet;
  packet.seqNum = 0;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 100;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(99u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.ClearUpTo(50);
  nack_module_.Process();
  EXPECT_EQ(50u, sent_nacks_.size());
  EXPECT_EQ(50, sent_nacks_[0]);
}

TEST_F(TestNackModule, ClearUpToWrap) {
  VCMPacket packet;
  packet.seqNum = 0xfff0;
  nack_module_.OnReceivedPacket(packet);
  packet.seqNum = 0xf;
  nack_module_.OnReceivedPacket(packet);
  EXPECT_EQ(30u, sent_nacks_.size());

  sent_nacks_.clear();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.ClearUpTo(0);
  nack_module_.Process();
  EXPECT_EQ(15u, sent_nacks_.size());
  EXPECT_EQ(0, sent_nacks_[0]);
}

TEST_F(TestNackModule, PacketNackCount) {
  VCMPacket packet;
  packet.seqNum = 0;
  EXPECT_EQ(0, nack_module_.OnReceivedPacket(packet));
  packet.seqNum = 2;
  EXPECT_EQ(0, nack_module_.OnReceivedPacket(packet));
  packet.seqNum = 1;
  EXPECT_EQ(1, nack_module_.OnReceivedPacket(packet));

  sent_nacks_.clear();
  nack_module_.UpdateRtt(100);
  packet.seqNum = 5;
  EXPECT_EQ(0, nack_module_.OnReceivedPacket(packet));
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  clock_->AdvanceTimeMilliseconds(100);
  nack_module_.Process();
  packet.seqNum = 3;
  EXPECT_EQ(3, nack_module_.OnReceivedPacket(packet));
  packet.seqNum = 4;
  EXPECT_EQ(3, nack_module_.OnReceivedPacket(packet));
  EXPECT_EQ(0, nack_module_.OnReceivedPacket(packet));
}

}  // namespace webrtc
