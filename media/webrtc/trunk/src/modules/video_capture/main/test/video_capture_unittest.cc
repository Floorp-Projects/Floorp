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

#include "gtest/gtest.h"
#include "process_thread.h"
#include "scoped_ptr.h"
#include "scoped_refptr.h"
#include "tick_util.h"
#include "video_capture.h"
#include "video_capture_factory.h"

using webrtc::TickTime;
using webrtc::VideoCaptureAlarm;
using webrtc::VideoCaptureCapability;
using webrtc::VideoCaptureDataCallback;
using webrtc::VideoCaptureFactory;
using webrtc::VideoCaptureFeedBack;
using webrtc::VideoCaptureModule;

#if defined(_WIN32)
#define SLEEP(x) Sleep(x)
#elif defined(WEBRTC_ANDROID)
#define SLEEP(x) usleep(x*1000)
#else
#include <unistd.h>
#define SLEEP(x) usleep(x * 1000)
#endif

#define WAIT_(ex, timeout, res) \
  do { \
    res = (ex); \
    WebRtc_Word64 start = TickTime::MillisecondTimestamp(); \
    while (!res && TickTime::MillisecondTimestamp() < start + timeout) { \
      SLEEP(5); \
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
    : capture_delay(0),
      last_render_time_ms(0),
      incoming_frames(0),
      timing_warnings(0) {
  }

  ~TestVideoCaptureCallback() {
    if (timing_warnings > 0)
      printf("No of timing warnings %d\n", timing_warnings);
  }

  virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                       webrtc::VideoFrame& videoFrame,
                                       webrtc::VideoCodecType codecType) {
    int height = static_cast<int>(videoFrame.Height());
    int width = static_cast<int>(videoFrame.Width());
    EXPECT_EQ(height, capability.height);
    EXPECT_EQ(width, capability.width);
    // RenderTimstamp should be the time now.
    EXPECT_TRUE(
        videoFrame.RenderTimeMs() >= TickTime::MillisecondTimestamp()-30 &&
        videoFrame.RenderTimeMs() <= TickTime::MillisecondTimestamp());

    if ((videoFrame.RenderTimeMs() >
            last_render_time_ms + (1000 * 1.1) / capability.maxFPS &&
            last_render_time_ms > 0) ||
        (videoFrame.RenderTimeMs() <
            last_render_time_ms + (1000 * 0.9) / capability.maxFPS &&
            last_render_time_ms > 0)) {
      timing_warnings++;
    }

    incoming_frames++;
    last_render_time_ms = videoFrame.RenderTimeMs();
    last_frame.CopyFrame(videoFrame);
  }

  virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                     const WebRtc_Word32 delay) {
    capture_delay = delay;
  }

  VideoCaptureCapability capability;
  int capture_delay;
  WebRtc_Word64 last_render_time_ms;
  int incoming_frames;
  int timing_warnings;
  webrtc::VideoFrame last_frame;
};

class TestVideoCaptureFeedBack : public VideoCaptureFeedBack {
 public:
  TestVideoCaptureFeedBack() : frame_rate(0), alarm(webrtc::Cleared) {}

  virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                  const WebRtc_UWord32 frameRate) {
    frame_rate = frameRate;
  }

  virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                const VideoCaptureAlarm reported_alarm) {
    alarm = reported_alarm;
  }
  unsigned int frame_rate;
  VideoCaptureAlarm alarm;
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
    EXPECT_EQ(capability, resulting_capability);
  }

  webrtc::scoped_ptr<VideoCaptureModule::DeviceInfo> device_info_;
  unsigned int number_of_devices_;
};

TEST_F(VideoCaptureTest, CreateDelete) {
  for (int i = 0; i < 5; ++i) {
    WebRtc_Word64 start_time = TickTime::MillisecondTimestamp();
    TestVideoCaptureCallback capture_observer;
    webrtc::scoped_refptr<VideoCaptureModule> module(OpenVideoCaptureDevice(
        0, &capture_observer));
    ASSERT_TRUE(module.get() != NULL);

#ifndef WEBRTC_MAC
    device_info_->GetCapability(module->CurrentDeviceName(), 0,
                                capture_observer.capability);
#else
    capture_observer.capability.width = kTestWidth;
    capture_observer.capability.height = kTestHeight;
    capture_observer.capability.maxFPS = kTestFramerate;
    capture_observer.capability.rawType = webrtc::kVideoUnknown;
#endif

    StartCapture(module.get(), capture_observer.capability);

    // Less than 4s to start the camera.
    EXPECT_LE(TickTime::MillisecondTimestamp() - start_time, 4000);

    // Make sure 5 frames are captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames >= 5, kTimeOut);

    EXPECT_GT(capture_observer.capture_delay, 0);

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
    device_info_->GetCapability(module->CurrentDeviceName(), i,
                                capture_observer.capability);
    StartCapture(module.get(), capture_observer.capability);
    // Make sure 5 frames are captured.
    EXPECT_TRUE_WAIT(capture_observer.incoming_frames >= 5, kTimeOut);
    capture_observer.incoming_frames = 0;
    EXPECT_EQ(0, module->StopCapture());
  }
}

TEST_F(VideoCaptureTest, TestTwoCameras) {
  if (number_of_devices_ < 2) {
    printf("There are not two cameras available. Aborting test. \n");
    return;
  }

  TestVideoCaptureCallback capture_observer1;
  webrtc::scoped_refptr<VideoCaptureModule> module1(OpenVideoCaptureDevice(
          0, &capture_observer1));
  ASSERT_TRUE(module1.get() != NULL);

#ifndef WEBRTC_MAC
  device_info_->GetCapability(module1->CurrentDeviceName(), 0,
                              capture_observer1.capability);
#else
  capture_observer1.capability.width = kTestWidth;
  capture_observer1.capability.height = kTestHeight;
  capture_observer1.capability.maxFPS = kTestFramerate;
  capture_observer1.capability.rawType = webrtc::kVideoUnknown;
#endif

  TestVideoCaptureCallback capture_observer2;
  webrtc::scoped_refptr<VideoCaptureModule> module2(OpenVideoCaptureDevice(
          1, &capture_observer2));
  ASSERT_TRUE(module1.get() != NULL);


#ifndef WEBRTC_MAC
  device_info_->GetCapability(module2->CurrentDeviceName(), 0,
                              capture_observer2.capability);
#else
  capture_observer2.capability.width = kTestWidth;
  capture_observer2.capability.height = kTestHeight;
  capture_observer2.capability.maxFPS = kTestFramerate;
  capture_observer2.capability.rawType = webrtc::kVideoUnknown;
#endif

  StartCapture(module1.get(), capture_observer1.capability);
  StartCapture(module2.get(), capture_observer2.capability);
  EXPECT_TRUE_WAIT(capture_observer1.incoming_frames >= 5, kTimeOut);
  EXPECT_TRUE_WAIT(capture_observer2.incoming_frames >= 5, kTimeOut);
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

    capture_callback_.capability.width = kTestWidth;
    capture_callback_.capability.height = kTestHeight;
    capture_callback_.capability.rawType = webrtc::kVideoYV12;
    capture_callback_.capability.maxFPS = kTestFramerate;

    test_frame_.VerifyAndAllocate(kTestWidth * kTestHeight * 3 / 2);
    test_frame_.SetLength(kTestWidth * kTestHeight * 3 / 2);
    test_frame_.SetHeight(kTestHeight);
    test_frame_.SetWidth(kTestWidth);
    SLEEP(1); // Wait 1ms so that two tests can't have the same timestamp.
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
      test_frame_.Buffer(), test_frame_.Length(), capture_callback_.capability,
      0));
  EXPECT_TRUE(CompareFrames(test_frame_, capture_callback_.last_frame));
}

// Test input of planar I420 frames.
TEST_F(VideoCaptureExternalTest , TestExternalCaptureI420) {
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

  EXPECT_TRUE(CompareFrames(frame_i420, capture_callback_.last_frame));
}

// Test frame rate and no picture alarm.
TEST_F(VideoCaptureExternalTest , FrameRate) {
  WebRtc_Word64 testTime = 3;
  TickTime startTime = TickTime::Now();
  capture_callback_.capability.maxFPS = 10;
  while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000) {
    EXPECT_EQ(0, capture_input_interface_->IncomingFrame(
        test_frame_.Buffer(), test_frame_.Length(),
        capture_callback_.capability, 0));
    SLEEP(1000 / capture_callback_.capability.maxFPS);
  }
  EXPECT_TRUE(capture_feedback_.frame_rate >= 8 &&
              capture_feedback_.frame_rate <= 10);
  SLEEP(500);
  EXPECT_EQ(webrtc::Raised, capture_feedback_.alarm);

  startTime = TickTime::Now();
  capture_callback_.capability.maxFPS = 30;
  while ((TickTime::Now() - startTime).Milliseconds() < testTime * 1000) {
    EXPECT_EQ(0, capture_input_interface_->IncomingFrame(
        test_frame_.Buffer(), test_frame_.Length(),
        capture_callback_.capability, 0));
    SLEEP(1000 / capture_callback_.capability.maxFPS);
  }
  EXPECT_EQ(webrtc::Cleared, capture_feedback_.alarm);
  // Frame rate might be less than 33 since we have paused providing
  // frames for a while.
  EXPECT_TRUE(capture_feedback_.frame_rate >= 25 &&
              capture_feedback_.frame_rate <= 33);
}

// Test start image
TEST_F(VideoCaptureExternalTest , StartImage) {
  capture_callback_.capability.maxFPS = 10;
  EXPECT_EQ(0, capture_module_->StartSendImage(
      test_frame_, capture_callback_.capability.maxFPS));

  EXPECT_TRUE_WAIT(capture_callback_.incoming_frames == 5, kTimeOut);
  EXPECT_EQ(0, capture_module_->StopSendImage());

  SLEEP(200);
  // Test that no more start images have arrived.
  EXPECT_TRUE(capture_callback_.incoming_frames >= 4 &&
              capture_callback_.incoming_frames <= 5);
  EXPECT_TRUE(CompareFrames(test_frame_, capture_callback_.last_frame));
}

