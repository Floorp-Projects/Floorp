/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "gtest/gtest.h"
#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/source/packet.h"
#include "modules/video_coding/main/source/session_info.h"

namespace webrtc {

class TestSessionInfo : public ::testing::Test {
 protected:
  enum { kPacketBufferSize = 10 };
  enum { kFrameBufferSize = 10 * kPacketBufferSize };

  virtual void SetUp() {
    memset(packet_buffer_, 0, kPacketBufferSize);
    memset(frame_buffer_, 0, kFrameBufferSize);
    session_.Reset();
    packet_.Reset();
    packet_.frameType = kVideoFrameDelta;
    packet_.sizeBytes = kPacketBufferSize;
    packet_.dataPtr = packet_buffer_;
    packet_.seqNum = 0;
    packet_.timestamp = 0;
  }

  void FillPacket(uint8_t start_value) {
    for (int i = 0; i < kPacketBufferSize; ++i)
      packet_buffer_[i] = start_value + i;
  }

  void VerifyPacket(uint8_t* start_ptr, uint8_t start_value) {
    for (int j = 0; j < kPacketBufferSize; ++j) {
      ASSERT_EQ(start_value + j, start_ptr[j]);
    }
  }

  uint8_t packet_buffer_[kPacketBufferSize];
  uint8_t frame_buffer_[kFrameBufferSize];
  VCMSessionInfo session_;
  VCMPacket packet_;
};

class TestVP8Partitions : public TestSessionInfo {
 protected:
  enum { kMaxVP8Partitions = 9 };

  virtual void SetUp() {
    TestSessionInfo::SetUp();
    vp8_header_ = &packet_header_.type.Video.codecHeader.VP8;
    packet_header_.frameType = kVideoFrameDelta;
    packet_header_.type.Video.codec = kRTPVideoVP8;
    vp8_header_->InitRTPVideoHeaderVP8();
    fragmentation_.VerifyAndAllocateFragmentationHeader(kMaxVP8Partitions);
  }

  bool VerifyPartition(int partition_id,
                       int packets_expected,
                       int start_value) {
    EXPECT_EQ(static_cast<uint32_t>(packets_expected * kPacketBufferSize),
              fragmentation_.fragmentationLength[partition_id]);
    for (int i = 0; i < packets_expected; ++i) {
      int packet_index = fragmentation_.fragmentationOffset[partition_id] +
          i * kPacketBufferSize;
      if (packet_index + kPacketBufferSize > kFrameBufferSize)
        return false;
      VerifyPacket(frame_buffer_ + packet_index, start_value + i);
    }
    return true;
  }

  WebRtcRTPHeader packet_header_;
  RTPVideoHeaderVP8* vp8_header_;
  RTPFragmentationHeader fragmentation_;
};

class TestNalUnits : public TestSessionInfo {
 protected:
  virtual void SetUp() {
    TestSessionInfo::SetUp();
    packet_.codec = kVideoCodecVP8;
  }

  bool VerifyNalu(int offset, int packets_expected, int start_value) {
    EXPECT_GE(session_.SessionLength(),
              packets_expected * kPacketBufferSize);
    for (int i = 0; i < packets_expected; ++i) {
      int packet_index = offset * kPacketBufferSize + i * kPacketBufferSize;
      VerifyPacket(frame_buffer_ + packet_index, start_value + i);
    }
    return true;
  }
};

class TestNackList : public TestSessionInfo {
 protected:
  enum { kMaxSeqNumListLength = 30 };

  virtual void SetUp() {
    TestSessionInfo::SetUp();
    seq_num_list_length_ = 0;
    memset(seq_num_list_, 0, sizeof(seq_num_list_));
  }

  void BuildSeqNumList(uint16_t low,
                       uint16_t high) {
    int i = 0;
    while (low != high + 1) {
      EXPECT_LT(i, kMaxSeqNumListLength);
      if (i >= kMaxSeqNumListLength) {
        seq_num_list_length_ = kMaxSeqNumListLength;
        return;
      }
      seq_num_list_[i] = low;
      low++;
      i++;
    }
    seq_num_list_length_ = i;
  }

  void VerifyAll(int value) {
    for (int i = 0; i < seq_num_list_length_; ++i)
      EXPECT_EQ(seq_num_list_[i], value);
  }

  int seq_num_list_[kMaxSeqNumListLength];
  int seq_num_list_length_;
};

TEST_F(TestSessionInfo, TestSimpleAPIs) {
  packet_.isFirstPacket = true;
  packet_.seqNum = 0xFFFE;
  packet_.sizeBytes = kPacketBufferSize;
  packet_.frameType = kVideoFrameKey;
  FillPacket(0);
  ASSERT_EQ(kPacketBufferSize,
            session_.InsertPacket(packet_, frame_buffer_, false, 0));
  EXPECT_FALSE(session_.HaveLastPacket());
  EXPECT_EQ(kVideoFrameKey, session_.FrameType());

  packet_.isFirstPacket = false;
  packet_.markerBit = true;
  packet_.seqNum += 1;
  ASSERT_EQ(kPacketBufferSize,
            session_.InsertPacket(packet_, frame_buffer_, false, 0));
  EXPECT_TRUE(session_.HaveLastPacket());
  EXPECT_EQ(packet_.seqNum, session_.HighSequenceNumber());
  EXPECT_EQ(0xFFFE, session_.LowSequenceNumber());

  // Insert empty packet which will be the new high sequence number.
  // To make things more difficult we will make sure to have a wrap here.
  packet_.isFirstPacket = false;
  packet_.markerBit = true;
  packet_.seqNum  = 2;
  packet_.sizeBytes = 0;
  packet_.frameType = kFrameEmpty;
  ASSERT_EQ(0,
            session_.InsertPacket(packet_, frame_buffer_, false, 0));
  EXPECT_EQ(packet_.seqNum, session_.HighSequenceNumber());
}

TEST_F(TestSessionInfo, NormalOperation) {
  packet_.seqNum = 0xFFFF;
  packet_.isFirstPacket = true;
  packet_.markerBit = false;
  FillPacket(0);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  for (int i = 1; i < 9; ++i) {
    packet_.seqNum += 1;
    FillPacket(i);
    ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
              kPacketBufferSize);
  }

  packet_.seqNum += 1;
  packet_.markerBit = true;
  FillPacket(9);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(0, session_.packets_not_decodable());
  EXPECT_EQ(10 * kPacketBufferSize, session_.SessionLength());
  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE("Calling VerifyPacket");
    VerifyPacket(frame_buffer_ + i * kPacketBufferSize, i);
  }
}

TEST_F(TestVP8Partitions, TwoPartitionsOneLoss) {
  // Partition 0 | Partition 1
  // [ 0 ] [ 2 ] | [ 3 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // One packet should be removed (end of partition 0).
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            2*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 1, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 3));
}

TEST_F(TestVP8Partitions, TwoPartitionsOneLoss2) {
  // Partition 0 | Partition 1
  // [ 1 ] [ 2 ] | [ 3 ] [ 5 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0)
            , kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // One packet should be removed (end of partition 2), 3 left.
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            3*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 3));
  EXPECT_EQ(1, session_.packets_not_decodable());
}

TEST_F(TestVP8Partitions, TwoPartitionsNoLossWrap) {
  // Partition 0       | Partition 1
  // [ fffd ] [ fffe ] | [ ffff ] [ 0 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0xfffd;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(1);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // No packet should be removed.
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 2, 2));
  EXPECT_EQ(0, session_.packets_not_decodable());
}

TEST_F(TestVP8Partitions, TwoPartitionsLossWrap) {
  // Partition 0       | Partition 1
  // [ fffd ] [ fffe ] | [ ffff ] [ 1 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0xfffd;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(1);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(3);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // One packet should be removed from the last partition
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            3*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 1, 2));
  EXPECT_EQ(1, session_.packets_not_decodable());
}


TEST_F(TestVP8Partitions, ThreePartitionsOneMissing) {
  // Partition 1  |Partition 2    | Partition 3
  // [ 1 ] [ 2 ]  |               | [ 5 ] | [ 6 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 3;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(6);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // No packet should be removed.
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(2, 2, 5));
  EXPECT_EQ(0, session_.packets_not_decodable());
}

TEST_F(TestVP8Partitions, ThreePartitionsLossInSecond) {
  // Partition 0  |Partition 1          | Partition 2
  // [ 1 ] [ 2 ]  |        [ 4 ] [ 5 ]  | [ 6 ] [ 7 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 1;
  FillPacket(1);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 0;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 2;
  FillPacket(4);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(5);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(6);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(7);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // 2 partitions left. 2 packets removed from second partition
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            4*kPacketBufferSize);
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 1));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(2, 2, 6));
  EXPECT_EQ(2, session_.packets_not_decodable());
}

TEST_F(TestVP8Partitions, AggregationOverTwoPackets) {
  // Partition 0   | Partition 1         | Partition 2
  // [ 0           |           ]  [ 1 ]  | [ 2 ]
  packet_header_.type.Video.isFirstPacket = true;
  vp8_header_->beginningOfPartition = true;
  vp8_header_->partitionId = 0;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber = 0;
  FillPacket(0);
  VCMPacket* packet = new VCMPacket(packet_buffer_, kPacketBufferSize,
                                    packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 1;
  vp8_header_->beginningOfPartition = false;
  packet_header_.header.markerBit = false;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(1);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  packet_header_.type.Video.isFirstPacket = false;
  vp8_header_->partitionId = 2;
  vp8_header_->beginningOfPartition = true;
  packet_header_.header.markerBit = true;
  packet_header_.header.sequenceNumber += 1;
  FillPacket(2);
  packet = new VCMPacket(packet_buffer_, kPacketBufferSize, packet_header_);
  ASSERT_EQ(session_.InsertPacket(*packet, frame_buffer_, false, 0),
            kPacketBufferSize);
  delete packet;

  // No packets removed.
  EXPECT_EQ(session_.BuildVP8FragmentationHeader(frame_buffer_,
                                                 kFrameBufferSize,
                                                 &fragmentation_),
            3*kPacketBufferSize);
  EXPECT_EQ(0, session_.packets_not_decodable());
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(0, 2, 0));
  // This partition is aggregated in partition 0
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(1, 0, 0));
  SCOPED_TRACE("Calling VerifyPartition");
  EXPECT_TRUE(VerifyPartition(2, 1, 2));
}

TEST_F(TestNalUnits, OnlyReceivedEmptyPacket) {
  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluComplete;
  packet_.frameType = kFrameEmpty;
  packet_.sizeBytes = 0;
  packet_.seqNum = 0;
  packet_.markerBit = false;
  ASSERT_EQ(0, session_.InsertPacket(packet_, frame_buffer_, false, 0));

  EXPECT_EQ(0, session_.MakeDecodable());
  EXPECT_EQ(0, session_.SessionLength());
  EXPECT_EQ(0, session_.packets_not_decodable());
}

TEST_F(TestNalUnits, OneIsolatedNaluLoss) {
  packet_.isFirstPacket = true;
  packet_.completeNALU = kNaluComplete;
  packet_.seqNum = 0;
  packet_.markerBit = false;
  FillPacket(0);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluComplete;
  packet_.seqNum += 2;
  packet_.markerBit = true;
  FillPacket(2);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(0, session_.MakeDecodable());
  EXPECT_EQ(2 * kPacketBufferSize, session_.SessionLength());
  EXPECT_EQ(0, session_.packets_not_decodable());
  SCOPED_TRACE("Calling VerifyNalu");
  EXPECT_TRUE(VerifyNalu(0, 1, 0));
  SCOPED_TRACE("Calling VerifyNalu");
  EXPECT_TRUE(VerifyNalu(1, 1, 2));
}

TEST_F(TestNalUnits, LossInMiddleOfNalu) {
  packet_.isFirstPacket = true;
  packet_.completeNALU = kNaluComplete;
  packet_.seqNum = 0;
  packet_.markerBit = false;
  FillPacket(0);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluEnd;
  packet_.seqNum += 2;
  packet_.markerBit = true;
  FillPacket(2);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(kPacketBufferSize, session_.MakeDecodable());
  EXPECT_EQ(kPacketBufferSize, session_.SessionLength());
  EXPECT_EQ(1, session_.packets_not_decodable());
  SCOPED_TRACE("Calling VerifyNalu");
  EXPECT_TRUE(VerifyNalu(0, 1, 0));
}

TEST_F(TestNalUnits, StartAndEndOfLastNalUnitLost) {
  packet_.isFirstPacket = true;
  packet_.completeNALU = kNaluComplete;
  packet_.seqNum = 0;
  packet_.markerBit = false;
  FillPacket(0);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluIncomplete;
  packet_.seqNum += 2;
  packet_.markerBit = false;
  FillPacket(1);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(kPacketBufferSize, session_.MakeDecodable());
  EXPECT_EQ(kPacketBufferSize, session_.SessionLength());
  EXPECT_EQ(1, session_.packets_not_decodable());
  SCOPED_TRACE("Calling VerifyNalu");
  EXPECT_TRUE(VerifyNalu(0, 1, 0));
}

TEST_F(TestNalUnits, ReorderWrapNoLoss) {
  packet_.seqNum = 0xFFFF;
  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluIncomplete;
  packet_.seqNum += 1;
  packet_.markerBit = false;
  FillPacket(1);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = true;
  packet_.completeNALU = kNaluComplete;
  packet_.seqNum -= 1;
  packet_.markerBit = false;
  FillPacket(0);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluEnd;
  packet_.seqNum += 2;
  packet_.markerBit = true;
  FillPacket(2);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(0, session_.MakeDecodable());
  EXPECT_EQ(0, session_.packets_not_decodable());
  EXPECT_EQ(3*kPacketBufferSize, session_.SessionLength());
  SCOPED_TRACE("Calling VerifyNalu");
  EXPECT_TRUE(VerifyNalu(0, 1, 0));
}

TEST_F(TestNalUnits, WrapLosses) {
  packet_.seqNum = 0xFFFF;
  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluIncomplete;
  packet_.markerBit = false;
  FillPacket(1);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluEnd;
  packet_.seqNum += 2;
  packet_.markerBit = true;
  FillPacket(2);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(2 * kPacketBufferSize, session_.MakeDecodable());
  EXPECT_EQ(0, session_.SessionLength());
  EXPECT_EQ(2, session_.packets_not_decodable());
}

TEST_F(TestNalUnits, ReorderWrapLosses) {
  packet_.seqNum = 0xFFFF;

  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluEnd;
  packet_.seqNum += 2;
  packet_.markerBit = true;
  FillPacket(2);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  packet_.seqNum -= 2;
  packet_.isFirstPacket = false;
  packet_.completeNALU = kNaluIncomplete;
  packet_.markerBit = false;
  FillPacket(1);
  ASSERT_EQ(session_.InsertPacket(packet_, frame_buffer_, false, 0),
            kPacketBufferSize);

  EXPECT_EQ(2 * kPacketBufferSize, session_.MakeDecodable());
  EXPECT_EQ(0, session_.SessionLength());
  EXPECT_EQ(2, session_.packets_not_decodable());
}
}  // namespace webrtc
