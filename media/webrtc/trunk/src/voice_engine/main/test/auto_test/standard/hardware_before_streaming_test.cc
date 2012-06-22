/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>

#include "after_initialization_fixture.h"

using namespace webrtc;

static const char* kNoDevicesErrorMessage =
    "Either you have no recording / playout device "
    "on your system, or the method failed.";

class HardwareBeforeStreamingTest : public AfterInitializationFixture {
};

// Tests that apply to both mobile and desktop:

TEST_F(HardwareBeforeStreamingTest,
       SetAudioDeviceLayerFailsSinceTheVoiceEngineHasBeenInitialized) {
  EXPECT_NE(0, voe_hardware_->SetAudioDeviceLayer(kAudioPlatformDefault));
  EXPECT_EQ(VE_ALREADY_INITED, voe_base_->LastError());
}

TEST_F(HardwareBeforeStreamingTest,
       GetCPULoadSucceedsOnWindowsButNotOtherPlatforms) {
  int load_percent;
#if defined(_WIN32)
  EXPECT_EQ(0, voe_hardware_->GetCPULoad(load_percent));
#else
  EXPECT_NE(0, voe_hardware_->GetCPULoad(load_percent)) <<
      "Should fail on non-Windows platforms.";
#endif
}

// Tests that only apply to mobile:

#ifdef MAC_IPHONE
TEST_F(HardwareBeforeStreamingTest, ResetsAudioDeviceOnIphone) {
  EXPECT_EQ(0, voe_hardware_->ResetAudioDevice());
}
#endif

// Tests that only apply to desktop:
#if !defined(MAC_IPHONE) & !defined(WEBRTC_ANDROID)

TEST_F(HardwareBeforeStreamingTest, GetSystemCpuLoadSucceeds) {
#ifdef _WIN32
  // This method needs some warm-up time on Windows. We sleep a good amount
  // of time instead of retrying to make the test simpler.
  Sleep(2000);
#endif

  int load_percent;
  EXPECT_EQ(0, voe_hardware_->GetSystemCPULoad(load_percent));
}

TEST_F(HardwareBeforeStreamingTest, GetPlayoutDeviceStatusReturnsTrue) {
  bool play_available = false;
  EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceStatus(play_available));
  ASSERT_TRUE(play_available) <<
      "Ensures that the method works and that hardware is in the right state.";
}

TEST_F(HardwareBeforeStreamingTest, GetRecordingDeviceStatusReturnsTrue) {
  bool recording_available = false;
  EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceStatus(recording_available));
  EXPECT_TRUE(recording_available) <<
      "Ensures that the method works and that hardware is in the right state.";
}

  // Win, Mac and Linux sound device tests.
TEST_F(HardwareBeforeStreamingTest,
       GetRecordingDeviceNameRetrievesDeviceNames) {
  char device_name[128] = {0};
  char guid_name[128] = {0};

#ifdef _WIN32
  EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(
      -1, device_name, guid_name));
  EXPECT_GT(strlen(device_name), 0u) << kNoDevicesErrorMessage;
  device_name[0] = '\0';

  EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(
      -1, device_name, guid_name));
  EXPECT_GT(strlen(device_name), 0u) << kNoDevicesErrorMessage;

#else
  EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(
      0, device_name, guid_name));
  EXPECT_GT(strlen(device_name), 0u) << kNoDevicesErrorMessage;
  device_name[0] = '\0';

  EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(
      0, device_name, guid_name));
  EXPECT_GT(strlen(device_name), 0u) << kNoDevicesErrorMessage;
#endif  // !WIN32
}

TEST_F(HardwareBeforeStreamingTest,
       AllEnumeratedRecordingDevicesCanBeSetAsRecordingDevice) {
  // Check recording side.
  // Extended Win32 enumeration tests: unique GUID outputs on Vista and up:
  // Win XP and below : device_name is copied to guid_name.
  // Win Vista and up : device_name is the friendly name and GUID is a unique
  //                    identifier.
  // Other            : guid_name is left unchanged.
  int num_of_recording_devices = 0;
  EXPECT_EQ(0, voe_hardware_->GetNumOfRecordingDevices(
      num_of_recording_devices));
  EXPECT_GT(num_of_recording_devices, 0) << kNoDevicesErrorMessage;

  char device_name[128] = {0};
  char guid_name[128] = {0};

  for (int i = 0; i < num_of_recording_devices; i++) {
    EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(
        i, device_name, guid_name));
    EXPECT_GT(strlen(device_name), 0u) <<
        "There should be no empty device names "
        "among the ones the system gives us.";
    EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(i));
  }
}

TEST_F(HardwareBeforeStreamingTest,
       AllEnumeratedPlayoutDevicesCanBeSetAsPlayoutDevice) {
  // Check playout side (see recording side test for more info on GUIDs).
  int num_of_playout_devices = 0;
  EXPECT_EQ(0, voe_hardware_->GetNumOfPlayoutDevices(
      num_of_playout_devices));
  EXPECT_GT(num_of_playout_devices, 0) << kNoDevicesErrorMessage;

  char device_name[128] = {0};
  char guid_name[128] = {0};

  for (int i = 0; i < num_of_playout_devices; ++i) {
    EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(
        i, device_name, guid_name));
    EXPECT_GT(strlen(device_name), 0u) <<
        "There should be no empty device names "
        "among the ones the system gives us.";
    EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(i));
  }
}

TEST_F(HardwareBeforeStreamingTest,
       SetDeviceWithMagicalArgumentsSetsDefaultSoundDevices) {
#ifdef _WIN32
  // -1 means "default device" on Windows.
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(-1));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(-1));
#else
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(0));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(0));
#endif
}

#endif // !defined(MAC_IPHONE) & !defined(WEBRTC_ANDROID)
