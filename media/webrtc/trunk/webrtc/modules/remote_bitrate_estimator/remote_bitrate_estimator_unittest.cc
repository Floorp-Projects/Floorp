/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file includes unit tests for RemoteBitrateEstimator.

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"
#include "system_wrappers/interface/constructor_magic.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

TEST_F(RemoteBitrateEstimatorTest, TestInitialBehavior) {
  const int kFramerate = 50;  // 50 fps to avoid rounding errors.
  const int kFrameIntervalMs = 1000 / kFramerate;
  unsigned int bitrate_bps = 0;
  uint32_t timestamp = 0;
  std::vector<unsigned int> ssrcs;
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(&ssrcs, &bitrate_bps));
  EXPECT_EQ(0u, ssrcs.size());
  clock_.AdvanceTimeMilliseconds(1000);
  bitrate_estimator_->Process();
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(&ssrcs, &bitrate_bps));
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  clock_.AdvanceTimeMilliseconds(1000);
  // Inserting a packet. Still no valid estimate. We need to wait 1 second.
  bitrate_estimator_->IncomingPacket(kDefaultSsrc, kMtu,
                                     clock_.TimeInMilliseconds(), timestamp);
  bitrate_estimator_->Process();
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(&ssrcs, &bitrate_bps));
  EXPECT_EQ(0u, ssrcs.size());
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  // Inserting packets for one second to get a valid estimate.
  for (int i = 0; i < kFramerate; ++i) {
    bitrate_estimator_->IncomingPacket(kDefaultSsrc, kMtu,
                                       clock_.TimeInMilliseconds(), timestamp);
    clock_.AdvanceTimeMilliseconds(1000 / kFramerate);
    timestamp += 90 * kFrameIntervalMs;
  }
  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_estimator_->LatestEstimate(&ssrcs, &bitrate_bps));
  ASSERT_EQ(1u, ssrcs.size());
  EXPECT_EQ(kDefaultSsrc, ssrcs.front());
  EXPECT_EQ(498075u, bitrate_bps);
  EXPECT_TRUE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  EXPECT_EQ(bitrate_observer_->latest_bitrate(), bitrate_bps);
}

TEST_F(RemoteBitrateEstimatorTest, TestRateIncreaseReordering) {
  uint32_t timestamp = 0;
  const int kFramerate = 50;  // 50 fps to avoid rounding errors.
  const int kFrameIntervalMs = 1000 / kFramerate;
  bitrate_estimator_->IncomingPacket(kDefaultSsrc, 1000,
                                     clock_.TimeInMilliseconds(), timestamp);
  bitrate_estimator_->Process();
  EXPECT_FALSE(bitrate_observer_->updated());  // No valid estimate.
  // Inserting packets for one second to get a valid estimate.
  for (int i = 0; i < kFramerate; ++i) {
    bitrate_estimator_->IncomingPacket(kDefaultSsrc, kMtu,
                                       clock_.TimeInMilliseconds(), timestamp);
    clock_.AdvanceTimeMilliseconds(kFrameIntervalMs);
    timestamp += 90 * kFrameIntervalMs;
  }
  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_EQ(498136u, bitrate_observer_->latest_bitrate());
  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds(2 * kFrameIntervalMs);
    timestamp += 2 * 90 * kFrameIntervalMs;
    bitrate_estimator_->IncomingPacket(kDefaultSsrc, 1000,
                                       clock_.TimeInMilliseconds(), timestamp);
    bitrate_estimator_->IncomingPacket(kDefaultSsrc,
                                       1000,
                                       clock_.TimeInMilliseconds() -
                                           kFrameIntervalMs,
                                       timestamp - 90 * kFrameIntervalMs);
  }
  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_EQ(498136u, bitrate_observer_->latest_bitrate());
}

// Make sure we initially increase the bitrate as expected.
TEST_F(RemoteBitrateEstimatorTest, TestRateIncreaseRtpTimestamps) {
  // This threshold corresponds approximately to increasing linearly with
  // bitrate(i) = 1.04 * bitrate(i-1) + 1000
  // until bitrate(i) > 500000, with bitrate(1) ~= 30000.
  const int kExpectedIterations = 1621;
  unsigned int bitrate_bps = 30000;
  int iterations = 0;
  AddDefaultStream();
  // Feed the estimator with a stream of packets and verify that it reaches
  // 500 kbps at the expected time.
  while (bitrate_bps < 5e5) {
    bool overuse = GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    if (overuse) {
      EXPECT_GT(bitrate_observer_->latest_bitrate(), bitrate_bps);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    ++iterations;
    ASSERT_LE(iterations, kExpectedIterations);
  }
  ASSERT_EQ(kExpectedIterations, iterations);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestamps) {
  const int kNumberOfFrames = 300;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  AddDefaultStream();
  // Run in steady state to make the estimator converge.
  unsigned int capacity_bps = 1000e3;
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc, kNumberOfFrames,
                                            kStartBitrate, kMinExpectedBitrate,
                                            kMaxExpectedBitrate, capacity_bps);
  // Reduce the capacity and verify the decrease time.
  capacity_bps = 500e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= capacity_bps) {
        bitrate_drop_time = clock_.TimeInMilliseconds();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(367, bitrate_drop_time - overuse_start_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestampsWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 8;  // Seconds.
  AddDefaultStream();
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  unsigned int capacity_bps = 1000e3;
  stream_generator_->set_capacity_bps(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate,
                                            capacity_bps);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  capacity_bps = 500e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= capacity_bps) {
        bitrate_drop_time = clock_.TimeInMilliseconds();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(367, bitrate_drop_time - overuse_start_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This test also converts the timestamps
// to NTP time.
TEST_F(RemoteBitrateEstimatorTestAlign, TestCapacityDropRtpTimestampsWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateTime = 8;  // Seconds.
  AddDefaultStream();
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  unsigned int capacity_bps = 1000e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate,
                                            capacity_bps);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  capacity_bps = 500e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= capacity_bps) {
        bitrate_drop_time = clock_.TimeInMilliseconds();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(367, bitrate_drop_time - overuse_start_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorTestAlign, TwoStreamsCapacityDropWithWrap) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateFrames = 9 * kFramerate;
  stream_generator_->AddStream(new testing::RtpStream(
      30,               // Frames per second.
      kStartBitrate/2,  // Bitrate.
      1,           // SSRC.
      90000,       // RTP frequency.
      0xFFFFF000,  // Timestamp offset.
      0));         // RTCP receive time.

  stream_generator_->AddStream(new testing::RtpStream(
      15,               // Frames per second.
      kStartBitrate/2,  // Bitrate.
      2,           // SSRC.
      90000,       // RTP frequency.
      0x00000FFF,  // Timestamp offset.
      0));         // RTCP receive time.
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateFrames * 90000);
  // Run in steady state to make the estimator converge.
  unsigned int capacity_bps = 1000e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateFrames,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate,
                                            capacity_bps);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  capacity_bps = 500e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= capacity_bps) {
        bitrate_drop_time = clock_.TimeInMilliseconds();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(567, bitrate_drop_time - overuse_start_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorTestAlign, ThreeStreams) {
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1100e3;
  const int kSteadyStateFrames = 12 * kFramerate;
  stream_generator_->AddStream(new testing::RtpStream(
      kFramerate,       // Frames per second.
      kStartBitrate/2,  // Bitrate.
      1,            // SSRC.
      90000,        // RTP frequency.
      0xFFFFF000,   // Timestamp offset.
      0));          // RTCP receive time.

  stream_generator_->AddStream(new testing::RtpStream(
      kFramerate,       // Frames per second.
      kStartBitrate/3,  // Bitrate.
      2,            // SSRC.
      90000,        // RTP frequency.
      0x00000FFF,   // Timestamp offset.
      0));          // RTCP receive time.

  stream_generator_->AddStream(new testing::RtpStream(
      kFramerate,       // Frames per second.
      kStartBitrate/6,  // Bitrate.
      3,            // SSRC.
      90000,        // RTP frequency.
      0x00000FFF,   // Timestamp offset.
      0));          // RTCP receive time.
  // Trigger wrap right after the steady state run.
  stream_generator_->set_rtp_timestamp_offset(kDefaultSsrc,
      std::numeric_limits<uint32_t>::max() - kSteadyStateFrames * 90000);
  // Run in steady state to make the estimator converge.
  unsigned int capacity_bps = 1000e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  unsigned int bitrate_bps = SteadyStateRun(kDefaultSsrc,
                                            kSteadyStateFrames,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate,
                                            capacity_bps);
  bitrate_observer_->Reset();
  // Reduce the capacity and verify the decrease time.
  capacity_bps = 500e3;
  stream_generator_->set_capacity_bps(capacity_bps);
  int64_t overuse_start_time = clock_.TimeInMilliseconds();
  int64_t bitrate_drop_time = -1;
  for (int i = 0; i < 200; ++i) {
    GenerateAndProcessFrame(kDefaultSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_drop_time == -1 &&
          bitrate_observer_->latest_bitrate() <= capacity_bps) {
        bitrate_drop_time = clock_.TimeInMilliseconds();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(433, bitrate_drop_time - overuse_start_time);
}

}  // namespace webrtc
