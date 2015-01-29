/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gflags/gflags.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"
#include "webrtc/voice_engine/include/voe_base.h"

DEFINE_bool(capture_test_ensure_resolution_alignment_in_capture_device, true,
            "If true, we will give resolutions slightly below a reasonable "
            "value to test the camera's ability to choose a good resolution. "
            "If false, we will provide reasonable resolutions instead.");

class CaptureObserver : public webrtc::ViECaptureObserver {
 public:
  CaptureObserver()
      : brightness_(webrtc::Normal),
        alarm_(webrtc::AlarmCleared),
        frame_rate_(0) {}

  virtual void BrightnessAlarm(const int capture_id,
                               const webrtc::Brightness brightness) {
    brightness_ = brightness;
    switch (brightness) {
      case webrtc::Normal:
        ViETest::Log("  BrightnessAlarm Normal");
        break;
      case webrtc::Bright:
        ViETest::Log("  BrightnessAlarm Bright");
        break;
      case webrtc::Dark:
        ViETest::Log("  BrightnessAlarm Dark");
        break;
    }
  }

  virtual void CapturedFrameRate(const int capture_id,
                                 const unsigned char frame_rate) {
    ViETest::Log("  CapturedFrameRate %u", frame_rate);
    frame_rate_ = frame_rate;
  }

  virtual void NoPictureAlarm(const int capture_id,
                              const webrtc::CaptureAlarm alarm) {
    alarm_ = alarm;
    if (alarm == webrtc::AlarmRaised) {
      ViETest::Log("NoPictureAlarm CARaised.");
    } else {
      ViETest::Log("NoPictureAlarm CACleared.");
    }
  }

  webrtc::Brightness brightness_;
  webrtc::CaptureAlarm alarm_;
  unsigned char frame_rate_;
};

class CaptureEffectFilter : public webrtc::ViEEffectFilter {
 public:
  CaptureEffectFilter(unsigned int expected_width, unsigned int expected_height)
      : number_of_captured_frames_(0),
        expected_width_(expected_width),
        expected_height_(expected_height) {
  }

  // Implements video_engineEffectFilter.
  virtual int Transform(int size,
                        unsigned char* frame_buffer,
                        int64_t ntp_time_ms,
                        unsigned int timestamp,
                        unsigned int width,
                        unsigned int height) {
    EXPECT_TRUE(frame_buffer != NULL);
    EXPECT_EQ(expected_width_, width);
    EXPECT_EQ(expected_height_, height);
    ++number_of_captured_frames_;
    return 0;
  }

  int number_of_captured_frames_;

 protected:
  unsigned int expected_width_;
  unsigned int expected_height_;
};

void ViEAutoTest::ViECaptureStandardTest() {
  /// **************************************************************
  //  Begin create/initialize WebRTC Video Engine for testing
  /// **************************************************************

  /// **************************************************************
  //  Engine ready. Begin testing class
  /// **************************************************************

  TbInterfaces video_engine("video_engineCaptureStandardTest");

  webrtc::VideoCaptureModule::DeviceInfo* dev_info =
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
  ASSERT_TRUE(dev_info != NULL);

  int number_of_capture_devices = dev_info->NumberOfDevices();
  ViETest::Log("Number of capture devices %d",
                        number_of_capture_devices);
  ASSERT_GT(number_of_capture_devices, 0)
      << "This test requires a capture device (i.e. a webcam)";

#if !defined(WEBRTC_MAC)
  int capture_device_id[10] = {0};
  webrtc::VideoCaptureModule* vcpms[10] = {0};
#endif

  // Check capabilities
  for (int device_index = 0; device_index < number_of_capture_devices;
       ++device_index) {
    char device_name[128];
    char device_unique_name[512];

    EXPECT_EQ(0, dev_info->GetDeviceName(device_index,
                                         device_name,
                                         sizeof(device_name),
                                         device_unique_name,
                                         sizeof(device_unique_name)));
    ViETest::Log("Found capture device %s\nUnique name %s",
                          device_name, device_unique_name);

#if !defined(WEBRTC_MAC)  // these functions will return -1
    int number_of_capabilities =
        dev_info->NumberOfCapabilities(device_unique_name);
    EXPECT_GT(number_of_capabilities, 0);

    for (int cap_index = 0; cap_index < number_of_capabilities; ++cap_index) {
      webrtc::VideoCaptureCapability capability;
      EXPECT_EQ(0, dev_info->GetCapability(device_unique_name, cap_index,
                                           capability));
      ViETest::Log("Capture capability %d (of %u)", cap_index + 1,
                   number_of_capabilities);
      ViETest::Log("width %d, height %d, frame rate %d",
                   capability.width, capability.height, capability.maxFPS);
      ViETest::Log("expected delay %d, color type %d, encoding %d",
                   capability.expectedCaptureDelay, capability.rawType,
                   capability.codecType);
      EXPECT_GT(capability.width, 0);
      EXPECT_GT(capability.height, 0);
      EXPECT_GT(capability.maxFPS, -1);  // >= 0
      EXPECT_GT(capability.expectedCaptureDelay, 0);
    }
#endif
  }
  // Capture Capability Functions are not supported on WEBRTC_MAC.
#if !defined(WEBRTC_MAC)

  // Check allocation. Try to allocate them all after each other.
  for (int device_index = 0; device_index < number_of_capture_devices;
       ++device_index) {
    char device_name[128];
    char device_unique_name[512];
    EXPECT_EQ(0, dev_info->GetDeviceName(device_index,
                                         device_name,
                                         sizeof(device_name),
                                         device_unique_name,
                                         sizeof(device_unique_name)));
    webrtc::VideoCaptureModule* vcpm =
        webrtc::VideoCaptureFactory::Create(device_index, device_unique_name);
    EXPECT_TRUE(vcpm != NULL);
    if (!vcpm)
      continue;

    vcpm->AddRef();
    vcpms[device_index] = vcpm;

    EXPECT_EQ(0, video_engine.capture->AllocateCaptureDevice(
        *vcpm, capture_device_id[device_index]));

    webrtc::VideoCaptureCapability capability;
    EXPECT_EQ(0, dev_info->GetCapability(device_unique_name, 0, capability));

    // Test that the camera select the closest capability to the selected
    // width and height.
    CaptureEffectFilter filter(capability.width, capability.height);
    EXPECT_EQ(0, video_engine.image_process->RegisterCaptureEffectFilter(
        capture_device_id[device_index], filter));

    ViETest::Log("Testing Device %s capability width %d  height %d",
                 device_unique_name, capability.width, capability.height);

    if (FLAGS_capture_test_ensure_resolution_alignment_in_capture_device) {
      // This tests that the capture device properly aligns to a
      // multiple of 16 (or at least 8).
      capability.height = capability.height - 2;
      capability.width  = capability.width  - 2;
    }

    webrtc::CaptureCapability vie_capability;
    vie_capability.width = capability.width;
    vie_capability.height = capability.height;
    vie_capability.codecType = capability.codecType;
    vie_capability.maxFPS = capability.maxFPS;
    vie_capability.rawType = capability.rawType;

    EXPECT_EQ(0, video_engine.capture->StartCapture(
        capture_device_id[device_index], vie_capability));
    webrtc::TickTime start_time = webrtc::TickTime::Now();

    while (filter.number_of_captured_frames_ < 10 &&
           (webrtc::TickTime::Now() - start_time).Milliseconds() < 10000) {
      AutoTestSleep(100);
    }

    EXPECT_GT(filter.number_of_captured_frames_, 9)
        << "Should capture at least some frames";

    EXPECT_EQ(0, video_engine.image_process->DeregisterCaptureEffectFilter(
        capture_device_id[device_index]));

#ifdef WEBRTC_ANDROID  // Can only allocate one camera at the time on Android.
    EXPECT_EQ(0, video_engine.capture->StopCapture(
        capture_device_id[device_index]));
    EXPECT_EQ(0, video_engine.capture->ReleaseCaptureDevice(
        capture_device_id[device_index]));
#endif
  }

  /// **************************************************************
  //  Testing finished. Tear down Video Engine
  /// **************************************************************
  delete dev_info;

  // Stop all started capture devices.
  for (int device_index = 0; device_index < number_of_capture_devices;
       ++device_index) {
#if !defined(WEBRTC_ANDROID)
    // Don't stop on Android since we can only allocate one camera.
    EXPECT_EQ(0, video_engine.capture->StopCapture(
        capture_device_id[device_index]));
    EXPECT_EQ(0, video_engine.capture->ReleaseCaptureDevice(
        capture_device_id[device_index]));
#endif  // !WEBRTC_ANDROID
    if (vcpms[device_index])
      vcpms[device_index]->Release();
  }
#endif  // !WEBRTC_MAC
}

void ViEAutoTest::ViECaptureExtendedTest() {
  ViECaptureExternalCaptureTest();
}

void ViEAutoTest::ViECaptureAPITest() {
  /// **************************************************************
  //  Begin create/initialize WebRTC Video Engine for testing
  /// **************************************************************

  /// **************************************************************
  //  Engine ready. Begin testing class
  /// **************************************************************
  TbInterfaces video_engine("video_engineCaptureAPITest");

  video_engine.capture->NumberOfCaptureDevices();

  char device_name[128];
  char device_unique_name[512];
  int capture_id = 0;

  webrtc::VideoCaptureModule::DeviceInfo* dev_info =
      webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
  ASSERT_TRUE(dev_info != NULL);
  ASSERT_GT(dev_info->NumberOfDevices(), 0u)
      << "This test requires a capture device (i.e. a webcam)";

  // Get the first capture device
  EXPECT_EQ(0, dev_info->GetDeviceName(0, device_name,
                                       sizeof(device_name),
                                       device_unique_name,
                                       sizeof(device_unique_name)));

  webrtc::VideoCaptureModule* vcpm =
      webrtc::VideoCaptureFactory::Create(0, device_unique_name);
  vcpm->AddRef();
  EXPECT_TRUE(vcpm != NULL);

  // Allocate capture device.
  EXPECT_EQ(0, video_engine.capture->AllocateCaptureDevice(*vcpm, capture_id));

  // Start the capture device.
  EXPECT_EQ(0, video_engine.capture->StartCapture(capture_id));

  // Start again. Should fail.
  EXPECT_NE(0, video_engine.capture->StartCapture(capture_id));
  EXPECT_EQ(kViECaptureDeviceAlreadyStarted, video_engine.LastError());

  // Start invalid capture device.
  EXPECT_NE(0, video_engine.capture->StartCapture(capture_id + 1));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Stop invalid capture device.
  EXPECT_NE(0, video_engine.capture->StopCapture(capture_id + 1));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Stop the capture device.
  EXPECT_EQ(0, video_engine.capture->StopCapture(capture_id));

  // Stop the capture device again.
  EXPECT_NE(0, video_engine.capture->StopCapture(capture_id));
  EXPECT_EQ(kViECaptureDeviceNotStarted, video_engine.LastError());

  // Connect to invalid channel.
  EXPECT_NE(0, video_engine.capture->ConnectCaptureDevice(capture_id, 0));
  EXPECT_EQ(kViECaptureDeviceInvalidChannelId,
            video_engine.LastError());

  TbVideoChannel channel(video_engine);

  // Connect invalid capture_id.
  EXPECT_NE(0, video_engine.capture->ConnectCaptureDevice(capture_id + 1,
                                                 channel.videoChannel));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Connect the capture device to the channel.
  EXPECT_EQ(0, video_engine.capture->ConnectCaptureDevice(capture_id,
                                                 channel.videoChannel));

  // Connect the channel again.
  EXPECT_NE(0, video_engine.capture->ConnectCaptureDevice(capture_id,
                                                 channel.videoChannel));
  EXPECT_EQ(kViECaptureDeviceAlreadyConnected,
            video_engine.LastError());

  // Start the capture device.
  EXPECT_EQ(0, video_engine.capture->StartCapture(capture_id));

  // Release invalid capture device.
  EXPECT_NE(0, video_engine.capture->ReleaseCaptureDevice(capture_id + 1));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Release the capture device.
  EXPECT_EQ(0, video_engine.capture->ReleaseCaptureDevice(capture_id));

  // Release the capture device again.
  EXPECT_NE(0, video_engine.capture->ReleaseCaptureDevice(capture_id));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Test GetOrientation.
  webrtc::VideoCaptureRotation orientation;
  char dummy_name[5];
  EXPECT_NE(0, dev_info->GetOrientation(dummy_name, orientation));

  // Test SetRotation.
  EXPECT_NE(0, video_engine.capture->SetRotateCapturedFrames(
      capture_id, webrtc::RotateCapturedFrame_90));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());

  // Allocate capture device.
  EXPECT_EQ(0, video_engine.capture->AllocateCaptureDevice(*vcpm, capture_id));

  EXPECT_EQ(0, video_engine.capture->SetRotateCapturedFrames(
      capture_id, webrtc::RotateCapturedFrame_0));
  EXPECT_EQ(0, video_engine.capture->SetRotateCapturedFrames(
      capture_id, webrtc::RotateCapturedFrame_90));
  EXPECT_EQ(0, video_engine.capture->SetRotateCapturedFrames(
      capture_id, webrtc::RotateCapturedFrame_180));
  EXPECT_EQ(0, video_engine.capture->SetRotateCapturedFrames(
      capture_id, webrtc::RotateCapturedFrame_270));

  // Release the capture device
  EXPECT_EQ(0, video_engine.capture->ReleaseCaptureDevice(capture_id));

  /// **************************************************************
  //  Testing finished. Tear down Video Engine
  /// **************************************************************
  delete dev_info;
  vcpm->Release();
}

void ViEAutoTest::ViECaptureExternalCaptureTest() {
  /// **************************************************************
  //  Begin create/initialize WebRTC Video Engine for testing
  /// **************************************************************

  TbInterfaces video_engine("video_engineCaptureExternalCaptureTest");
  TbVideoChannel channel(video_engine);
  channel.StartReceive();
  channel.StartSend();

  webrtc::VideoCaptureExternal* external_capture = NULL;
  int capture_id = 0;

  // Allocate the external capture device.
  webrtc::VideoCaptureModule* vcpm =
      webrtc::VideoCaptureFactory::Create(0, external_capture);
  EXPECT_TRUE(vcpm != NULL);
  EXPECT_TRUE(external_capture != NULL);
  vcpm->AddRef();

  EXPECT_EQ(0, video_engine.capture->AllocateCaptureDevice(*vcpm, capture_id));

  // Connect the capture device to the channel.
  EXPECT_EQ(0, video_engine.capture->ConnectCaptureDevice(capture_id,
                                                 channel.videoChannel));

  // Render the local capture.
  EXPECT_EQ(0, video_engine.render->AddRenderer(capture_id, _window1, 1, 0.0,
                                                0.0, 1.0, 1.0));

  // Render the remote capture.
  EXPECT_EQ(0, video_engine.render->AddRenderer(channel.videoChannel, _window2,
                                                1, 0.0, 0.0, 1.0, 1.0));
  EXPECT_EQ(0, video_engine.render->StartRender(capture_id));
  EXPECT_EQ(0, video_engine.render->StartRender(channel.videoChannel));

  // Register observer.
  CaptureObserver observer;
  EXPECT_EQ(0, video_engine.capture->RegisterObserver(capture_id, observer));

  // Enable brightness alarm.
  EXPECT_EQ(0, video_engine.capture->EnableBrightnessAlarm(capture_id, true));

  CaptureEffectFilter effect_filter(176, 144);
  EXPECT_EQ(0, video_engine.image_process->RegisterCaptureEffectFilter(
      capture_id, effect_filter));

  // Call started.
  ViETest::Log("You should see local preview from external capture\n"
               "in window 1 and the remote video in window 2.\n");

  /// **************************************************************
  //  Engine ready. Begin testing class
  /// **************************************************************
  const unsigned int video_frame_length = (176 * 144 * 3) / 2;
  unsigned char* video_frame = new unsigned char[video_frame_length];
  memset(video_frame, 128, 176 * 144);

  int frame_count = 0;
  webrtc::VideoCaptureCapability capability;
  capability.width = 176;
  capability.height = 144;
  capability.rawType = webrtc::kVideoI420;

  ViETest::Log("Testing external capturing and frame rate callbacks.");
  // TODO(mflodman) Change when using a real file!
  // while (fread(video_frame, video_frame_length, 1, foreman) == 1)
  while (frame_count < 120) {
    external_capture->IncomingFrame(
        video_frame, video_frame_length, capability,
        webrtc::TickTime::MillisecondTimestamp());
    AutoTestSleep(33);

    if (effect_filter.number_of_captured_frames_ > 2) {
      EXPECT_EQ(webrtc::Normal, observer.brightness_) <<
          "Brightness or picture alarm should not have been called yet.";
      EXPECT_EQ(webrtc::AlarmCleared, observer.alarm_) <<
          "Brightness or picture alarm should not have been called yet.";
    }
    frame_count++;
  }

  // Test brightness alarm.
  // Test bright image.
  for (int i = 0; i < 176 * 144; ++i) {
    if (video_frame[i] <= 155)
      video_frame[i] = video_frame[i] + 100;
    else
      video_frame[i] = 255;
  }
  ViETest::Log("Testing Brighness alarm");
  for (int frame = 0; frame < 30; ++frame) {
    external_capture->IncomingFrame(
        video_frame, video_frame_length, capability,
        webrtc::TickTime::MillisecondTimestamp());
    AutoTestSleep(33);
  }
  EXPECT_EQ(webrtc::Bright, observer.brightness_) <<
      "Should be bright at this point since we are using a bright image.";

  // Test Dark image
  for (int i = 0; i < 176 * 144; ++i) {
    video_frame[i] = video_frame[i] > 200 ? video_frame[i] - 200 : 0;
  }
  for (int frame = 0; frame < 30; ++frame) {
    external_capture->IncomingFrame(
        video_frame, video_frame_length, capability,
        webrtc::TickTime::MillisecondTimestamp());
    AutoTestSleep(33);
  }
  EXPECT_EQ(webrtc::Dark, observer.brightness_) <<
      "Should be dark at this point since we are using a dark image.";
  EXPECT_GT(effect_filter.number_of_captured_frames_, 150) <<
      "Frames should have been played.";

  EXPECT_GE(observer.frame_rate_, 29) <<
      "Frame rate callback should be approximately correct.";
  EXPECT_LE(observer.frame_rate_, 30) <<
      "Frame rate callback should be approximately correct.";

  // Test no picture alarm
  ViETest::Log("Testing NoPictureAlarm.");
  AutoTestSleep(1050);

  EXPECT_EQ(webrtc::AlarmRaised, observer.alarm_) <<
      "No picture alarm should be raised.";
  for (int frame = 0; frame < 10; ++frame) {
    external_capture->IncomingFrame(
        video_frame, video_frame_length, capability,
        webrtc::TickTime::MillisecondTimestamp());
    AutoTestSleep(33);
  }
  EXPECT_EQ(webrtc::AlarmCleared, observer.alarm_) <<
  "Alarm should be cleared since ge just got some data.";

  delete video_frame;

  // Release the capture device
  EXPECT_EQ(0, video_engine.capture->ReleaseCaptureDevice(capture_id));

  // Release the capture device again
  EXPECT_NE(0, video_engine.capture->ReleaseCaptureDevice(capture_id));
  EXPECT_EQ(kViECaptureDeviceDoesNotExist, video_engine.LastError());
  vcpm->Release();

  /// **************************************************************
  //  Testing finished. Tear down Video Engine
  /// **************************************************************
}
