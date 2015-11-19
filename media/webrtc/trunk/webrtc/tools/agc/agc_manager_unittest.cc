/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/agc/agc_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_processing/agc/mock_agc.h"
#include "webrtc/modules/audio_processing/include/mock_audio_processing.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/include/mock/fake_voe_external_media.h"
#include "webrtc/voice_engine/include/mock/mock_voe_volume_control.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SetArgReferee;

namespace webrtc {
namespace {

const int kSampleRateHz = 32000;
const int kNumChannels = 1;
const int kSamplesPerChannel = kSampleRateHz / 100;
const float kAboveClippedThreshold = 0.2f;

}  // namespace

class AgcManagerUnitTest : public ::testing::Test {
 protected:
  AgcManagerUnitTest()
      : media_(),
        volume_(),
        agc_(new MockAgc),
        audioproc_(new MockAudioProcessing),
        gctrl_(audioproc_->gain_control()),
        manager_(&media_, &volume_, agc_, audioproc_) {
    EXPECT_CALL(*gctrl_, Enable(true));
    ExpectInitialize();
    manager_.Enable(true);
    EXPECT_CALL(*agc_, GetRmsErrorDb(_))
        .WillOnce(Return(false));
    // TODO(bjornv): Find a better solution that adds an initial volume here
    // instead of applying SetVolumeAndProcess(128u) in each test, but at the
    // same time can test a too low initial value.
  }

  void SetInitialVolume(unsigned int volume) {
    ExpectInitialize();
    manager_.CaptureDeviceChanged();
    ExpectCheckVolumeAndReset(volume);
    EXPECT_CALL(*agc_, GetRmsErrorDb(_)).WillOnce(Return(false));
    PostProcCallback(1);
  }

  void SetVolumeAndProcess(unsigned int volume) {
    // Volume is checked on first process call.
    ExpectCheckVolumeAndReset(volume);
    PostProcCallback(1);
  }

  void ExpectCheckVolumeAndReset(unsigned int volume) {
    EXPECT_CALL(volume_, GetMicVolume(_))
        .WillOnce(DoAll(SetArgReferee<0>(volume), Return(0)));
    EXPECT_CALL(*agc_, Reset());
  }

  void ExpectVolumeChange(unsigned int current_volume,
                          unsigned int new_volume) {
    EXPECT_CALL(volume_, GetMicVolume(_))
        .WillOnce(DoAll(SetArgReferee<0>(current_volume), Return(0)));
    EXPECT_CALL(volume_, SetMicVolume(Eq(new_volume))).WillOnce(Return(0));
  }

  void ExpectInitialize() {
    EXPECT_CALL(*gctrl_, set_mode(GainControl::kFixedDigital));
    EXPECT_CALL(*gctrl_, set_target_level_dbfs(2));
    EXPECT_CALL(*gctrl_, set_compression_gain_db(7));
    EXPECT_CALL(*gctrl_, enable_limiter(true));
  }

  void PreProcCallback(int num_calls) {
    for (int i = 0; i < num_calls; ++i) {
      media_.CallProcess(kRecordingPreprocessing, NULL, kSamplesPerChannel,
                         kSampleRateHz, kNumChannels);
    }
  }

  void PostProcCallback(int num_calls) {
    for (int i = 0; i < num_calls; ++i) {
      EXPECT_CALL(*agc_, Process(_, _, _)).WillOnce(Return(0));
      EXPECT_CALL(*audioproc_, ProcessStream(_)).WillOnce(Return(0));
      media_.CallProcess(kRecordingAllChannelsMixed, NULL, kSamplesPerChannel,
                         kSampleRateHz, kNumChannels);
    }
  }

  ~AgcManagerUnitTest() {
    EXPECT_CALL(volume_, Release()).WillOnce(Return(0));
  }

  FakeVoEExternalMedia media_;
  MockVoEVolumeControl volume_;
  MockAgc* agc_;
  MockAudioProcessing* audioproc_;
  MockGainControl* gctrl_;
  AgcManager manager_;
  test::TraceToStderr trace_to_stderr;
};

TEST_F(AgcManagerUnitTest, MicVolumeResponseToRmsError) {
  SetVolumeAndProcess(128u);
  // Compressor default; no residual error.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  PostProcCallback(1);

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)));
  PostProcCallback(1);

  // Above the compressor's window; volume should be increased.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  ExpectVolumeChange(128u, 130u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  ExpectVolumeChange(130u, 168u);
  PostProcCallback(1);

  // Inside the compressor's window; no change of volume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)));
  PostProcCallback(1);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)));
  PostProcCallback(1);

  // Below the compressor's window; volume should be decreased.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  ExpectVolumeChange(168u, 167u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  ExpectVolumeChange(167u, 163u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-9), Return(true)));
  ExpectVolumeChange(163u, 129u);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, MicVolumeIsLimited) {
  SetVolumeAndProcess(128u);
  // Maximum upwards change is limited.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  ExpectVolumeChange(128u, 183u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  ExpectVolumeChange(183u, 243u);
  PostProcCallback(1);

  // Won't go higher than the maximum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  ExpectVolumeChange(243u, 255u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  ExpectVolumeChange(255u, 254u);
  PostProcCallback(1);

  // Maximum downwards change is limited.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(254u, 194u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(194u, 137u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(137u, 88u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(88u, 54u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(54u, 33u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(33u, 18u);
  PostProcCallback(1);

  // Won't go lower than the minimum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-40), Return(true)));
  ExpectVolumeChange(18u, 12u);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, CompressorStepsTowardsTarget) {
  SetVolumeAndProcess(128u);
  // Compressor default; no call to set_compression_gain_db.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(20);

  // Moves slowly upwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(1);

  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  PostProcCallback(1);

  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(20);

  // Moves slowly downward, then reverses before reaching the original target.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(5), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(9), Return(true)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  PostProcCallback(1);

  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(20);
}

TEST_F(AgcManagerUnitTest, CompressorErrorIsDeemphasized) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  PostProcCallback(1);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(20);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(7)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(6)).WillOnce(Return(0));
  PostProcCallback(1);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(_)).Times(0);
  PostProcCallback(20);
}

TEST_F(AgcManagerUnitTest, CompressorReachesMaximum) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(10), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(10)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(11)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(12)).WillOnce(Return(0));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, CompressorReachesMinimum) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(0), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(6)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(5)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(4)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(3)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(2)).WillOnce(Return(0));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, NoActionWhileMuted) {
  SetVolumeAndProcess(128u);
  manager_.SetCaptureMuted(true);
  media_.CallProcess(kRecordingAllChannelsMixed, NULL, kSamplesPerChannel,
                     kSampleRateHz, kNumChannels);
}

TEST_F(AgcManagerUnitTest, UnmutingChecksVolumeWithoutRaising) {
  SetVolumeAndProcess(128u);
  manager_.SetCaptureMuted(true);
  manager_.SetCaptureMuted(false);
  ExpectCheckVolumeAndReset(127u);
  // SetMicVolume should not be called.
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(Return(false));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, UnmutingRaisesTooLowVolume) {
  SetVolumeAndProcess(128u);
  manager_.SetCaptureMuted(true);
  manager_.SetCaptureMuted(false);
  ExpectCheckVolumeAndReset(11u);
  EXPECT_CALL(volume_, SetMicVolume(Eq(12u))).WillOnce(Return(0));
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(Return(false));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, ChangingDevicesChecksVolume) {
  SetVolumeAndProcess(128u);
  ExpectInitialize();
  manager_.CaptureDeviceChanged();
  ExpectCheckVolumeAndReset(128u);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(Return(false));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, LowInitialVolumeIsRaised) {
  ExpectCheckVolumeAndReset(11u);
  // Should set MicVolume to kMinInitMicLevel = 85.
  EXPECT_CALL(volume_, SetMicVolume(Eq(85u))).WillOnce(Return(0));
  PostProcCallback(1);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(Return(false));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, ManualLevelChangeResultsInNoSetMicCall) {
  SetVolumeAndProcess(128u);
  // Change outside of compressor's range, which would normally trigger a call
  // to SetMicVolume.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillOnce(DoAll(SetArgReferee<0>(154u), Return(0)));
  // SetMicVolume should not be called.
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(1);
  PostProcCallback(1);

  // Do the same thing, except downwards now.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillOnce(DoAll(SetArgReferee<0>(100u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(1);
  PostProcCallback(1);

  // And finally verify the AGC continues working without a manual change.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  ExpectVolumeChange(100u, 99u);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, RecoveryAfterManualLevelChangeFromMax) {
  SetVolumeAndProcess(128u);
  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  ExpectVolumeChange(128u, 183u);
  PostProcCallback(1);
  ExpectVolumeChange(183u, 243u);
  PostProcCallback(1);
  ExpectVolumeChange(243u, 255u);
  PostProcCallback(1);

  // Manual change does not result in SetMicVolume call.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillOnce(DoAll(SetArgReferee<0>(50u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(1);
  PostProcCallback(1);

  // Continues working as usual afterwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  ExpectVolumeChange(50u, 69u);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, RecoveryAfterManualLevelChangeBelowMin) {
  SetVolumeAndProcess(128u);
  // Manual change below min.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(-1), Return(true)));
  // Don't set to zero, which will cause AGC to take no action.
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillOnce(DoAll(SetArgReferee<0>(1u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(1);
  PostProcCallback(1);

  // Continues working as usual afterwards.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)));
  ExpectVolumeChange(1u, 2u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  ExpectVolumeChange(2u, 11u);
  PostProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillOnce(DoAll(SetArgPointee<0>(20), Return(true)));
  ExpectVolumeChange(11u, 18u);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, NoClippingHasNoImpact) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(volume_, GetMicVolume(_)).Times(0);
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(0);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _)).WillRepeatedly(Return(0));
  PreProcCallback(100);
}

TEST_F(AgcManagerUnitTest, ClippingUnderThresholdHasNoImpact) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(volume_, GetMicVolume(_)).Times(0);
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(0);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _)).WillOnce(Return(0.099));
  PreProcCallback(1);
}

TEST_F(AgcManagerUnitTest, ClippingLowersVolume) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(255u);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _)).WillOnce(Return(0.101));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(255u, 240u);
  PreProcCallback(1);
}

TEST_F(AgcManagerUnitTest, WaitingPeriodBetweenClippingChecks) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(255u);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(255u, 240u);
  PreProcCallback(1);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillRepeatedly(Return(kAboveClippedThreshold));
  EXPECT_CALL(volume_, GetMicVolume(_)).Times(0);
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(0);
  PreProcCallback(300);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(240u, 225u);
  PreProcCallback(1);
}

TEST_F(AgcManagerUnitTest, ClippingLoweringIsLimited) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(180u);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(180u, 170u);
  PreProcCallback(1);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillRepeatedly(Return(kAboveClippedThreshold));
  EXPECT_CALL(volume_, GetMicVolume(_)).Times(0);
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(0);
  PreProcCallback(1000);
}

TEST_F(AgcManagerUnitTest, ClippingMaxIsRespectedWhenEqualToLevel) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(255u);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(255u, 240u);
  PreProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(240u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  PostProcCallback(10);
}

TEST_F(AgcManagerUnitTest, ClippingMaxIsRespectedWhenHigherThanLevel) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(200u);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(200u, 185u);
  PreProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
    .WillRepeatedly(DoAll(SetArgPointee<0>(40), Return(true)));
  ExpectVolumeChange(185u, 240u);
  PostProcCallback(1);
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(240u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  PostProcCallback(10);
}

TEST_F(AgcManagerUnitTest, MaxCompressionIsIncreasedAfterClipping) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(210u);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(210u, 195u);
  PreProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(11), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(8)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(9)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(10)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(11)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(12)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(13)).WillOnce(Return(0));
  PostProcCallback(1);

  // Continue clipping until we hit the maximum surplus compression.
  PreProcCallback(300);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(195u, 180u);
  PreProcCallback(1);

  PreProcCallback(300);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(180u, 170u);
  PreProcCallback(1);

  // Current level is now at the minimum, but the maximum allowed level still
  // has more to decrease.
  PreProcCallback(300);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  PreProcCallback(1);

  PreProcCallback(300);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  PreProcCallback(1);

  PreProcCallback(300);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  PreProcCallback(1);

  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)))
      .WillRepeatedly(Return(false));
  PostProcCallback(19);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(14)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(15)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(16)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(17)).WillOnce(Return(0));
  PostProcCallback(20);
  EXPECT_CALL(*gctrl_, set_compression_gain_db(18)).WillOnce(Return(0));
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, UserCanRaiseVolumeAfterClipping) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(225u);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(*agc_, Reset()).Times(1);
  ExpectVolumeChange(225u, 210u);
  PreProcCallback(1);

  // High enough error to trigger a volume check.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(14), Return(true)));
  // User changed the volume.
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillOnce(DoAll(SetArgReferee<0>(250u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(1);
  PostProcCallback(1);

  // Move down...
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(-10), Return(true)));
  ExpectVolumeChange(250u, 210u);
  PostProcCallback(1);
  // And back up to the new max established by the user.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(40), Return(true)));
  ExpectVolumeChange(210u, 250u);
  PostProcCallback(1);
  // Will not move above new maximum.
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillOnce(DoAll(SetArgPointee<0>(30), Return(true)));
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(250u), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  PostProcCallback(1);
}

TEST_F(AgcManagerUnitTest, ClippingDoesNotPullLowVolumeBackUp) {
  SetVolumeAndProcess(128u);
  SetInitialVolume(80u);
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .WillOnce(Return(kAboveClippedThreshold));
  EXPECT_CALL(volume_, GetMicVolume(_)).Times(0);
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  EXPECT_CALL(*agc_, Reset()).Times(0);
  PreProcCallback(1);
}

TEST_F(AgcManagerUnitTest, TakesNoActionOnZeroMicVolume) {
  SetVolumeAndProcess(128u);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(30), Return(true)));
  EXPECT_CALL(volume_, GetMicVolume(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(0)));
  EXPECT_CALL(volume_, SetMicVolume(_)).Times(0);
  PostProcCallback(10);
}

}  // namespace webrtc
