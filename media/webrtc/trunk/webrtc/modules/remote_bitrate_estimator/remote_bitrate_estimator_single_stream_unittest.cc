/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

class RemoteBitrateEstimatorSingleTest : public RemoteBitrateEstimatorTest {
 public:
  RemoteBitrateEstimatorSingleTest() {}
  virtual void SetUp() {
    bitrate_estimator_.reset(RemoteBitrateEstimatorFactory().Create(
        bitrate_observer_.get(),
        &clock_));
  }
 protected:
  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorSingleTest);
};

TEST_F(RemoteBitrateEstimatorSingleTest, InitialBehavior) {
  InitialBehaviorTestHelper(498075);
}

TEST_F(RemoteBitrateEstimatorSingleTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper();
}

TEST_F(RemoteBitrateEstimatorSingleTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper();
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same.
TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 956214, 367);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario.
TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 956214, 367);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropTwoStreamsWrap) {
  CapacityDropTestHelper(2, true, 927088, 267);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario. This is a multi-stream test.
TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThreeStreamsWrap) {
  CapacityDropTestHelper(3, true, 920944, 333);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThirteenStreamsWrap) {
  CapacityDropTestHelper(13, true, 938944, 300);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropNineteenStreamsWrap) {
  CapacityDropTestHelper(19, true, 926718, 300);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThirtyStreamsWrap) {
  CapacityDropTestHelper(30, true, 927016, 300);
}
}  // namespace webrtc
