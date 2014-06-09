/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {
std::vector<const PacketSenderFactory*> VideoSenderFactories(uint32_t count) {
  static const VideoPacketSenderFactory factories[] = {
    VideoPacketSenderFactory(30.00f, 150, 0x1234, 0.13f),
    VideoPacketSenderFactory(15.00f, 500, 0x2345, 0.16f),
    VideoPacketSenderFactory(30.00f, 1200, 0x3456, 0.26f),
    VideoPacketSenderFactory(7.49f, 150, 0x4567, 0.05f),
    VideoPacketSenderFactory(7.50f, 150, 0x5678, 0.15f),
    VideoPacketSenderFactory(7.51f, 150, 0x6789, 0.25f),
    VideoPacketSenderFactory(15.02f, 150, 0x7890, 0.27f),
    VideoPacketSenderFactory(15.03f, 150, 0x8901, 0.38f),
    VideoPacketSenderFactory(30.02f, 150, 0x9012, 0.39f),
    VideoPacketSenderFactory(30.03f, 150, 0x0123, 0.52f)
  };

  assert(count <= sizeof(factories) / sizeof(factories[0]));

  std::vector<const PacketSenderFactory*> result;
  for (uint32_t i = 0; i < count; ++i) {
    result.push_back(&factories[i]);
  }

  return result;
}

std::vector<BweTestConfig::EstimatorConfig> EstimatorConfigs() {
  static const RemoteBitrateEstimatorFactory factories[] = {
    RemoteBitrateEstimatorFactory(),
    AbsoluteSendTimeRemoteBitrateEstimatorFactory()
  };

  std::vector<BweTestConfig::EstimatorConfig> result;
  result.push_back(BweTestConfig::EstimatorConfig("TOF", &factories[0]));
  result.push_back(BweTestConfig::EstimatorConfig("AST", &factories[1]));
  return result;
}

BweTestConfig MakeBweTestConfig(uint32_t sender_count) {
  BweTestConfig result = {
    VideoSenderFactories(sender_count), EstimatorConfigs()
  };
  return result;
}

INSTANTIATE_TEST_CASE_P(VideoSendersTest, BweTest,
    ::testing::Values(MakeBweTestConfig(1),
                      MakeBweTestConfig(3)));

TEST_P(BweTest, UnlimitedSpeed) {
  VerboseLogging(false);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, SteadyLoss) {
  LossFilter loss(this);
  loss.SetLoss(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingLoss1) {
  LossFilter loss(this);
  for (int i = 0; i < 76; ++i) {
    loss.SetLoss(i);
    RunFor(5000);
  }
}

TEST_P(BweTest, SteadyDelay) {
  DelayFilter delay(this);
  delay.SetDelay(1000);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingDelay1) {
  DelayFilter delay(this);
  RunFor(10 * 60 * 1000);
  for (int i = 0; i < 30 * 2; ++i) {
    delay.SetDelay(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingDelay2) {
  DelayFilter delay(this);
  RateCounterFilter counter(this);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(10.0f * i);
    RunFor(10 * 1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, JumpyDelay1) {
  DelayFilter delay(this);
  RunFor(10 * 60 * 1000);
  for (int i = 1; i < 200; ++i) {
    delay.SetDelay((10 * i) % 500);
    RunFor(1000);
    delay.SetDelay(1.0f);
    RunFor(1000);
  }
  delay.SetDelay(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, SteadyJitter) {
  JitterFilter jitter(this);
  RateCounterFilter counter(this);
  jitter.SetJitter(20);
  RunFor(2 * 60 * 1000);
}

TEST_P(BweTest, IncreasingJitter1) {
  JitterFilter jitter(this);
  for (int i = 0; i < 2 * 60 * 2; ++i) {
    jitter.SetJitter(i);
    RunFor(10 * 1000);
  }
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingJitter2) {
  JitterFilter jitter(this);
  RunFor(30 * 1000);
  for (int i = 1; i < 51; ++i) {
    jitter.SetJitter(10.0f * i);
    RunFor(10 * 1000);
  }
  jitter.SetJitter(0.0f);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, SteadyReorder) {
  ReorderFilter reorder(this);
  reorder.SetReorder(20.0);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingReorder1) {
  ReorderFilter reorder(this);
  for (int i = 0; i < 76; ++i) {
    reorder.SetReorder(i);
    RunFor(5000);
  }
}

TEST_P(BweTest, SteadyChoke) {
  ChokeFilter choke(this);
  choke.SetCapacity(140);
  RunFor(10 * 60 * 1000);
}

TEST_P(BweTest, IncreasingChoke1) {
  ChokeFilter choke(this);
  for (int i = 1200; i >= 100; i -= 100) {
    choke.SetCapacity(i);
    RunFor(5000);
  }
}

TEST_P(BweTest, IncreasingChoke2) {
  ChokeFilter choke(this);
  RunFor(60 * 1000);
  for (int i = 1200; i >= 100; i -= 20) {
    choke.SetCapacity(i);
    RunFor(1000);
  }
}

TEST_P(BweTest, Multi1) {
  DelayFilter delay(this);
  ChokeFilter choke(this);
  RateCounterFilter counter(this);
  choke.SetCapacity(1000);
  RunFor(1 * 60 * 1000);
  for (int i = 1; i < 51; ++i) {
    delay.SetDelay(100.0f * i);
    RunFor(10 * 1000);
  }
  RunFor(500 * 1000);
  delay.SetDelay(0.0f);
  RunFor(5 * 60 * 1000);
}

TEST_P(BweTest, Multi2) {
  ChokeFilter choke(this);
  JitterFilter jitter(this);
  RateCounterFilter counter(this);
  choke.SetCapacity(2000);
  jitter.SetJitter(120);
  RunFor(5 * 60 * 1000);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
