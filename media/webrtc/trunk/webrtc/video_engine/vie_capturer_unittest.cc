/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file includes unit tests for ViECapturer.

#include "webrtc/video_engine/vie_capturer.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common.h"
#include "webrtc/common_video/interface/native_handle.h"
#include "webrtc/common_video/interface/texture_video_frame.h"
#include "webrtc/modules/utility/interface/mock/mock_process_thread.h"
#include "webrtc/modules/video_capture/include/mock/mock_video_capture.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"
#include "webrtc/video_engine/mock/mock_vie_frame_provider_base.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithArg;

// If an output frame does not arrive in 500ms, the test will fail.
#define FRAME_TIMEOUT_MS 500

namespace webrtc {

bool EqualFrames(const I420VideoFrame& frame1,
                 const I420VideoFrame& frame2);
bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2);
bool EqualBufferFrames(const I420VideoFrame& frame1,
                       const I420VideoFrame& frame2);
bool EqualFramesVector(const ScopedVector<I420VideoFrame>& frames1,
                       const ScopedVector<I420VideoFrame>& frames2);
I420VideoFrame* CreateI420VideoFrame(uint8_t length);

class FakeNativeHandle : public NativeHandle {
 public:
  FakeNativeHandle() {}
  virtual ~FakeNativeHandle() {}
  virtual void* GetHandle() { return NULL; }
};

class ViECapturerTest : public ::testing::Test {
 protected:
  ViECapturerTest()
      : mock_capture_module_(new NiceMock<MockVideoCaptureModule>()),
        mock_process_thread_(new NiceMock<MockProcessThread>),
        mock_frame_callback_(new NiceMock<MockViEFrameCallback>),
        data_callback_(NULL),
        output_frame_event_(EventWrapper::Create()) {
  }

  virtual void SetUp() {
    EXPECT_CALL(*mock_capture_module_, RegisterCaptureDataCallback(_))
        .WillRepeatedly(Invoke(this, &ViECapturerTest::SetCaptureDataCallback));
    EXPECT_CALL(*mock_frame_callback_, DeliverFrame(_, _, _, _))
        .WillRepeatedly(
            WithArg<1>(Invoke(this, &ViECapturerTest::AddOutputFrame)));

    Config config;
    vie_capturer_.reset(
        ViECapturer::CreateViECapture(
            0, 0, config, mock_capture_module_.get(), *mock_process_thread_));
    vie_capturer_->RegisterFrameCallback(0, mock_frame_callback_.get());
  }

  virtual void TearDown() {
    // ViECapturer accesses |mock_process_thread_| in destructor and should
    // be deleted first.
    vie_capturer_.reset();
  }

  void SetCaptureDataCallback(VideoCaptureDataCallback& data_callback) {
    data_callback_ = &data_callback;
  }

  void AddInputFrame(I420VideoFrame* frame) {
    data_callback_->OnIncomingCapturedFrame(0, *frame);
  }

  void AddOutputFrame(I420VideoFrame* frame) {
    if (frame->native_handle() == NULL)
      output_frame_ybuffers_.push_back(frame->buffer(kYPlane));
    // Clone the frames because ViECapturer owns the frames.
    output_frames_.push_back(frame->CloneFrame());
    output_frame_event_->Set();
  }

  void WaitOutputFrame() {
    EXPECT_EQ(kEventSignaled, output_frame_event_->Wait(FRAME_TIMEOUT_MS));
  }

  scoped_ptr<MockVideoCaptureModule> mock_capture_module_;
  scoped_ptr<MockProcessThread> mock_process_thread_;
  scoped_ptr<MockViEFrameCallback> mock_frame_callback_;

  // Used to send input capture frames to ViECapturer.
  VideoCaptureDataCallback* data_callback_;

  scoped_ptr<ViECapturer> vie_capturer_;

  // Input capture frames of ViECapturer.
  ScopedVector<I420VideoFrame> input_frames_;

  // Indicate an output frame has arrived.
  scoped_ptr<EventWrapper> output_frame_event_;

  // Output delivered frames of ViECaptuer.
  ScopedVector<I420VideoFrame> output_frames_;

  // The pointers of Y plane buffers of output frames. This is used to verify
  // the frame are swapped and not copied.
  std::vector<uint8_t*> output_frame_ybuffers_;
};

TEST_F(ViECapturerTest, TestTextureFrames) {
  const int kNumFrame = 3;
  for (int i = 0 ; i < kNumFrame; ++i) {
    webrtc::RefCountImpl<FakeNativeHandle>* handle =
              new webrtc::RefCountImpl<FakeNativeHandle>();
    input_frames_.push_back(new TextureVideoFrame(handle, i, i, i, i));
    AddInputFrame(input_frames_[i]);
    WaitOutputFrame();
  }

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

TEST_F(ViECapturerTest, TestI420Frames) {
  const int kNumFrame = 4;
  ScopedVector<I420VideoFrame> copied_input_frames;
  std::vector<uint8_t*> ybuffer_pointers;
  for (int i = 0; i < kNumFrame; ++i) {
    input_frames_.push_back(CreateI420VideoFrame(static_cast<uint8_t>(i + 1)));
    ybuffer_pointers.push_back(input_frames_[i]->buffer(kYPlane));
    // Copy input frames because the buffer data will be swapped.
    copied_input_frames.push_back(input_frames_[i]->CloneFrame());
    AddInputFrame(input_frames_[i]);
    WaitOutputFrame();
  }

  EXPECT_TRUE(EqualFramesVector(copied_input_frames, output_frames_));
  // Make sure the buffer is swapped and not copied.
  for (int i = 0; i < kNumFrame; ++i)
    EXPECT_EQ(ybuffer_pointers[i], output_frame_ybuffers_[i]);
  // The pipeline should be filled with frames with allocated buffers. Check
  // the last input frame has the same allocated size after swapping.
  EXPECT_EQ(input_frames_.back()->allocated_size(kYPlane),
            copied_input_frames.back()->allocated_size(kYPlane));
}

TEST_F(ViECapturerTest, TestI420FrameAfterTextureFrame) {
  webrtc::RefCountImpl<FakeNativeHandle>* handle =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  input_frames_.push_back(new TextureVideoFrame(handle, 1, 1, 1, 1));
  AddInputFrame(input_frames_[0]);
  WaitOutputFrame();

  input_frames_.push_back(CreateI420VideoFrame(1));
  scoped_ptr<I420VideoFrame> copied_input_frame(input_frames_[1]->CloneFrame());
  AddInputFrame(copied_input_frame.get());
  WaitOutputFrame();

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

TEST_F(ViECapturerTest, TestTextureFrameAfterI420Frame) {
  input_frames_.push_back(CreateI420VideoFrame(1));
  scoped_ptr<I420VideoFrame> copied_input_frame(input_frames_[0]->CloneFrame());
  AddInputFrame(copied_input_frame.get());
  WaitOutputFrame();

  webrtc::RefCountImpl<FakeNativeHandle>* handle =
      new webrtc::RefCountImpl<FakeNativeHandle>();
  input_frames_.push_back(new TextureVideoFrame(handle, 1, 1, 1, 1));
  AddInputFrame(input_frames_[1]);
  WaitOutputFrame();

  EXPECT_TRUE(EqualFramesVector(input_frames_, output_frames_));
}

bool EqualFrames(const I420VideoFrame& frame1,
                 const I420VideoFrame& frame2) {
  if (frame1.native_handle() != NULL || frame2.native_handle() != NULL)
    return EqualTextureFrames(frame1, frame2);
  return EqualBufferFrames(frame1, frame2);
}

bool EqualTextureFrames(const I420VideoFrame& frame1,
                        const I420VideoFrame& frame2) {
  return ((frame1.native_handle() == frame2.native_handle()) &&
          (frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.timestamp() == frame2.timestamp()) &&
          (frame1.render_time_ms() == frame2.render_time_ms()));
}

bool EqualBufferFrames(const I420VideoFrame& frame1,
                       const I420VideoFrame& frame2) {
  return ((frame1.width() == frame2.width()) &&
          (frame1.height() == frame2.height()) &&
          (frame1.stride(kYPlane) == frame2.stride(kYPlane)) &&
          (frame1.stride(kUPlane) == frame2.stride(kUPlane)) &&
          (frame1.stride(kVPlane) == frame2.stride(kVPlane)) &&
          (frame1.timestamp() == frame2.timestamp()) &&
          (frame1.ntp_time_ms() == frame2.ntp_time_ms()) &&
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

bool EqualFramesVector(const ScopedVector<I420VideoFrame>& frames1,
                       const ScopedVector<I420VideoFrame>& frames2) {
  if (frames1.size() != frames2.size())
    return false;
  for (size_t i = 0; i < frames1.size(); ++i) {
    if (!EqualFrames(*frames1[i], *frames2[i]))
      return false;
  }
  return true;
}

I420VideoFrame* CreateI420VideoFrame(uint8_t data) {
  I420VideoFrame* frame = new I420VideoFrame();
  const int width = 36;
  const int height = 24;
  const int kSizeY = width * height * 2;
  const int kSizeUV = width * height;
  uint8_t buffer[kSizeY];
  memset(buffer, data, kSizeY);
  frame->CreateFrame(
      kSizeY, buffer, kSizeUV, buffer, kSizeUV, buffer, width, height, width,
      width / 2, width / 2);
  frame->set_timestamp(data);
  frame->set_ntp_time_ms(data);
  frame->set_render_time_ms(data);
  return frame;
}

}  // namespace webrtc
