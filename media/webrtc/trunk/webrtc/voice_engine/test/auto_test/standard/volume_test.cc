/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "after_streaming_fixture.h"

namespace {

void ExpectVolumeNear(int expected, int actual) {
  // The hardware volume may be more coarsely quantized than [0, 255], so
  // it is not always reasonable to expect to get exactly what we set. This
  // allows for some error.
  const int kMaxVolumeError = 10;
  EXPECT_NEAR(expected, actual, kMaxVolumeError);
  EXPECT_GE(actual, 0);
  EXPECT_LE(actual, 255);
}

}  // namespace

class VolumeTest : public AfterStreamingFixture {
};

// TODO(phoglund): a number of tests are disabled here on Linux, all pending
// investigation in
// http://code.google.com/p/webrtc/issues/detail?id=367

TEST_F(VolumeTest, DefaultSpeakerVolumeIsAtMost255) {
  unsigned int volume = 1000;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  EXPECT_LE(volume, 255u);
}

TEST_F(VolumeTest, SetVolumeBeforePlayoutWorks) {
  // This is a rather specialized test, intended to exercise some PulseAudio
  // code. However, these conditions should be satisfied on any platform.
  unsigned int original_volume = 0;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(original_volume));
  Sleep(1000);

  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(200));
  unsigned int volume;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(200u, volume);

  PausePlaying();
  ResumePlaying();
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  // Ensure the volume has not changed after resuming playout.
  ExpectVolumeNear(200u, volume);

  PausePlaying();
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(100));
  ResumePlaying();
  // Ensure the volume set while paused is retained.
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(100u, volume);

  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(original_volume));
}

TEST_F(VolumeTest, ManualSetVolumeWorks) {
  unsigned int original_volume = 0;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(original_volume));
  Sleep(1000);

  TEST_LOG("Setting speaker volume to 0 out of 255.\n");
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(0));
  unsigned int volume;
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(0u, volume);
  Sleep(1000);

  TEST_LOG("Setting speaker volume to 100 out of 255.\n");
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(100));
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(100u, volume);
  Sleep(1000);

  // Set the volume to 255 very briefly so we don't blast the poor user
  // listening to this. This is just to test the call succeeds.
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(255));
  EXPECT_EQ(0, voe_volume_control_->GetSpeakerVolume(volume));
  ExpectVolumeNear(255u, volume);

  TEST_LOG("Setting speaker volume to the original %d out of 255.\n",
      original_volume);
  EXPECT_EQ(0, voe_volume_control_->SetSpeakerVolume(original_volume));
  Sleep(1000);
}

#if !defined(WEBRTC_IOS)

TEST_F(VolumeTest, DISABLED_ON_LINUX(DefaultMicrophoneVolumeIsAtMost255)) {
  unsigned int volume = 1000;
  EXPECT_EQ(0, voe_volume_control_->GetMicVolume(volume));
  EXPECT_LE(volume, 255u);
}

TEST_F(VolumeTest, DISABLED_ON_LINUX(
          ManualRequiresMicrophoneCanSetMicrophoneVolumeWithAcgOff)) {
  SwitchToManualMicrophone();
  EXPECT_EQ(0, voe_apm_->SetAgcStatus(false));

  unsigned int original_volume = 0;
  EXPECT_EQ(0, voe_volume_control_->GetMicVolume(original_volume));

  TEST_LOG("Setting microphone volume to 0.\n");
  EXPECT_EQ(0, voe_volume_control_->SetMicVolume(channel_));
  Sleep(1000);
  TEST_LOG("Setting microphone volume to 255.\n");
  EXPECT_EQ(0, voe_volume_control_->SetMicVolume(255));
  Sleep(1000);
  TEST_LOG("Setting microphone volume back to saved value.\n");
  EXPECT_EQ(0, voe_volume_control_->SetMicVolume(original_volume));
  Sleep(1000);
}

TEST_F(VolumeTest, ChannelScalingIsOneByDefault) {
  float scaling = -1.0f;

  EXPECT_EQ(0, voe_volume_control_->GetChannelOutputVolumeScaling(
      channel_, scaling));
  EXPECT_FLOAT_EQ(1.0f, scaling);
}

TEST_F(VolumeTest, ManualCanSetChannelScaling) {
  EXPECT_EQ(0, voe_volume_control_->SetChannelOutputVolumeScaling(
      channel_, 0.1f));

  float scaling = 1.0f;
  EXPECT_EQ(0, voe_volume_control_->GetChannelOutputVolumeScaling(
      channel_, scaling));

  EXPECT_FLOAT_EQ(0.1f, scaling);

  TEST_LOG("Channel scaling set to 0.1: audio should be barely audible.\n");
  Sleep(2000);
}

#endif  // !WEBRTC_IOS

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)

TEST_F(VolumeTest, InputMutingIsNotEnabledByDefault) {
  bool is_muted = true;
  EXPECT_EQ(0, voe_volume_control_->GetInputMute(channel_, is_muted));
  EXPECT_FALSE(is_muted);
}

TEST_F(VolumeTest, DISABLED_ON_LINUX(ManualInputMutingMutesMicrophone)) {
  SwitchToManualMicrophone();

  // Enable muting.
  EXPECT_EQ(0, voe_volume_control_->SetInputMute(channel_, true));
  bool is_muted = false;
  EXPECT_EQ(0, voe_volume_control_->GetInputMute(channel_, is_muted));
  EXPECT_TRUE(is_muted);

  TEST_LOG("Muted: talk into microphone and verify you can't hear yourself.\n");
  Sleep(2000);

  // Test that we can disable muting.
  EXPECT_EQ(0, voe_volume_control_->SetInputMute(channel_, false));
  EXPECT_EQ(0, voe_volume_control_->GetInputMute(channel_, is_muted));
  EXPECT_FALSE(is_muted);

  TEST_LOG("Unmuted: talk into microphone and verify you can hear yourself.\n");
  Sleep(2000);
}

TEST_F(VolumeTest, DISABLED_ON_LINUX(SystemInputMutingIsNotEnabledByDefault)) {
  bool is_muted = true;
  EXPECT_EQ(0, voe_volume_control_->GetSystemInputMute(is_muted));
  EXPECT_FALSE(is_muted);
}

TEST_F(VolumeTest, DISABLED_ON_LINUX(ManualSystemInputMutingMutesMicrophone)) {
  SwitchToManualMicrophone();

  // Enable system input muting.
  EXPECT_EQ(0, voe_volume_control_->SetSystemInputMute(true));
  bool is_muted = false;
  EXPECT_EQ(0, voe_volume_control_->GetSystemInputMute(is_muted));
  EXPECT_TRUE(is_muted);

  TEST_LOG("Muted: talk into microphone and verify you can't hear yourself.\n");
  Sleep(2000);

  // Test that we can disable system input muting.
  EXPECT_EQ(0, voe_volume_control_->SetSystemInputMute(false));
  EXPECT_EQ(0, voe_volume_control_->GetSystemInputMute(is_muted));
  EXPECT_FALSE(is_muted);

  TEST_LOG("Unmuted: talk into microphone and verify you can hear yourself.\n");
  Sleep(2000);
}

TEST_F(VolumeTest, DISABLED_ON_LINUX(SystemOutputMutingIsNotEnabledByDefault)) {
  bool is_muted = true;
  EXPECT_EQ(0, voe_volume_control_->GetSystemOutputMute(is_muted));
  EXPECT_FALSE(is_muted);
}

TEST_F(VolumeTest, ManualSystemOutputMutingMutesOutput) {
  // Enable muting.
  EXPECT_EQ(0, voe_volume_control_->SetSystemOutputMute(true));
  bool is_muted = false;
  EXPECT_EQ(0, voe_volume_control_->GetSystemOutputMute(is_muted));
  EXPECT_TRUE(is_muted);

  TEST_LOG("Muted: you should hear no audio.\n");
  Sleep(2000);

  // Test that we can disable muting.
  EXPECT_EQ(0, voe_volume_control_->SetSystemOutputMute(false));
  EXPECT_EQ(0, voe_volume_control_->GetSystemOutputMute(is_muted));
  EXPECT_FALSE(is_muted);

  TEST_LOG("Unmuted: you should hear audio.\n");
  Sleep(2000);
}

TEST_F(VolumeTest, ManualTestInputAndOutputLevels) {
  SwitchToManualMicrophone();

  TEST_LOG("Speak and verify that the following levels look right:\n");
  for (int i = 0; i < 5; i++) {
    Sleep(1000);
    unsigned int input_level = 0;
    unsigned int output_level = 0;
    unsigned int input_level_full_range = 0;
    unsigned int output_level_full_range = 0;

    EXPECT_EQ(0, voe_volume_control_->GetSpeechInputLevel(
        input_level));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechOutputLevel(
        channel_, output_level));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechInputLevelFullRange(
        input_level_full_range));
    EXPECT_EQ(0, voe_volume_control_->GetSpeechOutputLevelFullRange(
        channel_, output_level_full_range));

    TEST_LOG("    warped levels (0-9)    : in=%5d, out=%5d\n",
        input_level, output_level);
    TEST_LOG("    linear levels (0-32768): in=%5d, out=%5d\n",
        input_level_full_range, output_level_full_range);
  }
}

TEST_F(VolumeTest, ChannelsAreNotPannedByDefault) {
  float left = -1.0;
  float right = -1.0;

  EXPECT_EQ(0, voe_volume_control_->GetOutputVolumePan(channel_, left, right));
  EXPECT_FLOAT_EQ(1.0, left);
  EXPECT_FLOAT_EQ(1.0, right);
}

TEST_F(VolumeTest, ManualTestChannelPanning) {
  TEST_LOG("Panning left.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 0.8f, 0.1f));
  Sleep(1000);

  TEST_LOG("Back to center.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 1.0f, 1.0f));
  Sleep(1000);

  TEST_LOG("Panning right.\n");
  EXPECT_EQ(0, voe_volume_control_->SetOutputVolumePan(channel_, 0.1f, 0.8f));
  Sleep(1000);

  // To finish, verify that the getter works.
  float left = 0.0f;
  float right = 0.0f;

  EXPECT_EQ(0, voe_volume_control_->GetOutputVolumePan(channel_, left, right));
  EXPECT_FLOAT_EQ(0.1f, left);
  EXPECT_FLOAT_EQ(0.8f, right);
}

#endif  // !WEBRTC_ANDROID && !WEBRTC_IOS
