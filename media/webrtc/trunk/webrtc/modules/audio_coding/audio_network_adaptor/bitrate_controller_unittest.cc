/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace audio_network_adaptor {

namespace {

void CheckDecision(BitrateController* controller,
                   const rtc::Optional<int>& target_audio_bitrate_bps,
                   const rtc::Optional<size_t>& overhead_bytes_per_packet,
                   const rtc::Optional<int>& frame_length_ms,
                   int expected_bitrate_bps) {
  Controller::NetworkMetrics metrics;
  metrics.target_audio_bitrate_bps = target_audio_bitrate_bps;
  metrics.overhead_bytes_per_packet = overhead_bytes_per_packet;
  AudioNetworkAdaptor::EncoderRuntimeConfig config;
  config.frame_length_ms = frame_length_ms;
  controller->MakeDecision(metrics, &config);
  EXPECT_EQ(rtc::Optional<int>(expected_bitrate_bps), config.bitrate_bps);
}

}  // namespace

// These tests are named AnaBitrateControllerTest to distinguish from
// BitrateControllerTest in
// modules/bitrate_controller/bitrate_controller_unittest.cc.

TEST(AnaBitrateControllerTest, OutputInitValueWhenTargetBitrateUnknown) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  constexpr size_t kOverheadBytesPerPacket = 64;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  CheckDecision(&controller, rtc::Optional<int>(),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(kInitialFrameLengthMs * 2),
                kInitialBitrateBps);
}

TEST(AnaBitrateControllerTest, OutputInitValueWhenOverheadUnknown) {
  constexpr int kInitialBitrateBps = 32000;
  constexpr int kInitialFrameLengthMs = 20;
  constexpr int kTargetBitrateBps = 48000;
  BitrateController controller(
      BitrateController::Config(kInitialBitrateBps, kInitialFrameLengthMs));
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(),
                rtc::Optional<int>(kInitialFrameLengthMs * 2),
                kInitialBitrateBps);
}

TEST(AnaBitrateControllerTest, ChangeBitrateOnTargetBitrateChanged) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  // Frame length unchanged, bitrate changes in accordance with
  // |metrics.target_audio_bitrate_bps| and |metrics.overhead_bytes_per_packet|.
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(kInitialFrameLengthMs), kBitrateBps);
}

TEST(AnaBitrateControllerTest, TreatUnknownFrameLengthAsFrameLengthUnchanged) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));
  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(), kBitrateBps);
}

TEST(AnaBitrateControllerTest, IncreaseBitrateOnFrameLengthIncreased) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 20;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));

  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(), kBitrateBps);

  constexpr int kFrameLengthMs = 60;
  constexpr size_t kPacketOverheadRateDiff =
      kOverheadBytesPerPacket * 8 * 1000 / 20 -
      kOverheadBytesPerPacket * 8 * 1000 / 60;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(kFrameLengthMs),
                kBitrateBps + kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, DecreaseBitrateOnFrameLengthDecreased) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  constexpr int kInitialFrameLengthMs = 60;
  BitrateController controller(
      BitrateController::Config(32000, kInitialFrameLengthMs));

  constexpr int kTargetBitrateBps = 48000;
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kBitrateBps =
      kTargetBitrateBps -
      kOverheadBytesPerPacket * 8 * 1000 / kInitialFrameLengthMs;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(), kBitrateBps);

  constexpr int kFrameLengthMs = 20;
  constexpr size_t kPacketOverheadRateDiff =
      kOverheadBytesPerPacket * 8 * 1000 / 20 -
      kOverheadBytesPerPacket * 8 * 1000 / 60;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(kFrameLengthMs),
                kBitrateBps - kPacketOverheadRateDiff);
}

TEST(AnaBitrateControllerTest, BitrateNeverBecomesNegative) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  BitrateController controller(BitrateController::Config(32000, 20));
  constexpr size_t kOverheadBytesPerPacket = 64;
  constexpr int kFrameLengthMs = 60;
  // Set a target rate smaller than overhead rate, the bitrate is bounded by 0.
  constexpr int kTargetBitrateBps =
      kOverheadBytesPerPacket * 8 * 1000 / kFrameLengthMs - 1;
  CheckDecision(&controller, rtc::Optional<int>(kTargetBitrateBps),
                rtc::Optional<size_t>(kOverheadBytesPerPacket),
                rtc::Optional<int>(kFrameLengthMs), 0);
}

TEST(AnaBitrateControllerTest, CheckBehaviorOnChangingCondition) {
  test::ScopedFieldTrials override_field_trials(
      "WebRTC-SendSideBwe-WithOverhead/Enabled/");
  BitrateController controller(BitrateController::Config(32000, 20));

  // Start from an arbitrary overall bitrate.
  int overall_bitrate = 34567;
  size_t overhead_bytes_per_packet = 64;
  int frame_length_ms = 20;
  int current_bitrate =
      overall_bitrate - overhead_bytes_per_packet * 8 * 1000 / frame_length_ms;

  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);

  // Next: increase overall bitrate.
  overall_bitrate += 100;
  current_bitrate += 100;
  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);

  // Next: change frame length.
  frame_length_ms = 60;
  current_bitrate += overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;
  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);

  // Next: change overhead.
  overhead_bytes_per_packet -= 30;
  current_bitrate += 30 * 8 * 1000 / frame_length_ms;
  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);

  // Next: change frame length.
  frame_length_ms = 20;
  current_bitrate -= overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;
  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);

  // Next: decrease overall bitrate and frame length.
  overall_bitrate -= 100;
  current_bitrate -= 100;
  frame_length_ms = 60;
  current_bitrate += overhead_bytes_per_packet * 8 * 1000 / 20 -
                     overhead_bytes_per_packet * 8 * 1000 / 60;

  CheckDecision(&controller, rtc::Optional<int>(overall_bitrate),
                rtc::Optional<size_t>(overhead_bytes_per_packet),
                rtc::Optional<int>(frame_length_ms), current_bitrate);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
