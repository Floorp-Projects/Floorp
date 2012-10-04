/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>



#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "system_wrappers/interface/scoped_refptr.h"
#include "system_wrappers/interface/sleep.h"
#include "system_wrappers/interface/tick_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "modules/utility/interface/process_thread.h"
#include "modules/video_capture/main/interface/video_capture.h"
#include "modules/video_capture/main/interface/video_capture_factory.h"

using webrtc::CriticalSectionWrapper;
using webrtc::CriticalSectionScoped;
using webrtc::scoped_ptr;
using webrtc::SleepMs;
using webrtc::TickTime;
using webrtc::VideoCaptureAlarm;
using webrtc::VideoCaptureCapability;
using webrtc::VideoCaptureDataCallback;
using webrtc::VideoCaptureFactory;
using webrtc::VideoCaptureFeedBack;
using webrtc::VideoCaptureModule;


#define WAIT_(ex, timeout, res) \
  do { \
    res = (ex); \
    WebRtc_Word64 start = TickTime::MillisecondTimestamp(); \
    while (!res && TickTime::MillisecondTimestamp() < start + timeout) { \
      SleepMs(5); \
      res = (ex); \
    } \
  } while (0);\

#define EXPECT_TRUE_WAIT(ex, timeout) \
  do { \
    bool res; \
    WAIT_(ex, timeout, res); \
    if (!res) EXPECT_TRUE(ex); \
  } while (0);


static const int kTimeOut = 5000;
static const int kTestHeight = 288;
static const int kTestWidth = 352;
static const int kTestFramerate = 30;

// Compares the content of two video frames.
static bool CompareFrames(const webrtc::VideoFrame& frame1,
                          const webrtc::VideoFrame& frame2) {
  bool result =
      (frame1.Length() == frame2.Length()) &&
      (frame1.Width() == frame2.Width()) &&
      (frame1.Height() == frame2.Height());

  for (unsigned int i = 0; i < frame1.Length() && result; ++i)
    result = (*(frame1.Buffer()+i) == *(frame2.Buffer()+i));
  return result;
}

// Compares the content of a I420 frame in planar form and video frame.
static bool CompareFrames(const webrtc::VideoFrameI420& frame1,
                          const webrtc::VideoFrame& frame2) {
  if (frame1.width != frame2.Width() ||
      frame1.height != frame2.Height()) {
      return false;
  }

  // Compare Y
  unsigned char* y_plane = frame1.y_plane;
  for (unsigned int i = 0; i < frame2.Height(); ++i) {
    for (unsigned int j = 0; j < frame2.Width(); ++j) {
      if (*y_plane != *(frame2.Buffer()+i*frame2.Width() +j))
        return false;
      ++y_plane;
    }
    y_plane += frame1.y_pitch - frame1.width;
  }

  // Compare U
  unsigned char* u_plane = frame1.u_plane;
  for (unsigned int i = 0; i < frame2.Height() /2; ++i) {
    for (unsigned int j = 0; j < frame2.Width() /2; ++j) {
      if (*u_plane !=*(
          frame2.Buffer()+frame2.Width() * frame2.Height() +
          i*frame2.Width() / 2 + j)) {
        return false;
      }
      ++u_plane;
    }
    u_plane += frame1.u_pitch - frame1.width / 2;
  }

  // Compare V
  unsigned char* v_plane = frame1.v_plane;
  for (unsigned int i = 0; i < frame2.Height() /2; ++i) {
    for (unsigned int j = 0; j < frame2.Width() /2; ++j) {
      if (*v_plane != *(
          frame2.Buffer()+frame2.Width() * frame2.Height()* 5 / 4 +
          i*frame2.Width() / 2 + j)) {
        return false;
      }
      ++v_plane;
    }
    v_plane += frame1.v_pitch - frame1.width / 2;
  }
  return true;
}


class TestVideoCaptureCallback : public VideoCaptureDataCallback {
 public:
  TestVideoCaptureCallback()
    : capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      capture_delay_(0),
      last_render_time_ms_(0),
      incoming_frames_(0),
      timing_warnings_(0) {
  }

  ~TestVideoCaptureCallback() {
    if (timing_warnings_ > 0)
      printf("No of timing warnings %d\n", timing_warnings_);
  }

  virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                       webrtc::VideoFrame& videoFrame,
                                       webrtc::VideoCodecType codecType) {
    CriticalSectionScoped cs(capture_cs_.get());

    int height = static_cast<int>(videoFrame.Height());
    int width = static_cast<int>(videoFrame.Width());
    EXPECT_EQ(height, capability_.height);
    EXPECT_EQ(width, capability_.width);
    // RenderTimstamp should be the time now.
    EXPECT_TRUE(
        videoFrame.RenderTimeMs() >= TickTime::MillisecondTimestamp()-30 &&
        videoFrame.RenderTimeMs() <= TickTime::MillisecondTimestamp());

    if ((videoFrame.RenderTimeMs() >
            last_render_time_ms_ + (1000 * 1.1) / capability_.maxFPS &&
            last_render_time_ms_ > 0) ||
        (videoFrame.RenderTimeMs() <
            last_render_time_ms_ + (1000 * 0.9) / capability_.maxFPS &&
            last_render_time_ms_ > 0)) {
      timing_warnings_++;
    }

    incoming_frames_++;
    last_render_time_ms_ = videoFrame.RenderTimeMs();
    last_frame_.CopyFrame(videoFrame);
  }

  virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                     const WebRtc_Word32 delay) {
    CriticalSectionScoped cs(capture_cs_.get());
    capture_delay_ = delay;
  }

  void SetExpectedCapability(VideoCaptureCapability capability) {
    CriticalSectionScoped cs(capture_cs_.get());
    capability_= capability;
    incoming_frames_ = 0;
    last_render_time_ms_ = 0;
    capture_delay_ = 0;
  }
  int incoming_frames() {
    CriticalSectionScoped cs(capture_cs_.get());
    return incoming_frames_;
  }

  int capture_delay() {
    CriticalSectionScoped cs(capture_cs_.get());
    return capture_delay_;
  }
  int timing_warnings() {
    CriticalSectionScoped cs(capture_cs_.get());
    return timing_warnings_;
  }
  VideoCaptureCapability capability() {
    CriticalSectionScoped cs(capture_cs_.get());
    return capability_;
  }

  bool CompareLastFrame(const webrtc::VideoFrame& frame) {
    CriticalSectionScoped cs(capture_cs_.get());
    return CompareFrames(last_frame_, frame);
  }

  bool CompareLastFrame(const webrtc::VideoFrameI420& frame) {
    CriticalSectionScoped cs(capture_cs_.get());
    return CompareFrames(frame, last_frame_);
  }

 private:
  scoped_ptr<CriticalSectionWrapper> capture_cs_;
  VideoCaptureCapability capability_;
  int capture_delay_;
  WebRtc_Word64 last_render_time_ms_;
  int incoming_frames_;
  int timing_warnings_;
  webrtc::VideoFrame last_frame_;
};

class TestVideoCaptureFeedBack : public VideoCaptureFeedBack {
 public:
  TestVideoCaptureFeedBack() :
    capture_cs_(CriticalSectionWrapper::CreateCriticalSection()),
    frame_rate_(0),
    alarm_(webrtc::Cleared) {
  }

  virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 frameRate) {
    CriticalSectionScoped cs(capture_cs_.get());
    frame_rate_ = frameRate;
  }

  virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                const VideoCaptureAlarm reported_alarm) {
    CriticalSectionScoped cs(capture_cs_.get());
    alarm_ = reported_alarm;
  }
  int frame_rate() {
    CriticalSectionScoped cs(capture_cs_.get());
    return frame_rate_;

  }
  VideoCaptureAlarm alarm() {
    CriticalSectionScoped cs(capture_cs_.get());
    return alarm_;
  }

 private:
  scoped_ptr<CriticalSectionWrapper> capture_cs_;
  unsigned int frame_rate_;
  VideoCaptureAlarm alarm_;
};

class VideoCaptureTest : public testing::Test {
 public:
  VideoCaptureTest() : number_of_devices_(0) {}

  void SetUp() {
    device_info_.reset(VideoCaptureFactory::CreateDeviceInfo(5));
    number_of_devices_ = device_info_->NumberOfDevices();
    ASSERT_GT(number_of_devices_, 0u);
  }

  webrtc::scoped_refptr<VideoCaptureModule> OpenVideoCaptureDevice(
      unsigned int device,
      VideoCaptureDataCallback* callback) {
    char device_name[256];
    char unique_name[256];

    EXPECT_EQ(0, device_info_->GetDeviceName(
        device, device_name, 256, unique_name, 256));

    webrtc::scoped_refptr<VideoCaptureModule> module(
        VideoCaptureFactory::Create(device, unique_name));
    if (module.get() == NULL)
      return NULL;

    EXPECT_FALSE(module->CaptureStarted());

    EXPECT_EQ(0, module->RegisterCaptureDataCallback(*callback));
    return module;
  }

  void StartCapture(VideoCaptureModule* capture_module,
                    VideoCaptureCapability capability) {
    EXPECT_EQ(0, capture_module->StartCapture(capability));
    EXPECT_TRUE(capture_module->CaptureStarted());

    VideoCaptureCapability resulting_capability;
    EXPECT_EQ(0, capture_module->CaptureSettings(resulting_capability));
    EXPECT_EQ(capability.width, resulting_capability.width);
    EXPECT_EQ(capability.height, resulting_capability.height);
  }

  scoped_ptr<VideoCaptureModule::DeviceInfo> device_info_;
  unsigned int number_of_devices_;
};

TEST_F(VideoCaptureTest, CreateDelete) {
  for (int i = 0; i < 5; ++i) {
    WebRtc_Word64 start_time = TickTime::MillisecondTimestamp();
    TestVideoCaptureCallback capture_observer;
    webrtc::scoped_refptr<VideoCaptureModule> module(OpenVideoCaptureDevice(
        0, &capture_observer));
    ASSERT_TRUE(module.get() != NULL);

    VideoCaptureCapability capability;
#ifndef WEBRTC_MAC
    device_info_->GetCapability(module->CurrentDeviceName(), 0, capability);
#else
    capability.width = kTestWidth;
    capability.height = kTestHeight;
    capability.maxFPS = kTestFramerate;
    capability.rawType = webrtc::kVideoUnknown;
#endif
    capture_observer.SetExpectedCapability(capability);
    StartCapture(module.get(), capability);

    // Less than 4s to start the camera.
    EXPECT_LE(TickTime::MillisecondTimestamp() - start_time, 4000);

    // Make sure 5 frames are captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames() >= 5, kTimeOut);

    EXPECT_GT(capture_observer.capture_delay(), 0);

    WebRtc_Word64 stop_time = TickTime::MillisecondTimestamp();
    EXPECT_EQ(0, module->StopCapture());
    EXPECT_FALSE(module->CaptureStarted());

    // Less than 3s to stop the camera.
    EXPECT_LE(TickTime::MillisecondTimestamp() - stop_time, 3000);
  }
}

TEST_F(VideoCaptureTest, Capabilities) {
#ifdef WEBRTC_MAC
  printf("Video capture capabilities are not supported on Mac.\n");
  return;
#endif

  TestVideoCaptureCallback capture_observer;

  webrtc::scoped_refptr<VideoCaptureModule> module(OpenVideoCaptureDevice(
          0, &capture_observer));
  ASSERT_TRUE(module.get() != NULL);

  int number_of_capabilities = device_info_->NumberOfCapabilities(
      module->CurrentDeviceName());
  EXPECT_GT(number_of_capabilities, 0);
  for (int i = 0; i < number_of_capabilities; ++i) {
    VideoCaptureCapability capability;
    EXPECT_EQ(0, device_info_->GetCapability(module->CurrentDeviceName(), i,
                                             capability));
    capture_observer.SetExpectedCapability(capability);
    StartCapture(module.get(), capability);
    // Make sure 5 frames are captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames() >= 5, kTimeOut);

    EXPECT_EQ(0, module->StopCapture());
  }
}

// NOTE: flaky, crashes sometimes.
// http://code.google.com/p/webrtc/issues/detail?id=777
TEST_F(VideoCaptureTest, DISABLED_TestTwoCameras) {
  if (number_of_devices_ < 2) {
    printf("There are not two cameras available. Aborting test. \n");
    return;
  }

  TestVideoCaptureCallback capture_observer1;
  webrtc::scoped_refptr<VideoCaptureModule> module1(OpenVideoCaptureDevice(
          0, &capture_observer1));
  ASSERT_TRUE(module1.get() != NULL);
  VideoCaptureCapability capability1;
#ifndef WEBRTC_MAC
  device_info_->GetCapability(module1->CurrentDeviceName(), 0, capability1);
#else
  capability1.width = kTestWidth;
  capability1.height = kTestHeight;
  capability1.maxFPS = kTestFramerate;
  capability1.rawType = webrtc::kVideoUnknown;
#endif
  capture_observer1.SetExpectedCapability(capability1);

  TestVideoCaptureCallback capture_observer2;
  webrtc::scoped_refptr<VideoCaptureModule> module2(OpenVideoCaptureDevice(
          1, &capture_observer2));
  ASSERT_TRUE(module1.get() != NULL);


  VideoCaptureCapability capability2;
#ifndef WEBRTC_MAC
  device_info_->GetCapability(module2->CurrentDeviceName(), 0, capability2);
#else
  capability2.width = kTestWidth;
  capability2.height = kTestHeight;
  capability2.maxFPS = kTestFramerate;
  capability2.rawType = webrtc::kVideoUnknown;
#endif
  capture_observer2.SetExpectedCapability(capability2);

  StartCapture(module1.get(), capability1);
  StartCapture(module2.get(), capability2);
  EXPECT_TRUE_WAIT(capture_observer1.incoming_frames() >= 5, kTimeOut);
  EXPECT_TRUE_WAIT(capture_observer2.incoming_frames() >= 5, kTimeOut);
}

// Test class for testing external capture and capture feedback information
// such as frame rate and picture alarm.
class VideoCaptureExternalTest : public testing::Test {
 public:
  void SetUp() {
    capture_module_ = VideoCaptureFactory::Create(0, capture_input_interface_);
    process_module_ = webrtc::ProcessThread::CreateProcessThread();
    process_module_->Start();
    process_module_->RegisterModule(capture_module_);

    VideoCaptureCapability capability;
    capability.width = kTestWidth;
    capability.height = kTestHeight;
    capability.rawType = webrtc::kVideoYV12;
    capability.maxFPS = kTestFramerate;
    capture_callback_.SetExpectedCapability(capability);

    test_frame_.VerifyAndAllocate(kTestWidth * kTestHeight * 3 / 2);
    test_frame_.SetLength(kTestWidth * kTestHeight * 3 / 2);
    test_frame_.SetHeight(kTestHeight);
    test_frame_.SetWidth(kTestWidth);
    SleepMs(1); // Wait 1ms so that two tests can't have the same timestamp.
    memset(test_frame_.Buffer(), 127, test_frame_.Length());

    EXPECT_EQ(0, capture_module_->RegisterCaptureDataCallback(
        capture_callback_));
    EXPECT_EQ(0, capture_module_->RegisterCaptureCallback(capture_feedback_));
    EXPECT_EQ(0, capture_module_->EnableFrameRateCallback(true));
    EXPECT_EQ(0, capture_module_->EnableNoPictureAlarm(true));
  }

  void TearDown() {
    process_module_->Stop();
    webrtc::ProcessThread::DestroyProcessThread(process_module_);
  }

  webrtc::VideoCaptureExternal* capture_input_interface_;
  webrtc::scoped_refptr<VideoCaptureModule> capture_module_;
  webrtc::ProcessThread* process_module_;
  webrtc::VideoFrame test_frame_;
  TestVideoCaptureCallback capture_callback_;
  TestVideoCaptureFeedBack capture_feedback_;
};

// Test input of external video frames.
TEST_F(VideoCaptureExternalTest , TestExternalCapture) {
  EXPECT_EQ(0, capture_input_interface_->IncomingFrame(
      test_frame_.Buffer(), test_frame_.Length(),
      capture_callback_.capability(), 0));
  EXPECT_TRUE(capture_callback_.CompareLastFrame(test_frame_));
}

// Test input of planar I420 frames.
// NOTE: flaky, sometimes fails on the last CompareLastFrame.
// http://code.google.com/p/webrtc/issues/detail?id=777
TEST_F(VideoCaptureExternalTest, DISABLED_TestExternalCaptureI420) {
  webrtc::VideoFrameI420 frame_i420;
  frame_i420.width = kTestWidth;
  frame_i420.height = kTestHeight;
  frame_i420.y_plane = test_frame_.Buffer();
  frame_i420.u_plane = frame_i420.y_plane + (kTestWidth * kTestHeight);
  frame_i420.v_plane = frame_i420.u_plane + ((kTestWidth * kTestHeight) >> 2);
  frame_i420.y_pitch = kTestWidth;
  frame_i420.u_pitch = kTestWidth / 2;
  frame_i420.v_pitch = kTestWidth / 2;
  EXPECT_EQ(0, capture_input_interface_->IncomingFrameI420(frame_i420, 0));
  EXPECT_TRUE(capture_callback_.CompareLastFrame(frame_i420));

  // Test with a frame with pitch not equal to width
  memset(test_frame_.Buffer(), 0xAA, test_frame_.Length());
  webrtc::VideoFrame aligned_test_frame;
  int y_pitch = kTestWidth + 2;
  int u_pitch = kTestWidth / 2 + 1;
  int v_pitch = u_pitch;
  aligned_test_frame.VerifyAndAllocate(kTestHeight * y_pitch +
                                       (kTestHeight / 2) * u_pitch +
                                       (kTestHeight / 2) * v_pitch);
  aligned_test_frame.SetLength(aligned_test_frame.Size());
  memset(aligned_test_frame.Buffer(), 0, aligned_test_frame.Length());
  // Copy the test_frame_ to aligned_test_frame.
  int y_width = kTestWidth;
  int uv_width = kTestWidth / 2;
  int y_rows = kTestHeight;
  int uv_rows = kTestHeight / 2;
  unsigned char* current_pointer = aligned_test_frame.Buffer();
  unsigned char* y_plane = test_frame_.Buffer();
  unsigned char* u_plane = y_plane + kTestWidth * kTestHeight;
  unsigned char* v_plane = u_plane + ((kTestWidth * kTestHeight) >> 2);
  // Copy Y
  for (int i = 0; i < y_rows; ++i) {
    memcpy(current_pointer, y_plane, y_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += y_pitch;
    y_plane += y_width;
  }
  // Copy U
  for (int i = 0; i < uv_rows; ++i) {
    memcpy(current_pointer, u_plane, uv_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += u_pitch;
    u_plane += uv_width;
  }
  // Copy V
  for (int i = 0; i < uv_rows; ++i) {
    memcpy(current_pointer, v_plane, uv_width);
    // Remove the alignment which ViE doesn't support.
    current_pointer += v_pitch;
    v_plane += uv_width;
  }
  frame_i420.width = kTestWidth;
  frame_i420.height = kTestHeight;
  frame_i420.y_plane = aligned_test_frame.Buffer();
  frame_i420.u_plane = frame_i420.y_plane + (y_pitch * y_rows);
  frame_i420.v_plane = frame_i420.u_plane + (u_pitch * uv_rows);
  frame_i420.y_pitch = y_pitch;
  frame_i420.u_pitch = u_pitch;
  frame_i420.v_pitch = v_pitch;

  EXPECT_EQ(0, capture_input_interface_->IncomingFrameI420(frame_i420, 0));
  EXPECT_TRUE(capture_callback_.CompareLastFrame(test_frame_));
}

// Test frame rate and no picture alarm.
TEST_F(VideoCaptureExternalTest , FrameRate) {
  WebRtc_Word64 testTime = 3;
  TickTime startTime = TickTime::Now();

  while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000) {
    EXPECT_EQ(0, capture_input_interface_->IncomingFrame(
        test_frame_.Buffer(), test_frame_.Length(),
        capture_callback_.capability(), 0));
    SleepMs(100);
  }
  EXPECT_TRUE(capture_feedback_.frame_rate() >= 8 &&
              capture_feedback_.frame_rate() <= 10);
  SleepMs(500);
  EXPECT_EQ(webrtc::Raised, capture_feedback_.alarm());

  startTime = TickTime::Now();
  while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000) {
    EXPECT_EQ(0, capture_input_interface_->IncomingFrame(
        test_frame_.Buffer(), test_frame_.Length(),
        capture_callback_.capability(), 0));
    SleepMs(1000 / 30);
  }
  EXPECT_EQ(webrtc::Cleared, capture_feedback_.alarm());
  // Frame rate might be less than 33 since we have paused providing
  // frames for a while.
  EXPECT_TRUE(capture_feedback_.frame_rate() >= 25 &&
              capture_feedback_.frame_rate() <= 33);
}

// Test start image
TEST_F(VideoCaptureExternalTest , StartImage) {
  EXPECT_EQ(0, capture_module_->StartSendImage(
      test_frame_, 10));

  EXPECT_TRUE_WAIT(capture_callback_.incoming_frames() == 5, kTimeOut);
  EXPECT_EQ(0, capture_module_->StopSendImage());

  SleepMs(200);
  // Test that no more start images have arrived.
  EXPECT_TRUE(capture_callback_.incoming_frames() >= 4 &&
              capture_callback_.incoming_frames() <= 5);
  EXPECT_TRUE(capture_callback_.CompareLastFrame(test_frame_));
}

