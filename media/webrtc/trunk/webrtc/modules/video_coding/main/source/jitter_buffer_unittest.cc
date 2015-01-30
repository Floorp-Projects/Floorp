/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <list>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"
#include "webrtc/modules/video_coding/main/source/media_opt_util.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/test/stream_generator.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

class TestBasicJitterBuffer : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    jitter_buffer_.reset(
        new VCMJitterBuffer(clock_.get(), &event_factory_));
    jitter_buffer_->Start();
    seq_num_ = 1234;
    timestamp_ = 0;
    size_ = 1400;
    // Data vector -  0, 0, 0x80, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0x80, 3....
    data_[0] = 0;
    data_[1] = 0;
    data_[2] = 0x80;
    int count = 3;
    for (unsigned int i = 3; i < sizeof(data_) - 3; ++i) {
      data_[i] = count;
      count++;
      if (count == 10) {
        data_[i + 1] = 0;
        data_[i + 2] = 0;
        data_[i + 3] = 0x80;
        count = 3;
        i += 3;
      }
    }
    packet_.reset(new VCMPacket(data_, size_, seq_num_, timestamp_, true));
  }

  VCMEncodedFrame* DecodeCompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextCompleteTimestamp(10, &timestamp);
    if (!found_frame)
      return NULL;
    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    return frame;
  }

  VCMEncodedFrame* DecodeIncompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp);
    if (!found_frame)
      return NULL;
    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    return frame;
  }

  void CheckOutFrame(VCMEncodedFrame* frame_out,
                    unsigned int size,
                    bool startCode) {
    ASSERT_TRUE(frame_out);

    const uint8_t* outData = frame_out->Buffer();
    unsigned int i = 0;

    if (startCode) {
      EXPECT_EQ(0, outData[0]);
      EXPECT_EQ(0, outData[1]);
      EXPECT_EQ(0, outData[2]);
      EXPECT_EQ(1, outData[3]);
      i += 4;
    }

    EXPECT_EQ(size, frame_out->Length());
    int count = 3;
    for (; i < size; i++) {
      if (outData[i] == 0 && outData[i + 1] == 0 && outData[i + 2] == 0x80) {
        i += 2;
      } else if (startCode && outData[i] == 0 && outData[i + 1] == 0) {
        EXPECT_EQ(0, outData[0]);
        EXPECT_EQ(0, outData[1]);
        EXPECT_EQ(0, outData[2]);
        EXPECT_EQ(1, outData[3]);
        i += 3;
      } else {
        EXPECT_EQ(count, outData[i]);
        count++;
        if (count == 10) {
          count = 3;
        }
      }
    }
  }

  uint16_t seq_num_;
  uint32_t timestamp_;
  int size_;
  uint8_t data_[1500];
  scoped_ptr<VCMJitterBuffer> jitter_buffer_;
  scoped_ptr<VCMPacket> packet_;
  scoped_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
};


class TestRunningJitterBuffer : public ::testing::Test {
 protected:
  enum { kDataBufferSize = 10 };

  virtual void SetUp() {
    clock_.reset(new SimulatedClock(0));
    max_nack_list_size_ = 150;
    oldest_packet_to_nack_ = 250;
    jitter_buffer_ = new VCMJitterBuffer(clock_.get(), &event_factory_);
    stream_generator_ = new StreamGenerator(0, 0, clock_->TimeInMilliseconds());
    jitter_buffer_->Start();
    jitter_buffer_->SetNackSettings(max_nack_list_size_,
                                    oldest_packet_to_nack_, 0);
    memset(data_buffer_, 0, kDataBufferSize);
  }

  virtual void TearDown() {
    jitter_buffer_->Stop();
    delete stream_generator_;
    delete jitter_buffer_;
  }

  VCMFrameBufferEnum InsertPacketAndPop(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->PopPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    bool retransmitted = false;
    return jitter_buffer_->InsertPacket(packet, &retransmitted);
  }

  VCMFrameBufferEnum InsertPacket(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->GetPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    bool retransmitted = false;
    return jitter_buffer_->InsertPacket(packet, &retransmitted);
  }

  VCMFrameBufferEnum InsertFrame(FrameType frame_type) {
    stream_generator_->GenerateFrame(frame_type,
                                    (frame_type != kFrameEmpty) ? 1 : 0,
                                    (frame_type == kFrameEmpty) ? 1 : 0,
                                    clock_->TimeInMilliseconds());
    VCMFrameBufferEnum ret = InsertPacketAndPop(0);
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
    return ret;
  }

  VCMFrameBufferEnum InsertFrames(int num_frames, FrameType frame_type) {
    VCMFrameBufferEnum ret_for_all = kNoError;
    for (int i = 0; i < num_frames; ++i) {
      VCMFrameBufferEnum ret = InsertFrame(frame_type);
      if (ret < kNoError) {
        ret_for_all = ret;
      } else if (ret_for_all >= kNoError) {
        ret_for_all = ret;
      }
    }
    return ret_for_all;
  }

  void DropFrame(int num_packets) {
    stream_generator_->GenerateFrame(kVideoFrameDelta, num_packets, 0,
                                     clock_->TimeInMilliseconds());
    for (int i = 0; i < num_packets; ++i)
      stream_generator_->DropLastPacket();
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }

  bool DecodeCompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextCompleteTimestamp(0, &timestamp);
    if (!found_frame)
      return false;

    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  bool DecodeIncompleteFrame() {
    uint32_t timestamp = 0;
    bool found_frame = jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp);
    if (!found_frame)
      return false;
    VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(timestamp);
    bool ret = (frame != NULL);
    jitter_buffer_->ReleaseFrame(frame);
    return ret;
  }

  VCMJitterBuffer* jitter_buffer_;
  StreamGenerator* stream_generator_;
  scoped_ptr<SimulatedClock> clock_;
  NullEventFactory event_factory_;
  size_t max_nack_list_size_;
  int oldest_packet_to_nack_;
  uint8_t data_buffer_[kDataBufferSize];
};

class TestJitterBufferNack : public TestRunningJitterBuffer {
 protected:
  virtual void SetUp() {
    TestRunningJitterBuffer::SetUp();
    jitter_buffer_->SetNackMode(kNack, -1, -1);
  }

  virtual void TearDown() {
    TestRunningJitterBuffer::TearDown();
  }
};

TEST_F(TestBasicJitterBuffer, StopRunning) {
  jitter_buffer_->Stop();
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());
  jitter_buffer_->Start();
  // Allow selective errors.
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);

  // No packets inserted.
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());

  // Allow decoding with errors.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);

  // No packets inserted.
  EXPECT_TRUE(NULL == DecodeCompleteFrame());
  EXPECT_TRUE(NULL == DecodeIncompleteFrame());
}

TEST_F(TestBasicJitterBuffer, SinglePacketFrame) {
  // Always start with a complete key frame when not allowing errors.
  jitter_buffer_->SetDecodeErrorMode(kNoErrors);
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->timestamp += 123 * 90;

  // Insert the packet to the jitter buffer and get a frame.
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  VCMEncodedFrame* frame_out =  DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, DualPacketFrame) {
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  // Should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  ++seq_num_;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, 100PacketKeyFrame) {
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 frames.
  int loop = 0;
  do {
    seq_num_++;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                        &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert last packet.
  ++seq_num_;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, 100PacketDeltaFrame) {
  // Always start with a complete key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_FALSE(frame_out == NULL);

  ++seq_num_;
  packet_->seqNum = seq_num_;
  packet_->markerBit = false;
  packet_->frameType = kVideoFrameDelta;
  packet_->timestamp += 33 * 90;

  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  packet_->isFirstPacket = false;
  // Insert 98 frames.
  int loop = 0;
  do {
    ++seq_num_;
    packet_->seqNum = seq_num_;

    // Insert a packet into a frame.
    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                        &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert the last packet.
  ++seq_num_;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, PacketReorderingReverseOrder) {
  // Insert the "first" packet last.
  seq_num_ += 100;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 packets.
  int loop = 0;
  do {
    seq_num_--;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                        &retransmitted));
    loop++;
  } while (loop < 98);

  // Insert the last packet.
  seq_num_--;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();;

  CheckOutFrame(frame_out, 100 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, FrameReordering2Frames2PacketsEach) {
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  // check that we fail to get frame since seqnum is not continuous
  frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  seq_num_ -= 3;
  timestamp_ -= 33*90;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  frame_out = DecodeCompleteFrame();

  // It should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  jitter_buffer_->ReleaseFrame(frame_out);

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, DuplicatePackets) {
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(0, jitter_buffer_->num_packets());
  EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);
  EXPECT_EQ(1, jitter_buffer_->num_packets());
  EXPECT_EQ(0, jitter_buffer_->num_duplicated_packets());

  packet_->isFirstPacket = false;
  packet_->markerBit = true;

  // Insert a packet into a frame.
  EXPECT_EQ(kDuplicatePacket, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  EXPECT_EQ(2, jitter_buffer_->num_packets());
  EXPECT_EQ(1, jitter_buffer_->num_duplicated_packets());

  seq_num_++;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
  EXPECT_EQ(3, jitter_buffer_->num_packets());
  EXPECT_EQ(1, jitter_buffer_->num_duplicated_packets());
}

TEST_F(TestBasicJitterBuffer, H264InsertStartCode) {
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->insertStartCode = true;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Frame should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_ * 2 + 4 * 2, true);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

// Test threshold conditions of decodable state.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsThresholdCheck) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame. Use 10 packets to test Decodable State
  // boundaries.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  uint32_t timestamp = 0;
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  packet_->isFirstPacket = false;
  for (int i = 1; i < 9; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
        &retransmitted));
    EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // last packet
  packet_->markerBit = true;
  packet_->seqNum++;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 10 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33 * 90 * 8;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame
  packet_->seqNum -= 99;
  packet_->timestamp -= 33 * 90 * 7;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  packet_->isFirstPacket = false;
  for (int i = 1; i < 8; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                              &retransmitted));
    EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
    EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  packet_->seqNum++;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  frame_out = DecodeIncompleteFrame();
  ASSERT_FALSE(NULL == frame_out);
  CheckOutFrame(frame_out, 9 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());

  packet_->markerBit = true;
  packet_->seqNum++;
  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));
}

// Make sure first packet is present before a frame can be decoded.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsIncompleteKey) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = false;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33*90*8;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  uint32_t timestamp;
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame - an incomplete key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->seqNum -= 99;
  packet_->timestamp -= 33*90*7;

  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert a few more packets. Make sure we're waiting for the key frame to be
  // complete.
  packet_->isFirstPacket = false;
  for (int i = 1; i < 5; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                              &retransmitted));
    EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // Complete key frame.
  packet_->markerBit = true;
  packet_->seqNum++;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 6 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

// Make sure first packet is present before a frame can be decoded.
TEST_F(TestBasicJitterBuffer, PacketLossWithSelectiveErrorsMissingFirstPacket) {
  jitter_buffer_->SetDecodeErrorMode(kSelectiveErrors);
  // Always start with a key frame.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  // An incomplete frame can only be decoded once a subsequent frame has begun
  // to arrive. Insert packet in distant frame for this purpose.
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = false;
  packet_->markerBit = false;
  packet_->seqNum += 100;
  packet_->timestamp += 33*90*8;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  uint32_t timestamp;
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  // Insert second frame with the first packet missing. Make sure we're waiting
  // for the key frame to be complete.
  packet_->seqNum -= 98;
  packet_->timestamp -= 33*90*7;

  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  for (int i = 0; i < 5; ++i) {
    packet_->seqNum++;
    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                              &retransmitted));
    EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
    EXPECT_FALSE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));
  }

  // Add first packet. Frame should now be decodable, but incomplete.
  packet_->isFirstPacket = true;
  packet_->seqNum -= 6;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &timestamp));
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&timestamp));

  frame_out = DecodeIncompleteFrame();
  CheckOutFrame(frame_out, 7 * size_, false);
  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, DiscontinuousStreamWhenDecodingWithErrors) {
  // Will use one packet per frame.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  uint32_t next_timestamp;
  EXPECT_TRUE(jitter_buffer_->NextCompleteTimestamp(0, &next_timestamp));
  EXPECT_EQ(packet_->timestamp, next_timestamp);
  VCMEncodedFrame* frame = jitter_buffer_->ExtractAndSetDecode(next_timestamp);
  EXPECT_TRUE(frame != NULL);
  jitter_buffer_->ReleaseFrame(frame);

  // Drop a complete frame.
  timestamp_ += 2 * 33 * 90;
  seq_num_ += 2;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  // Insert a packet (so the previous one will be released).
  timestamp_ += 33 * 90;
  seq_num_ += 2;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  EXPECT_FALSE(jitter_buffer_->NextCompleteTimestamp(0, &next_timestamp));
  EXPECT_TRUE(jitter_buffer_->NextMaybeIncompleteTimestamp(&next_timestamp));
  EXPECT_EQ(packet_->timestamp - 33 * 90, next_timestamp);
}

TEST_F(TestBasicJitterBuffer, PacketLoss) {
  // Verify missing packets statistics and not decodable packets statistics.
  // Insert 10 frames consisting of 4 packets and remove one from all of them.
  // The last packet is an empty (non-media) packet.

  // Select a start seqNum which triggers a difficult wrap situation
  // The JB will only output (incomplete)frames if the next one has started
  // to arrive. Start by inserting one frame (key).
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  seq_num_ = 0xffff - 4;
  seq_num_++;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->completeNALU = kNaluStart;

  bool retransmitted = false;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  for (int i = 0; i < 11; ++i) {
    webrtc::FrameType frametype = kVideoFrameDelta;
    seq_num_++;
    timestamp_ += 33*90;
    packet_->frameType = frametype;
    packet_->isFirstPacket = true;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;
    packet_->completeNALU = kNaluStart;

    EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                              &retransmitted));

    VCMEncodedFrame* frame_out = DecodeCompleteFrame();

    // Should not be complete.
    EXPECT_TRUE(frame_out == NULL);

    seq_num_ += 2;
    packet_->isFirstPacket = false;
    packet_->markerBit = true;
    packet_->seqNum = seq_num_;
    packet_->completeNALU = kNaluEnd;

    EXPECT_EQ(jitter_buffer_->InsertPacket(*packet_, &retransmitted),
              kDecodableSession);

    // Insert an empty (non-media) packet.
    seq_num_++;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->completeNALU = kNaluEnd;
    packet_->frameType = kFrameEmpty;

    EXPECT_EQ(jitter_buffer_->InsertPacket(*packet_, &retransmitted),
              kDecodableSession);
    frame_out = DecodeIncompleteFrame();

    // One of the packets has been discarded by the jitter buffer.
    // Last frame can't be extracted yet.
    if (i < 10) {
      CheckOutFrame(frame_out, size_, false);

      if (i == 0) {
          EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
      } else {
         EXPECT_EQ(frametype, frame_out->FrameType());
      }
      EXPECT_FALSE(frame_out->Complete());
      EXPECT_FALSE(frame_out->MissingFrame());
    }

    jitter_buffer_->ReleaseFrame(frame_out);
  }

  // Insert 3 old packets and verify that we have 3 discarded packets
  // Match value to actual latest timestamp decoded.
  timestamp_ -= 33 * 90;
  packet_->timestamp = timestamp_ - 1000;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));

  packet_->timestamp = timestamp_ - 500;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));

  packet_->timestamp = timestamp_ - 100;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));

  EXPECT_EQ(3, jitter_buffer_->num_discarded_packets());

  jitter_buffer_->Flush();

  // This statistic shouldn't be reset by a flush.
  EXPECT_EQ(3, jitter_buffer_->num_discarded_packets());
}

TEST_F(TestBasicJitterBuffer, DeltaFrame100PacketsWithSeqNumWrap) {
  seq_num_ = 0xfff0;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  int loop = 0;
  do {
    seq_num_++;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                        &retransmitted));

    frame_out = DecodeCompleteFrame();

    EXPECT_TRUE(frame_out == NULL);

    loop++;
  } while (loop < 98);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 100 * size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, PacketReorderingReverseWithNegSeqNumWrap) {
  // Insert "first" packet last seqnum.
  seq_num_ = 10;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  // Should not be complete.
  EXPECT_TRUE(frame_out == NULL);

  // Insert 98 frames.
  int loop = 0;
  do {
    seq_num_--;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                        &retransmitted));

    frame_out = DecodeCompleteFrame();

    EXPECT_TRUE(frame_out == NULL);

    loop++;
  } while (loop < 98);

  // Insert last packet.
  seq_num_--;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();
  CheckOutFrame(frame_out, 100 * size_, false);
  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, TestInsertOldFrame) {
  //   -------      -------
  //  |   2   |    |   1   |
  //   -------      -------
  //  t = 3000     t = 2000
  seq_num_ = 2;
  timestamp_ = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->timestamp = timestamp_;
  packet_->seqNum = seq_num_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(3000u, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_--;
  timestamp_ = 2000;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));
}

TEST_F(TestBasicJitterBuffer, TestInsertOldFrameWithSeqNumWrap) {
  //   -------      -------
  //  |   2   |    |   1   |
  //   -------      -------
  //  t = 3000     t = 0xffffff00

  seq_num_ = 2;
  timestamp_ = 3000;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(timestamp_, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_--;
  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;


  // This timestamp is old.
  EXPECT_EQ(kOldPacket, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));
}

TEST_F(TestBasicJitterBuffer, TimestampWrap) {
  //  ---------------     ---------------
  // |   1   |   2   |   |   3   |   4   |
  //  ---------------     ---------------
  //  t = 0xffffff00        t = 33*90

  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 2 * size_, false);

  jitter_buffer_->ReleaseFrame(frame_out);

  seq_num_++;
  timestamp_ += 33*90;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = false;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                       &retransmitted));

  frame_out = DecodeCompleteFrame();

  EXPECT_TRUE(frame_out == NULL);

  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();

  CheckOutFrame(frame_out, 2 * size_, false);

  EXPECT_EQ(kVideoFrameDelta, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, 2FrameWithTimestampWrap) {
  //   -------          -------
  //  |   1   |        |   2   |
  //   -------          -------
  // t = 0xffffff00    t = 2700

  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  // Insert first frame (session will be complete).
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  // Insert next frame.
  seq_num_++;
  timestamp_ = 2700;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(0xffffff00, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  VCMEncodedFrame* frame_out2 = DecodeCompleteFrame();
  EXPECT_EQ(2700u, frame_out2->TimeStamp());

  CheckOutFrame(frame_out2, size_, false);

  EXPECT_EQ(kVideoFrameDelta, frame_out2->FrameType());
}

TEST_F(TestBasicJitterBuffer, Insert2FramesReOrderedWithTimestampWrap) {
  //   -------          -------
  //  |   2   |        |   1   |
  //   -------          -------
  //  t = 2700        t = 0xffffff00

  seq_num_ = 2;
  timestamp_ = 2700;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  bool retransmitted = false;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  // Insert second frame
  seq_num_--;
  timestamp_ = 0xffffff00;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(0xffffff00, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());

  VCMEncodedFrame* frame_out2 = DecodeCompleteFrame();
  EXPECT_EQ(2700u, frame_out2->TimeStamp());

  CheckOutFrame(frame_out2, size_, false);

  EXPECT_EQ(kVideoFrameDelta, frame_out2->FrameType());
}

TEST_F(TestBasicJitterBuffer, DeltaFrameWithMoreThanMaxNumberOfPackets) {
  int loop = 0;
  bool firstPacket = true;
  bool retransmitted = false;
  // Insert kMaxPacketsInJitterBuffer into frame.
  do {
    seq_num_++;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;

    if (firstPacket) {
      EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
      firstPacket = false;
    } else {
      EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(*packet_,
                                                          &retransmitted));
    }

    loop++;
  } while (loop < kMaxPacketsInSession);

  // Max number of packets inserted.
  // Insert one more packet.
  seq_num_++;
  packet_->isFirstPacket = false;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;

  // Insert the packet -> frame recycled.
  EXPECT_EQ(kSizeError, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));
  EXPECT_TRUE(NULL == DecodeCompleteFrame());

}

TEST_F(TestBasicJitterBuffer, ExceedNumOfFrameWithSeqNumWrap) {
  // TEST fill JB with more than max number of frame (50 delta frames +
  // 51 key frames) with wrap in seq_num_
  //
  //  --------------------------------------------------------------
  // | 65485 | 65486 | 65487 | .... | 65535 | 0 | 1 | 2 | .....| 50 |
  //  --------------------------------------------------------------
  // |<-----------delta frames------------->|<------key frames----->|

  int loop = 0;
  seq_num_ = 65485;
  uint32_t first_key_frame_timestamp = 0;
  bool retransmitted = false;
  // Insert MAX_NUMBER_OF_FRAMES frames.
  do {
    timestamp_ += 33*90;
    seq_num_++;
    packet_->isFirstPacket = true;
    packet_->markerBit = true;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;

    if (loop == 50) {
      first_key_frame_timestamp = packet_->timestamp;
      packet_->frameType = kVideoFrameKey;
    }

    // Insert frame.
    EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                             &retransmitted));

    loop++;
  } while (loop < kMaxNumberOfFrames);

  // Max number of frames inserted.

  // Insert one more frame.
  timestamp_ += 33*90;
  seq_num_++;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;

  // Now, no free frame - frames will be recycled until first key frame.
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));

  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_EQ(first_key_frame_timestamp, frame_out->TimeStamp());

  CheckOutFrame(frame_out, size_, false);

  EXPECT_EQ(kVideoFrameKey, frame_out->FrameType());
}

TEST_F(TestBasicJitterBuffer, EmptyLastFrame) {
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  seq_num_ = 3;
  // Insert one empty packet per frame, should never return the last timestamp
  // inserted. Only return empty frames in the presence of subsequent frames.
  int maxSize = 1000;
  bool retransmitted = false;
  for (int i = 0; i < maxSize + 10; i++) {
    timestamp_ += 33 * 90;
    seq_num_++;
    packet_->isFirstPacket = false;
    packet_->markerBit = false;
    packet_->seqNum = seq_num_;
    packet_->timestamp = timestamp_;
    packet_->frameType = kFrameEmpty;

    EXPECT_EQ(kNoError, jitter_buffer_->InsertPacket(*packet_,
                                                     &retransmitted));
    VCMEncodedFrame* testFrame = DecodeIncompleteFrame();
    // Timestamp should never be the last TS inserted.
    if (testFrame != NULL) {
      EXPECT_TRUE(testFrame->TimeStamp() < timestamp_);
    }
  }
}

TEST_F(TestBasicJitterBuffer, H264IncompleteNalu) {
  jitter_buffer_->SetNackMode(kNoNack, -1, -1);
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  ++seq_num_;
  timestamp_ += 33 * 90;
  int insertedLength = 0;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;
  bool retransmitted = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  seq_num_ += 2;  // Skip one packet.
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluIncomplete;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = true;  // Last packet.
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  // The JB will only output (incomplete) frames if a packet belonging to a
  // subsequent frame was already inserted. Insert one packet of a subsequent
  // frame. place high timestamp so the JB would always have a next frame
  // (otherwise, for every inserted frame we need to take care of the next
  // frame as well).
  packet_->seqNum = 1;
  packet_->timestamp = timestamp_ + 33 * 90 * 10;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  VCMEncodedFrame* frame_out = DecodeIncompleteFrame();

  // We can decode everything from a NALU until a packet has been lost.
  // Thus we can decode the first packet of the first NALU and the second NALU
  // which consists of one packet.
  CheckOutFrame(frame_out, packet_->sizeBytes * 2, false);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Test reordered start frame + 1 lost.
  seq_num_ += 2;  // Re-order 1 frame.
  timestamp_ += 33*90;
  insertedLength = 0;

  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  insertedLength += packet_->sizeBytes; // This packet should be decoded.
  seq_num_--;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  insertedLength += packet_->sizeBytes;  // This packet should be decoded.

  seq_num_ += 3;  // One packet drop.
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  insertedLength += packet_->sizeBytes;  // This packet should be decoded.
  seq_num_++;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluStart;
  packet_->markerBit = false;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  // This packet should be decoded since it's the beginning of a NAL.
  insertedLength += packet_->sizeBytes;

  seq_num_ += 2;
  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = false;
  packet_->completeNALU = kNaluEnd;
  packet_->markerBit = true;
  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));
  // This packet should not be decoded because it is an incomplete NAL if it
  // is the last.
  frame_out = DecodeIncompleteFrame();
  // Only last NALU is complete.
  CheckOutFrame(frame_out, insertedLength, false);
  jitter_buffer_->ReleaseFrame(frame_out);

  // Test to insert empty packet.
  seq_num_++;
  timestamp_ += 33 * 90;
  VCMPacket emptypacket(data_, 0, seq_num_, timestamp_, true);
  emptypacket.seqNum = seq_num_;
  emptypacket.timestamp = timestamp_;
  emptypacket.frameType = kVideoFrameKey;
  emptypacket.isFirstPacket = true;
  emptypacket.completeNALU = kNaluComplete;
  emptypacket.markerBit = true;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(emptypacket,
                                                           &retransmitted));
  // This packet should not be decoded because it is an incomplete NAL if it
  // is the last.

  // Will be sent to the decoder, as a packet belonging to a subsequent frame
  // has arrived.
  frame_out = DecodeIncompleteFrame();


  // Test that a frame can include an empty packet.
  seq_num_++;
  timestamp_ += 33 * 90;

  packet_->seqNum = seq_num_;
  packet_->timestamp = timestamp_;
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->completeNALU = kNaluComplete;
  packet_->markerBit = false;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  seq_num_++;
  emptypacket.seqNum = seq_num_;
  emptypacket.timestamp = timestamp_;
  emptypacket.frameType = kVideoFrameKey;
  emptypacket.isFirstPacket = true;
  emptypacket.completeNALU = kNaluComplete;
  emptypacket.markerBit = true;
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(emptypacket,
                                                           &retransmitted));

  frame_out = DecodeCompleteFrame();
  // Only last NALU is complete
  CheckOutFrame(frame_out, packet_->sizeBytes, false);
}

TEST_F(TestBasicJitterBuffer, NextFrameWhenIncomplete) {
  // Test that a we cannot get incomplete frames from the JB if we haven't
  // received the marker bit, unless we have received a packet from a later
  // timestamp.
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);
  // Start with a complete key frame - insert and decode.
  packet_->frameType = kVideoFrameKey;
  packet_->isFirstPacket = true;
  packet_->markerBit = true;
  bool retransmitted = false;

  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(*packet_,
                                                           &retransmitted));
  VCMEncodedFrame* frame_out = DecodeCompleteFrame();
  EXPECT_TRUE(frame_out != NULL);

  packet_->seqNum += 2;
  packet_->timestamp += 33 * 90;
  packet_->frameType = kVideoFrameDelta;
  packet_->isFirstPacket = false;
  packet_->markerBit = false;


  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  frame_out = DecodeIncompleteFrame();
  EXPECT_TRUE(frame_out == NULL);

  packet_->seqNum += 2;
  packet_->timestamp += 33 * 90;
  packet_->isFirstPacket = true;

  EXPECT_EQ(kDecodableSession, jitter_buffer_->InsertPacket(*packet_,
                                                            &retransmitted));

  frame_out = DecodeIncompleteFrame();

  CheckOutFrame(frame_out, packet_->sizeBytes, false);
}

TEST_F(TestRunningJitterBuffer, Full) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());
  DropFrame(1);
  // Fill the jitter buffer.
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kVideoFrameDelta), kNoError);
  // Make sure we can't decode these frames.
  EXPECT_FALSE(DecodeCompleteFrame());
  // This frame will make the jitter buffer recycle frames until a key frame.
  // Since none is found it will have to wait until the next key frame before
  // decoding.
  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  EXPECT_FALSE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, EmptyPackets) {
  // Make sure a frame can get complete even though empty packets are missing.
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 3,
                                   clock_->TimeInMilliseconds());
  bool request_key_frame = false;
  // Insert empty packet.
  EXPECT_EQ(kNoError, InsertPacketAndPop(4));
  EXPECT_FALSE(request_key_frame);
  // Insert 3 media packets.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  // Insert empty packet.
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
}

TEST_F(TestRunningJitterBuffer, StatisticsTest) {
  std::map<FrameType, uint32_t> frame_stats(jitter_buffer_->FrameStatistics());
  EXPECT_EQ(0u, frame_stats[kVideoFrameDelta]);
  EXPECT_EQ(0u, frame_stats[kVideoFrameKey]);

  uint32_t framerate = 0;
  uint32_t bitrate = 0;
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(0u, framerate);
  EXPECT_EQ(0u, bitrate);

  // Insert a couple of key and delta frames.
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameDelta);
  InsertFrame(kVideoFrameKey);
  InsertFrame(kVideoFrameDelta);
  // Decode some of them to make sure the statistics doesn't depend on frames
  // being decoded.
  EXPECT_TRUE(DecodeCompleteFrame());
  EXPECT_TRUE(DecodeCompleteFrame());
  frame_stats = jitter_buffer_->FrameStatistics();
  EXPECT_EQ(3u, frame_stats[kVideoFrameDelta]);
  EXPECT_EQ(2u, frame_stats[kVideoFrameKey]);

  // Insert 20 more frames to get estimates of bitrate and framerate over
  // 1 second.
  for (int i = 0; i < 20; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  // TODO(holmer): The current implementation returns the average of the last
  // two framerate calculations, which is why it takes two calls to reach the
  // actual framerate. This should be fixed.
  EXPECT_EQ(kDefaultFrameRate / 2u, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
  // Insert 25 more frames to get estimates of bitrate and framerate over
  // 2 seconds.
  for (int i = 0; i < 25; ++i) {
    InsertFrame(kVideoFrameDelta);
  }
  jitter_buffer_->IncomingRateStatistics(&framerate, &bitrate);
  EXPECT_EQ(kDefaultFrameRate, framerate);
  EXPECT_EQ(kDefaultBitrateKbps, bitrate);
}

TEST_F(TestRunningJitterBuffer, SkipToKeyFrame) {
  // Insert delta frames.
  EXPECT_GE(InsertFrames(5, kVideoFrameDelta), kNoError);
  // Can't decode without a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  InsertFrame(kVideoFrameKey);
  // Skip to the next key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestRunningJitterBuffer, DontSkipToKeyFrameIfDecodable) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  const int kNumDeltaFrames = 5;
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  for (int i = 0; i < kNumDeltaFrames + 1; ++i) {
    EXPECT_TRUE(DecodeCompleteFrame());
  }
}

TEST_F(TestRunningJitterBuffer, KeyDeltaKeyDelta) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  const int kNumDeltaFrames = 5;
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  EXPECT_GE(InsertFrames(kNumDeltaFrames, kVideoFrameDelta), kNoError);
  InsertFrame(kVideoFrameKey);
  for (int i = 0; i < 2 * (kNumDeltaFrames + 1); ++i) {
    EXPECT_TRUE(DecodeCompleteFrame());
  }
}

TEST_F(TestRunningJitterBuffer, TwoPacketsNonContinuous) {
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                   clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  stream_generator_->GenerateFrame(kVideoFrameDelta, 2, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_TRUE(DecodeCompleteFrame());
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, EmptyPackets) {
  // Make sure empty packets doesn't clog the jitter buffer.
  jitter_buffer_->SetNackMode(kNack, media_optimization::kLowRttNackMs, -1);
  EXPECT_GE(InsertFrames(kMaxNumberOfFrames, kFrameEmpty), kNoError);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackTooOldPackets) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Drop one frame and insert |kNackHistoryLength| to trigger NACKing a too
  // old packet.
  DropFrame(1);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator, InsertFrames(oldest_packet_to_nack_ + 1,
                                          kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  uint16_t* nack_list = jitter_buffer_->GetNackList(&nack_list_length,
                                                    &request_key_frame);
  // No key frame will be requested since the jitter buffer is empty.
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(nack_list == NULL);
  EXPECT_EQ(0, nack_list_length);

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // Verify that the jitter buffer requests a key frame since we need one to
  // start decoding.
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(nack_list == NULL);
  EXPECT_EQ(0, nack_list_length);
  // Waiting for a key frame.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());

  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackLargeJitterBuffer) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_GE(InsertFrames(oldest_packet_to_nack_, kVideoFrameDelta), kNoError);

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  jitter_buffer_->GetNackList(&nack_list_length, &request_key_frame);
  // Verify that the jitter buffer does not request a key frame.
  EXPECT_FALSE(request_key_frame);
  // Verify that no packets are NACKed.
  EXPECT_EQ(0, nack_list_length);
  // Verify that we can decode the next frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NackListFull) {
  // Insert a key frame and decode it.
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeCompleteFrame());

  // Generate and drop |kNackHistoryLength| packets to fill the NACK list.
  DropFrame(max_nack_list_size_ + 1);
  // Insert a frame which should trigger a recycle until the next key frame.
  EXPECT_EQ(kFlushIndicator, InsertFrame(kVideoFrameDelta));
  EXPECT_FALSE(DecodeCompleteFrame());

  uint16_t nack_list_length = max_nack_list_size_;
  bool request_key_frame = false;
  jitter_buffer_->GetNackList(&nack_list_length, &request_key_frame);
  // The jitter buffer is empty, so we won't request key frames until we get a
  // packet.
  EXPECT_FALSE(request_key_frame);

  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  // Now we have a packet in the jitter buffer, a key frame will be requested
  // since it's not a key frame.
  jitter_buffer_->GetNackList(&nack_list_length, &request_key_frame);
  // The jitter buffer is empty, so we won't request key frames until we get a
  // packet.
  EXPECT_TRUE(request_key_frame);
  // The next complete continuous frame isn't a key frame, but we're waiting
  // for one.
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  // Skipping ahead to the key frame.
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, NoNackListReturnedBeforeFirstDecode) {
  DropFrame(10);
  // Insert a frame and try to generate a NACK list. Shouldn't get one.
  EXPECT_GE(InsertFrame(kVideoFrameDelta), kNoError);
  uint16_t nack_list_size = 0;
  bool request_key_frame = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size,
                                                  &request_key_frame);
  // No list generated, and a key frame request is signaled.
  EXPECT_TRUE(list == NULL);
  EXPECT_EQ(0, nack_list_size);
  EXPECT_TRUE(request_key_frame);
}

TEST_F(TestJitterBufferNack, NackListBuiltBeforeFirstDecode) {
  stream_generator_->Init(0, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  stream_generator_->GenerateFrame(kVideoFrameDelta, 2, 0,
                                  clock_->TimeInMilliseconds());
  stream_generator_->NextPacket(NULL);  // Drop packet.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_TRUE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  EXPECT_TRUE(list != NULL);
}

TEST_F(TestJitterBufferNack, VerifyRetransmittedFlag) {
  stream_generator_->Init(0, 0, clock_->TimeInMilliseconds());
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                   clock_->TimeInMilliseconds());
  VCMPacket packet;
  stream_generator_->PopPacket(&packet, 0);
  bool retransmitted = false;
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(packet, &retransmitted));
  EXPECT_FALSE(retransmitted);
  // Drop second packet.
  stream_generator_->PopPacket(&packet, 1);
  EXPECT_EQ(kIncomplete, jitter_buffer_->InsertPacket(packet, &retransmitted));
  EXPECT_FALSE(retransmitted);
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  ASSERT_TRUE(list != NULL);
  stream_generator_->PopPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, list[0]);
  EXPECT_EQ(kCompleteSession, jitter_buffer_->InsertPacket(packet,
                                                           &retransmitted));
  EXPECT_TRUE(retransmitted);
  EXPECT_TRUE(DecodeCompleteFrame());
}

TEST_F(TestJitterBufferNack, UseNackToRecoverFirstKeyFrame) {
  stream_generator_->Init(0, 0, clock_->TimeInMilliseconds());
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  ASSERT_TRUE(list != NULL);
  VCMPacket packet;
  stream_generator_->GetPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, list[0]);
}

TEST_F(TestJitterBufferNack, UseNackToRecoverFirstKeyFrameSecondInQueue) {
  VCMPacket packet;
  stream_generator_->Init(0, 0, clock_->TimeInMilliseconds());
  // First frame is delta.
  stream_generator_->GenerateFrame(kVideoFrameDelta, 3, 0,
                                   clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet in frame.
  ASSERT_TRUE(stream_generator_->PopPacket(&packet, 0));
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Second frame is key.
  stream_generator_->GenerateFrame(kVideoFrameKey, 3, 0,
                                   clock_->TimeInMilliseconds() + 10);
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  // Drop second packet in frame.
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(1));
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(1, nack_list_size);
  ASSERT_TRUE(list != NULL);
  stream_generator_->GetPacket(&packet, 0);
  EXPECT_EQ(packet.seqNum, list[0]);
}

TEST_F(TestJitterBufferNack, NormalOperation) {
  EXPECT_EQ(kNack, jitter_buffer_->nack_mode());
  jitter_buffer_->SetDecodeErrorMode(kWithErrors);

  EXPECT_GE(InsertFrame(kVideoFrameKey), kNoError);
  EXPECT_TRUE(DecodeIncompleteFrame());

  //  ----------------------------------------------------------------
  // | 1 | 2 | .. | 8 | 9 | x | 11 | 12 | .. | 19 | x | 21 | .. | 100 |
  //  ----------------------------------------------------------------
  stream_generator_->GenerateFrame(kVideoFrameKey, 100, 0,
                                  clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
  // Verify that the frame is incomplete.
  EXPECT_FALSE(DecodeCompleteFrame());
  while (stream_generator_->PacketsRemaining() > 1) {
    if (stream_generator_->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
  }
  EXPECT_EQ(kDecodableSession, InsertPacketAndPop(0));
  EXPECT_EQ(0, stream_generator_->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeIncompleteFrame());
  uint16_t nack_list_size = 0;
  bool request_key_frame = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size,
                                               &request_key_frame);
  // Verify the NACK list.
  const int kExpectedNackSize = 9;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ((1 + i) * 10, list[i]);
}

TEST_F(TestJitterBufferNack, NormalOperationWrap) {
  bool request_key_frame = false;
  //  -------   ------------------------------------------------------------
  // | 65532 | | 65533 | 65534 | 65535 | x | 1 | .. | 9 | x | 11 |.....| 96 |
  //  -------   ------------------------------------------------------------
  stream_generator_->Init(65532, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 100, 0,
                                  clock_->TimeInMilliseconds());
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  while (stream_generator_->PacketsRemaining() > 1) {
    if (stream_generator_->NextSequenceNumber() % 10 != 0) {
      EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
      EXPECT_FALSE(request_key_frame);
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
  }
  EXPECT_EQ(kIncomplete, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  EXPECT_EQ(0, stream_generator_->PacketsRemaining());
  EXPECT_FALSE(DecodeCompleteFrame());
  EXPECT_FALSE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  // Verify the NACK list.
  const int kExpectedNackSize = 10;
  ASSERT_EQ(kExpectedNackSize, nack_list_size);
  for (int i = 0; i < nack_list_size; ++i)
    EXPECT_EQ(i * 10, list[i]);
}

TEST_F(TestJitterBufferNack, NormalOperationWrap2) {
  bool request_key_frame = false;
  //  -----------------------------------
  // | 65532 | 65533 | 65534 | x | 0 | 1 |
  //  -----------------------------------
  stream_generator_->Init(65532, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_FALSE(request_key_frame);
  EXPECT_TRUE(DecodeCompleteFrame());
  stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                   clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  for (int i = 0; i < 5; ++i) {
    if (stream_generator_->NextSequenceNumber()  != 65535) {
      EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
      EXPECT_FALSE(request_key_frame);
    } else {
      stream_generator_->NextPacket(NULL);  // Drop packet
    }
    stream_generator_->GenerateFrame(kVideoFrameDelta, 1, 0,
                                     clock_->TimeInMilliseconds());
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  }
  EXPECT_EQ(kCompleteSession, InsertPacketAndPop(0));
  EXPECT_FALSE(request_key_frame);
  uint16_t nack_list_size = 0;
  bool extended = false;
  uint16_t* list = jitter_buffer_->GetNackList(&nack_list_size, &extended);
  // Verify the NACK list.
  ASSERT_EQ(1, nack_list_size);
  EXPECT_EQ(65535, list[0]);
}

TEST_F(TestJitterBufferNack, ResetByFutureKeyFrameDoesntError) {
  stream_generator_->Init(0, 0, clock_->TimeInMilliseconds());
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  uint16_t nack_list_size = 0;
  bool extended = false;
  jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(0, nack_list_size);

  // Far-into-the-future video frame, could be caused by resetting the encoder
  // or otherwise restarting. This should not fail when error when the packet is
  // a keyframe, even if all of the nack list needs to be flushed.
  stream_generator_->Init(10000, 0, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  InsertFrame(kVideoFrameKey);
  EXPECT_TRUE(DecodeCompleteFrame());
  jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(0, nack_list_size);

  // Stream should be decodable from this point.
  clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
  InsertFrame(kVideoFrameDelta);
  EXPECT_TRUE(DecodeCompleteFrame());
  jitter_buffer_->GetNackList(&nack_list_size, &extended);
  EXPECT_EQ(0, nack_list_size);
}

}  // namespace webrtc
