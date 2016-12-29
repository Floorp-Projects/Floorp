/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/android/build_info.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/ensure_initialized.h"

#define PRINT(...) fprintf(stderr, __VA_ARGS__);

namespace webrtc {

static const char kTag[] = "  ";

class AudioManagerTest : public ::testing::Test {
 protected:
  AudioManagerTest() {
    // One-time initialization of JVM and application context. Ensures that we
    // can do calls between C++ and Java.
    webrtc::audiodevicemodule::EnsureInitialized();
    audio_manager_.reset(new AudioManager());
    SetActiveAudioLayer();
    playout_parameters_ = audio_manager()->GetPlayoutAudioParameters();
    record_parameters_ = audio_manager()->GetRecordAudioParameters();
  }

  AudioManager* audio_manager() const { return audio_manager_.get(); }

  // A valid audio layer must always be set before calling Init(), hence we
  // might as well make it a part of the test fixture.
  void SetActiveAudioLayer() {
    EXPECT_EQ(0, audio_manager()->GetDelayEstimateInMilliseconds());
    audio_manager()->SetActiveAudioLayer(AudioDeviceModule::kAndroidJavaAudio);
    EXPECT_NE(0, audio_manager()->GetDelayEstimateInMilliseconds());
  }

  rtc::scoped_ptr<AudioManager> audio_manager_;
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

TEST_F(AudioManagerTest, ConstructDestruct) {
}

TEST_F(AudioManagerTest, InitClose) {
  EXPECT_TRUE(audio_manager()->Init());
  EXPECT_TRUE(audio_manager()->Close());
}

TEST_F(AudioManagerTest, IsAcousticEchoCancelerSupported) {
  PRINT("%sAcoustic Echo Canceler support: %s\n", kTag,
        audio_manager()->IsAcousticEchoCancelerSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsAutomaticGainControlSupported) {
  PRINT("%sAutomatic Gain Control support: %s\n", kTag,
        audio_manager()->IsAutomaticGainControlSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsNoiseSuppressorSupported) {
  PRINT("%sNoise Suppressor support: %s\n", kTag,
        audio_manager()->IsNoiseSuppressorSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, IsLowLatencyPlayoutSupported) {
  PRINT("%sLow latency output support: %s\n", kTag,
        audio_manager()->IsLowLatencyPlayoutSupported() ? "Yes" : "No");
}

TEST_F(AudioManagerTest, ShowAudioParameterInfo) {
  const bool low_latency_out = audio_manager()->IsLowLatencyPlayoutSupported();
  PRINT("PLAYOUT:\n");
  PRINT("%saudio layer: %s\n", kTag,
        low_latency_out ? "Low latency OpenSL" : "Java/JNI based AudioTrack");
  PRINT("%ssample rate: %d Hz\n", kTag, playout_parameters_.sample_rate());
  PRINT("%schannels: %" PRIuS "\n", kTag, playout_parameters_.channels());
  PRINT("%sframes per buffer: %" PRIuS " <=> %.2f ms\n", kTag,
        playout_parameters_.frames_per_buffer(),
        playout_parameters_.GetBufferSizeInMilliseconds());
  PRINT("RECORD: \n");
  PRINT("%saudio layer: %s\n", kTag, "Java/JNI based AudioRecord");
  PRINT("%ssample rate: %d Hz\n", kTag, record_parameters_.sample_rate());
  PRINT("%schannels: %" PRIuS "\n", kTag, record_parameters_.channels());
  PRINT("%sframes per buffer: %" PRIuS " <=> %.2f ms\n", kTag,
        record_parameters_.frames_per_buffer(),
        record_parameters_.GetBufferSizeInMilliseconds());
}

// Add device-specific information to the test for logging purposes.
TEST_F(AudioManagerTest, ShowDeviceInfo) {
  BuildInfo build_info;
  PRINT("%smodel: %s\n", kTag, build_info.GetDeviceModel().c_str());
  PRINT("%sbrand: %s\n", kTag, build_info.GetBrand().c_str());
  PRINT("%smanufacturer: %s\n",
        kTag, build_info.GetDeviceManufacturer().c_str());
}

// Add Android build information to the test for logging purposes.
TEST_F(AudioManagerTest, ShowBuildInfo) {
  BuildInfo build_info;
  PRINT("%sbuild release: %s\n", kTag, build_info.GetBuildRelease().c_str());
  PRINT("%sbuild id: %s\n", kTag, build_info.GetAndroidBuildId().c_str());
  PRINT("%sbuild type: %s\n", kTag, build_info.GetBuildType().c_str());
  PRINT("%sSDK version: %s\n", kTag, build_info.GetSdkVersion().c_str());
}

// Basic test of the AudioParameters class using default construction where
// all members are set to zero.
TEST_F(AudioManagerTest, AudioParametersWithDefaultConstruction) {
  AudioParameters params;
  EXPECT_FALSE(params.is_valid());
  EXPECT_EQ(0, params.sample_rate());
  EXPECT_EQ(0U, params.channels());
  EXPECT_EQ(0U, params.frames_per_buffer());
  EXPECT_EQ(0U, params.frames_per_10ms_buffer());
  EXPECT_EQ(0U, params.GetBytesPerFrame());
  EXPECT_EQ(0U, params.GetBytesPerBuffer());
  EXPECT_EQ(0U, params.GetBytesPer10msBuffer());
  EXPECT_EQ(0.0f, params.GetBufferSizeInMilliseconds());
}

// Basic test of the AudioParameters class using non default construction.
TEST_F(AudioManagerTest, AudioParametersWithNonDefaultConstruction) {
  const int kSampleRate = 48000;
  const size_t kChannels = 1;
  const size_t kFramesPerBuffer = 480;
  const size_t kFramesPer10msBuffer = 480;
  const size_t kBytesPerFrame = 2;
  const float kBufferSizeInMs = 10.0f;
  AudioParameters params(kSampleRate, kChannels, kFramesPerBuffer);
  EXPECT_TRUE(params.is_valid());
  EXPECT_EQ(kSampleRate, params.sample_rate());
  EXPECT_EQ(kChannels, params.channels());
  EXPECT_EQ(kFramesPerBuffer, params.frames_per_buffer());
  EXPECT_EQ(static_cast<size_t>(kSampleRate / 100),
            params.frames_per_10ms_buffer());
  EXPECT_EQ(kBytesPerFrame, params.GetBytesPerFrame());
  EXPECT_EQ(kBytesPerFrame * kFramesPerBuffer, params.GetBytesPerBuffer());
  EXPECT_EQ(kBytesPerFrame * kFramesPer10msBuffer,
            params.GetBytesPer10msBuffer());
  EXPECT_EQ(kBufferSizeInMs, params.GetBufferSizeInMilliseconds());
}

}  // namespace webrtc

