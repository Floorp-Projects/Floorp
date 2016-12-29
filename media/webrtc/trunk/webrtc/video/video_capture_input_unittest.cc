/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video/video_capture_input.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/event.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common.h"
#include "webrtc/modules/utility/include/mock/mock_process_thread.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/ref_count.h"
#include "webrtc/system_wrappers/include/scoped_vector.h"
#include "webrtc/test/fake_texture_frame.h"
#include "webrtc/video/send_statistics_proxy.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

// If an output frame does not arrive in 500ms, the test will fail.
#define FRAME_TIMEOUT_MS 500

namespace webrtc {

class MockVideoCaptureCallback : public VideoCaptureCallback {
 public:
  MOCK_METHOD1(DeliverFrame, void(VideoFrame video_frame));
};

bool EqualFrames(const VideoFrame& frame1, const VideoFrame& frame2);
bool EqualTextureFrames(const VideoFrame& frame1, const VideoFrame& frame2);
bool EqualBufferFrames(const VideoFrame& frame1, const VideoFrame& frame2);
bool EqualFramesVector(const ScopedVector<VideoFrame>& frames1,
                       const ScopedVector<VideoFrame>& frames2);
VideoFrame* CreateVideoFrame(uint8_t length);

class VideoCaptureInputTest : public ::testing::Test {
 protected:
  VideoCaptureInputTest()
      : mock_process_thread_(new NiceMock<MockProcessThread>),
        mock_frame_callback_(new NiceMock<MockVideoCaptureCallback>),
        output_frame_event_(false, false),
        stats_proxy_(Clock::GetRealTimeClock(),
                     webrtc::VideoSendStream::Config(nullptr),
                     webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo) {}

  virtual void SetUp() {
    EXPECT_CALL(*mock_frame_callback_, DeliverFrame(_))
        .WillRepeatedly(
            WithArg<0>(Invoke(this, &VideoCaptureInputTest::AddOutputFrame)));

    Config config;
    input_.reset(new internal::VideoCaptureInput(
        mock_process_thread_.get(), mock_frame_callback_.get(), nullptr,
        &stats_proxy_, nullptr, nullptr));
  }

  virtual void TearDown() {
    // VideoCaptureInput accesses |mock_process_thread_| in destructor and
    // should
    // be deleted first.
    input_.reset();
  }

  void AddInputFrame(VideoFrame* frame) {
    input_->IncomingCapturedFrame(*frame);
  }

  void AddOutputFrame(const VideoFrame& frame) {
    if (frame.native_handle() == NULL)
      output_frame_ybuffers_.push_back(frame.buffer(kYPlane));
    output_frames_.push_back(new VideoFrame(frame));
    output_frame_event_.Set();
  }

  void WaitOutputFrame() {
    EXPECT_TRUE(output_frame_event_.Wait(FRAME_TIMEOUT_MS));
  }

  rtc::scoped_ptr<MockProcessThread> mock_process_thread_;
  rtc::scoped_ptr<MockVideoCaptureCallback> mock_frame_callback_;

  // Used to send input capture frames to VideoCaptureInput.
  rtc::scoped_ptr<internal::VideoCaptureInput> input_;

  // Input capture frames of VideoCaptureInput.
  ScopedVector<VideoFrame> input_frames_;

  // Indicate an output frame has arrived.
  rtc::Event output_frame_event_;

  // Output delivered frames of VideoCaptureInput.
  ScopedVector<VideoFrame> output_frames_;

  // The pointers of Y plane buffers of output frames. This is used to verify
  // the frame are swapped and not copied.
  std::vector<const uint8_t*> output_frame_ybuffers_;
  SendStatisticsProxy stats_proxy_;
};

TEST_F(VideoCaptureInputTest, DoesNotRetainHandleNorCopyBuffer) {
  // Indicate an output frame has arrived.
  rtc::Event frame_destroyed_event(false, false);
  class TestBuffer : public webrtc::I420Buffer {
   public:
    explicit TestBuffer(rtc::Event* event) : I420Buffer(5, 5), event_(event) {}

   private:
    friend class rtc::RefCountedObject<TestBuffer>;
    ~TestBuffer() override { event_->Set(); }
    rtc::Event* const event_;
  };

  VideoFrame frame(
      new rtc::RefCountedObject<TestBuffer>(&frame_destroyed_event), 1, 1,
      kVideoRotation_0);

  AddInputFrame(&frame);
  WaitOutputFrame();

  EXPECT_EQ(output_frames_[0]->video_frame_buffer().get(),
            frame.video_frame_buffer().get());
  output_frames_.clear();
  frame.Reset();
  EXPECT_TRUE(frame_destroyed_event.Wait(FRAME_TIMEOUT_MS));
}

TEST_F(VideoCaptureInputTest, TestNtpTimeStampSetIfRenderTimeSet) {
  input_frames_.push_back(CreateVideoFrame(0));
  input_frames_[0]->set_render_time_ms(5);
  input_frames_[0]->set_ntp_time_ms(0);

  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();
  EXPECT_GT(output_frames_[0]->ntp_time_ms(),
            input_frames_[0]->render_time_ms());
}

TEST_F(VideoCaptureInputTest, TestRtpTimeStampSet) {
  input_frames_.push_back(CreateVideoFrame(0));
  input_frames_[0]->set_render_time_ms(0);
  input_frames_[0]->set_ntp_time_ms(1);
  input_frames_[0]->set_timestamp(0);

  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();
  EXPECT_EQ(output_frames_[0]->timestamp(),
            input_frames_[0]->ntp_time_ms() * 90);
}

TEST_F(VideoCaptureInputTest, DropsFramesWithSameOrOldNtpTimestamp) {
  input_frames_.push_back(CreateVideoFrame(0));

  input_frames_[0]->set_ntp_time_ms(17);
  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();
  EXPECT_EQ(output_frames_[0]->timestamp(),
            input_frames_[0]->ntp_time_ms() * 90);

  // Repeat frame with the same NTP timestamp should drop.
  AddInputFrame(input_frames_[0]);
  EXPECT_FALSE(output_frame_event_.Wait(FRAME_TIMEOUT_MS));

  // As should frames with a decreased NTP timestamp.
  input_frames_[0]->set_ntp_time_ms(input_frames_[0]->ntp_time_ms() - 1);
  AddInputFrame(input_frames_[0]);
  EXPECT_FALSE(output_frame_event_.Wait(FRAME_TIMEOUT_MS));

  // But delivering with an increased NTP timestamp should succeed.
  input_frames_[0]->set_ntp_time_ms(4711);
  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();
  EXPECT_EQ(output_frames_[1]->timestamp(),
            input_frames_[0]->ntp_time_ms() * 90);
}

TEST_F(VideoCaptureInputTest, TestTextureFrames) {
  const int kNumFrame = 3;
  for (int i = 0 ; i < kNumFrame; ++i) {
    test::FakeNativeHandle* dummy_handle = new test::FakeNativeHandle();
    // Add one to |i| so that width/height > 0.
    input_frames_.push_back(new VideoFrame(test::FakeNativeHandle::CreateFrame(
        dummy_handle, i + 1, i + 1, i + 1, i + 1, webrtc::kVideoRotation_0)));
    AddInputFrame(input_frames_[i]);
    WaitOutputFrame();
    EXPECT_EQ(dummy_handle, output_frames_[i]->native_handle());
  }

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

TEST_F(VideoCaptureInputTest, TestI420Frames) {
  const int kNumFrame = 4;
  std::vector<const uint8_t*> ybuffer_pointers;
  for (int i = 0; i < kNumFrame; ++i) {
    input_frames_.push_back(CreateVideoFrame(static_cast<uint8_t>(i + 1)));
    const VideoFrame* const_input_frame = input_frames_[i];
    ybuffer_pointers.push_back(const_input_frame->buffer(kYPlane));
    AddInputFrame(input_frames_[i]);
    WaitOutputFrame();
  }

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
  // Make sure the buffer is not copied.
  for (int i = 0; i < kNumFrame; ++i)
    EXPECT_EQ(ybuffer_pointers[i], output_frame_ybuffers_[i]);
}

TEST_F(VideoCaptureInputTest, TestI420FrameAfterTextureFrame) {
  test::FakeNativeHandle* dummy_handle = new test::FakeNativeHandle();
  input_frames_.push_back(new VideoFrame(test::FakeNativeHandle::CreateFrame(
      dummy_handle, 1, 1, 1, 1, webrtc::kVideoRotation_0)));
  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();
  EXPECT_EQ(dummy_handle, output_frames_[0]->native_handle());

  input_frames_.push_back(CreateVideoFrame(2));
  AddInputFrame(input_frames_[1]);
  WaitOutputFrame();

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

TEST_F(VideoCaptureInputTest, TestTextureFrameAfterI420Frame) {
  input_frames_.push_back(CreateVideoFrame(1));
  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();

  test::FakeNativeHandle* dummy_handle = new test::FakeNativeHandle();
  input_frames_.push_back(new VideoFrame(test::FakeNativeHandle::CreateFrame(
      dummy_handle, 1, 1, 2, 2, webrtc::kVideoRotation_0)));
  AddInputFrame(input_frames_[1]);
  WaitOutputFrame();

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

bool EqualFrames(const VideoFrame& frame1, const VideoFrame& frame2) {
  if (frame1.native_handle() != NULL || frame2.native_handle() != NULL)
    return EqualTextureFrames(frame1, frame2);
  return EqualBufferFrames(frame1, frame2);
}

bool EqualTextureFrames(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ((frame1.native_handle() == frame2.native_handle()) &&
          (frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.render_time_ms() == frame2.render_time_ms()));
}

bool EqualBufferFrames(const VideoFrame& frame1, const VideoFrame& frame2) {
  return ((frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.stride(kYPlane) == frame2.stride(kYPlane)) &&
          (frame1.stride(kUPlane) == frame2.stride(kUPlane)) &&
          (frame1.stride(kVPlane) == frame2.stride(kVPlane)) &&
          (frame1.render_time_ms() == frame2.render_time_ms()) &&
          (frame1.allocated_size(kYPlane) == frame2.allocated_size(kYPlane)) &&
          (frame1.allocated_size(kUPlane) == frame2.allocated_size(kUPlane)) &&
          (frame1.allocated_size(kVPlane) == frame2.allocated_size(kVPlane)) &&
          (memcmp(frame1.buffer(kYPlane), frame2.buffer(kYPlane),
                  frame1.allocated_size(kYPlane)) == 0) &&
          (memcmp(frame1.buffer(kUPlane), frame2.buffer(kUPlane),
                  frame1.allocated_size(kUPlane)) == 0) &&
          (memcmp(frame1.buffer(kVPlane), frame2.buffer(kVPlane),
                  frame1.allocated_size(kVPlane)) == 0));
}

bool EqualFramesVector(const ScopedVector<VideoFrame>& frames1,
                       const ScopedVector<VideoFrame>& frames2) {
  if (frames1.size() != frames2.size())
    return false;
  for (size_t i = 0; i < frames1.size(); ++i) {
    if (!EqualFrames(*frames1[i], *frames2[i]))
      return false;
  }
  return true;
}

VideoFrame* CreateVideoFrame(uint8_t data) {
  VideoFrame* frame = new VideoFrame();
  const int width = 36;
  const int height = 24;
  const int kSizeY = width * height * 2;
  uint8_t buffer[kSizeY];
  memset(buffer, data, kSizeY);
  frame->CreateFrame(buffer, buffer, buffer, width, height, width, width / 2,
                     width / 2);
  frame->set_render_time_ms(data);
  return frame;
}

}  // namespace webrtc
