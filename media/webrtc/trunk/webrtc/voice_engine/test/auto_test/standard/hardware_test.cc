/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

class HardwareTest : public AfterStreamingFixture {
};

#if !defined(WEBRTC_IOS) && !defined(WEBRTC_ANDROID)
TEST_F(HardwareTest, AbleToQueryForDevices) {
  int num_recording_devices = 0;
  int num_playout_devices = 0;
  EXPECT_EQ(0, voe_hardware_->GetNumOfRecordingDevices(num_recording_devices));
  EXPECT_EQ(0, voe_hardware_->GetNumOfPlayoutDevices(num_playout_devices));

  ASSERT_GT(num_recording_devices, 0) <<
      "There seem to be no recording devices on your system, "
      "and this test really doesn't make sense then.";
  ASSERT_GT(num_playout_devices, 0) <<
      "There seem to be no playout devices on your system, "
      "and this test really doesn't make sense then.";

  // Recording devices are handled a bit differently on Windows - we can
  // just tell it to set the 'default' communication device there.
#ifdef _WIN32
  // Should also work while already recording.
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(
      webrtc::AudioDeviceModule::kDefaultCommunicationDevice));
  // Should also work while already playing.
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(
      webrtc::AudioDeviceModule::kDefaultCommunicationDevice));
#else
  // For other platforms, just use the first device encountered.
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(0));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(0));
#endif

  // It's hard to know what names this will return (it's system-dependent),
  // so just check that it's possible to do it.
  char device_name[128] = {0};
  char guid_name[128] = {0};
  EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(
      0, device_name, guid_name));
  EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(
      0, device_name, guid_name));
}
#endif

// Flakily hangs on Windows: code.google.com/p/webrtc/issues/detail?id=2179.
TEST_F(HardwareTest,
       DISABLED_ON_WIN(BuiltInWasapiAECWorksForAudioWindowsCoreAudioLayer)) {
#ifdef WEBRTC_IOS
  // Ensure the sound device is reset on iPhone.
  EXPECT_EQ(0, voe_hardware_->ResetAudioDevice());
  Sleep(2000);
#endif
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));

  webrtc::AudioLayers given_layer;
  EXPECT_EQ(0, voe_hardware_->GetAudioDeviceLayer(given_layer));
  if (given_layer != webrtc::kAudioWindowsCore) {
    // Not Windows Audio Core - then it shouldn't work.
    EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(true));
    EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(false));
    return;
  }

  TEST_LOG("Testing AEC for Audio Windows Core.\n");
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  // Can't be set after StartSend().
  EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(true));
  EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(false));

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_hardware_->EnableBuiltInAEC(true));

  // Can't be called before StartPlayout().
  EXPECT_EQ(-1, voe_base_->StartSend(channel_));

  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  TEST_LOG("Processing capture data with built-in AEC...\n");
  Sleep(2000);

  TEST_LOG("Looping through capture devices...\n");
  int num_devs = 0;
  char dev_name[128] = { 0 };
  char guid_name[128] = { 0 };
  EXPECT_EQ(0, voe_hardware_->GetNumOfRecordingDevices(num_devs));
  for (int dev_index = 0; dev_index < num_devs; ++dev_index) {
    EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(dev_index,
                                                       dev_name,
                                                       guid_name));
    TEST_LOG("%d: %s\n", dev_index, dev_name);
    EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(dev_index));
    Sleep(2000);
  }

  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(-1));
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(-1));

  TEST_LOG("Looping through render devices, restarting for each "
      "device...\n");
  EXPECT_EQ(0, voe_hardware_->GetNumOfPlayoutDevices(num_devs));
  for (int dev_index = 0; dev_index < num_devs; ++dev_index) {
    EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(dev_index,
                                                     dev_name,
                                                     guid_name));
    TEST_LOG("%d: %s\n", dev_index, dev_name);
    EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(dev_index));
    Sleep(2000);
  }

  TEST_LOG("Using default devices...\n");
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(-1));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(-1));
  Sleep(2000);

  // Possible, but not recommended before StopSend().
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  Sleep(2000);  // To verify that there is no garbage audio.

  TEST_LOG("Disabling built-in AEC.\n");
  EXPECT_EQ(0, voe_hardware_->EnableBuiltInAEC(false));

  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
}

// Flakily hangs on Windows: code.google.com/p/webrtc/issues/detail?id=2179.
TEST_F(HardwareTest,
       DISABLED_ON_WIN(BuiltInWasapiAECWorksForAudioWindowsCoreAudioLayer)) {
#ifdef WEBRTC_IOS
  // Ensure the sound device is reset on iPhone.
  EXPECT_EQ(0, voe_hardware_->ResetAudioDevice());
  Sleep(2000);
#endif
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));

  webrtc::AudioLayers given_layer;
  EXPECT_EQ(0, voe_hardware_->GetAudioDeviceLayer(given_layer));
  if (given_layer != webrtc::kAudioWindowsCore) {
    // Not Windows Audio Core - then it shouldn't work.
    EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(true));
    EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(false));
    return;
  }

  TEST_LOG("Testing AEC for Audio Windows Core.\n");
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  // Can't be set after StartSend().
  EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(true));
  EXPECT_EQ(-1, voe_hardware_->EnableBuiltInAEC(false));

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_hardware_->EnableBuiltInAEC(true));

  // Can't be called before StartPlayout().
  EXPECT_EQ(-1, voe_base_->StartSend(channel_));

  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  TEST_LOG("Processing capture data with built-in AEC...\n");
  Sleep(2000);

  TEST_LOG("Looping through capture devices...\n");
  int num_devs = 0;
  char dev_name[128] = { 0 };
  char guid_name[128] = { 0 };
  EXPECT_EQ(0, voe_hardware_->GetNumOfRecordingDevices(num_devs));
  for (int dev_index = 0; dev_index < num_devs; ++dev_index) {
    EXPECT_EQ(0, voe_hardware_->GetRecordingDeviceName(dev_index,
                                                       dev_name,
                                                       guid_name));
    TEST_LOG("%d: %s\n", dev_index, dev_name);
    EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(dev_index));
    Sleep(2000);
  }

  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(-1));
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(-1));

  TEST_LOG("Looping through render devices, restarting for each "
      "device...\n");
  EXPECT_EQ(0, voe_hardware_->GetNumOfPlayoutDevices(num_devs));
  for (int dev_index = 0; dev_index < num_devs; ++dev_index) {
    EXPECT_EQ(0, voe_hardware_->GetPlayoutDeviceName(dev_index,
                                                     dev_name,
                                                     guid_name));
    TEST_LOG("%d: %s\n", dev_index, dev_name);
    EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(dev_index));
    Sleep(2000);
  }

  TEST_LOG("Using default devices...\n");
  EXPECT_EQ(0, voe_hardware_->SetRecordingDevice(-1));
  EXPECT_EQ(0, voe_hardware_->SetPlayoutDevice(-1));
  Sleep(2000);

  // Possible, but not recommended before StopSend().
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  Sleep(2000);  // To verify that there is no garbage audio.

  TEST_LOG("Disabling built-in AEC.\n");
  EXPECT_EQ(0, voe_hardware_->EnableBuiltInAEC(false));

  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
}
