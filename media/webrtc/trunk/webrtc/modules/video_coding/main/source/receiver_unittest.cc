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
#include "webrtc/modules/video_coding/main/source/receiver.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/source/timing.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"
#include "webrtc/modules/video_coding/main/source/stream_generator.h"
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
      return kStateError;  // Return here to avoid crashes below.
    // Arbitrary width and height.
    return receiver_.InsertPacket(packet, 640, 480);
  }

  int32_t InsertPacketAndPop(int index) {
    VCMPacket packet;
    packet.dataPtr = data_buffer_;
    bool packet_available = stream_generator_->PopPacket(&packet, index);
    EXPECT_TRUE(packet_available);
    if (!packet_available)
      return kStateError;  // Return here to avoid crashes below.
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
    clock_->AdvanceTimeMilliseconds(kDefaultFramePeriodMs);
    return ret;
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
  EXPECT_EQ(num_of_frames  * kDefaultFramePeriodMs,
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
  EXPECT_EQ(num_of_frames * kDefaultFramePeriodMs,
      receiver_.RenderBufferSizeMs());
}

TEST_F(TestVCMReceiver, RenderBufferSize_NoKeyFrame) {
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
  int num_of_frames = 10;
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_GE(InsertFrame(kVideoFrameDelta, false), kNoError);
  for (int i = 0; i < num_of_frames; ++i) {
    EXPECT_GE(InsertFrame(kVideoFrameDelta, true), kNoError);
  }
  EXPECT_EQ(0, receiver_.RenderBufferSizeMs());
}

}  // namespace webrtc
