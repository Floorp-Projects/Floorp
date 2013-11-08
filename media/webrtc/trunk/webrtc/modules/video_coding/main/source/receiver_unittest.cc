/*  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
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
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/receiver.h"
#include "webrtc/modules/video_coding/main/source/test/stream_generator.h"
#include "webrtc/modules/video_coding/main/source/timing.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class TestVCMReceiver : public ::testing::Test {
 protected:
  enum { kDataBufferSize = 10 };
  enum { kWidth = 640 };
  enum { kHeight = 480 };

  TestVCMReceiver()
      : clock_(new SimulatedClock(0)),
        timing_(clock_.get()),
        receiver_(&timing_, clock_.get(), &event_factory_, 1, 1, true) {
    stream_generator_.reset(new
        StreamGenerator(0, 0, clock_->TimeInMilliseconds()));
    memset(data_buffer_, 0, kDataBufferSize);
  }

  virtual void SetUp() {
    receiver_.Reset();
  }

  int32_t InsertPacket(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->GetPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    // Arbitrary width and height.
    return receiver_.InsertPacket(packet, 640, 480);
  }

  int32_t InsertPacketAndPop(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->PopPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kGeneralError;  // Return here to avoid crashes below.
    return receiver_.InsertPacket(packet, kWidth, kHeight);
  }

  int32_t InsertFrame(FrameType frame_type, bool complete) {
    int num_of_packets = complete ? 1 : 2;
    stream_generator_->GenerateFrame(
        frame_type,
        (frame_type != kFrameEmpty) ? num_of_packets : 0,
        (frame_type == kFrameEmpty) ? 1 : 0,
        clock_->TimeInMilliseconds());
    int32_t ret = InsertPacketAndPop(0);
    if (!complete) {
      // Drop the second packet.
      VCMPacket packet;
      stream_generator_->PopPacket(&packet, 0);
    }
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
    return ret;
  }

  bool DecodeNextFrame() {
    int64_t render_time_ms = 0;
    VCMEncodedFrame* frame = receiver_.FrameForDecoding(0, render_time_ms,
                                                        false, NULL);
    if (!frame)
      return false;
    receiver_.ReleaseFrame(frame);
    return true;
  }

  scoped_ptr<SimulatedClock> clock_;
  VCMTiming timing_;
  NullEventFactory event_factory_;
  VCMReceiver receiver_;
  scoped_ptr<StreamGenerator> stream_generator_;
  uint8_t data_buffer_[kDataBufferSize];
};

TEST_F(TestVCMReceiver, RenderBufferSize_AllComplete) {
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  int num_of_frames = 10;
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_EQ(num_of_frames * kDefaultFramePeriodMs,
            receiver_.RenderBufferSizeMs());
}

TEST_F(TestVCMReceiver, RenderBufferSize_SkipToKeyFrame) {
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
  const int kNumOfNonDecodableFrames = 2;
  for (int i = 0; i < kNumOfNonDecodableFrames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  const int kNumOfFrames = 10;
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  for (int i = 0; i < kNumOfFrames - 1; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_EQ((kNumOfFrames - 1) * kDefaultFramePeriodMs,
      receiver_.RenderBufferSizeMs());
}

TEST_F(TestVCMReceiver, RenderBufferSize_NotAllComplete) {
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  int num_of_frames = 10;
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  num_of_frames++;
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_EQ((num_of_frames - 1) * kDefaultFramePeriodMs,
      receiver_.RenderBufferSizeMs());
}

TEST_F(TestVCMReceiver, RenderBufferSize_NoKeyFrame) {
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
  int num_of_frames = 10;
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  int64_t next_render_time_ms = 0;
  VCMEncodedFrame* frame = receiver_.FrameForDecoding(10, next_render_time_ms);
  EXPECT_TRUE(frame == NULL);
  receiver_.ReleaseFrame(frame);
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
}

TEST_F(TestVCMReceiver, NonDecodableDuration_Empty) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  const int kMinDelayMs = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Advance time until it's time to decode the key frame.
  clock_->AdvanceTimeMilliseconds(kMinDelayMs);
  EXPECT_TRUE(DecodeNextFrame());
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackOk, ret);
}

TEST_F(TestVCMReceiver, NonDecodableDuration_NoKeyFrame) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  const int kNumFrames = kDefaultFrameRate * kMaxNonDecodableDuration / 1000;
  for (int i = 0; i < kNumFrames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackKeyFrameRequest, ret);
}

TEST_F(TestVCMReceiver, NonDecodableDuration_OneIncomplete) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  const int kMaxNonDecodableDurationFrames = (kDefaultFrameRate *
      kMaxNonDecodableDuration + 500) / 1000;
  const int kMinDelayMs = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  receiver_.SetMinReceiverDelay(kMinDelayMs);
  int64_t key_frame_inserted = clock_->TimeInMilliseconds();
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Insert an incomplete frame.
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  // Insert enough frames to have too long non-decodable sequence.
  for (int i = 0; i < kMaxNonDecodableDurationFrames;
       ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  // Advance time until it's time to decode the key frame.
  clock_->AdvanceTimeMilliseconds(kMinDelayMs - clock_->TimeInMilliseconds() -
      key_frame_inserted);
  EXPECT_TRUE(DecodeNextFrame());
  // Make sure we get a key frame request.
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackKeyFrameRequest, ret);
}

TEST_F(TestVCMReceiver, NonDecodableDuration_NoTrigger) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  const int kMaxNonDecodableDurationFrames = (kDefaultFrameRate *
      kMaxNonDecodableDuration + 500) / 1000;
  const int kMinDelayMs = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  receiver_.SetMinReceiverDelay(kMinDelayMs);
  int64_t key_frame_inserted = clock_->TimeInMilliseconds();
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Insert an incomplete frame.
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  // Insert all but one frame to not trigger a key frame request due to
  // too long duration of non-decodable frames.
  for (int i = 0; i < kMaxNonDecodableDurationFrames - 1;
       ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  // Advance time until it's time to decode the key frame.
  clock_->AdvanceTimeMilliseconds(kMinDelayMs - clock_->TimeInMilliseconds() -
      key_frame_inserted);
  EXPECT_TRUE(DecodeNextFrame());
  // Make sure we don't get a key frame request since we haven't generated
  // enough frames.
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackOk, ret);
}

TEST_F(TestVCMReceiver, NonDecodableDuration_NoTrigger2) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  const int kMaxNonDecodableDurationFrames = (kDefaultFrameRate *
      kMaxNonDecodableDuration + 500) / 1000;
  const int kMinDelayMs = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  receiver_.SetMinReceiverDelay(kMinDelayMs);
  int64_t key_frame_inserted = clock_->TimeInMilliseconds();
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Insert enough frames to have too long non-decodable sequence, except that
  // we don't have any losses.
  for (int i = 0; i < kMaxNonDecodableDurationFrames;
       ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  // Insert an incomplete frame.
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  // Advance time until it's time to decode the key frame.
  clock_->AdvanceTimeMilliseconds(kMinDelayMs - clock_->TimeInMilliseconds() -
      key_frame_inserted);
  EXPECT_TRUE(DecodeNextFrame());
  // Make sure we don't get a key frame request since the non-decodable duration
  // is only one frame.
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackOk, ret);
}

TEST_F(TestVCMReceiver, NonDecodableDuration_KeyFrameAfterIncompleteFrames) {
  // Enable NACK and with no RTT thresholds for disabling retransmission delay.
  receiver_.SetNackMode(kNack, -1, -1);
  const size_t kMaxNackListSize = 1000;
  const int kMaxPacketAgeToNack = 1000;
  const int kMaxNonDecodableDuration = 500;
  const int kMaxNonDecodableDurationFrames = (kDefaultFrameRate *
      kMaxNonDecodableDuration + 500) / 1000;
  const int kMinDelayMs = 500;
  receiver_.SetNackSettings(kMaxNackListSize, kMaxPacketAgeToNack,
      kMaxNonDecodableDuration);
  receiver_.SetMinReceiverDelay(kMinDelayMs);
  int64_t key_frame_inserted = clock_->TimeInMilliseconds();
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Insert an incomplete frame.
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  // Insert enough frames to have too long non-decodable sequence.
  for (int i = 0; i < kMaxNonDecodableDurationFrames;
       ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_GE(InsertFrame(kVideoFrameKey, true), kNoError);
  // Advance time until it's time to decode the key frame.
  clock_->AdvanceTimeMilliseconds(kMinDelayMs - clock_->TimeInMilliseconds() -
      key_frame_inserted);
  EXPECT_TRUE(DecodeNextFrame());
  // Make sure we don't get a key frame request since we have a key frame
  // in the list.
  uint16_t nack_list[kMaxNackListSize];
  uint16_t nack_list_length = 0;
  VCMNackStatus ret = receiver_.NackList(nack_list, kMaxNackListSize,
                                         &nack_list_length);
  EXPECT_EQ(kNackOk, ret);
}
}  // namespace webrtc
