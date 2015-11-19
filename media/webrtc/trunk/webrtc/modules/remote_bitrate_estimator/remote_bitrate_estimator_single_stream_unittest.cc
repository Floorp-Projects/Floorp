/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"

namespace webrtc {

class RemoteBitrateEstimatorSingleTest :
    public RemoteBitrateEstimatorTest {
 public:
  static const uint32_t kRemoteBitrateEstimatorMinBitrateBps = 30000;

  RemoteBitrateEstimatorSingleTest() {}
  virtual void SetUp() {
    bitrate_estimator_.reset(RemoteBitrateEstimatorFactory().Create(
        bitrate_observer_.get(),
        &clock_,
        kMimdControl,
        kRemoteBitrateEstimatorMinBitrateBps));
  }
 protected:
  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorSingleTest);
};

TEST_F(RemoteBitrateEstimatorSingleTest, InitialBehavior) {
  InitialBehaviorTestHelper(508017);
}

TEST_F(RemoteBitrateEstimatorSingleTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper(506422);
}

TEST_F(RemoteBitrateEstimatorSingleTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1621);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropTwoStreamsWrap) {
  CapacityDropTestHelper(2, true, 700);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThreeStreamsWrap) {
  CapacityDropTestHelper(3, true, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThirteenStreamsWrap) {
  CapacityDropTestHelper(13, true, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropNineteenStreamsWrap) {
  CapacityDropTestHelper(19, true, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, CapacityDropThirtyStreamsWrap) {
  CapacityDropTestHelper(30, true, 733);
}

TEST_F(RemoteBitrateEstimatorSingleTest, TestTimestampGrouping) {
  TestTimestampGroupingTestHelper();
}
}  // namespace webrtc
