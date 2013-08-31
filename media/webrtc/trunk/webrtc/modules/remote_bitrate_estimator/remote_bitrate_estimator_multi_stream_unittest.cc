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

class RemoteBitrateEstimatorMultiTest : public RemoteBitrateEstimatorTest {
 public:
  RemoteBitrateEstimatorMultiTest() {}
  virtual void SetUp() {
    bitrate_estimator_.reset(MultiStreamRemoteBitrateEstimatorFactory().Create(
        bitrate_observer_.get(),
        &clock_));
  }
 protected:
  DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorMultiTest);
};

TEST_F(RemoteBitrateEstimatorMultiTest, InitialBehavior) {
  InitialBehaviorTestHelper(497919);
}

TEST_F(RemoteBitrateEstimatorMultiTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper();
}

TEST_F(RemoteBitrateEstimatorMultiTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper();
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 956214, 367);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 956214, 367);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropOneStreamWrapAlign) {
  align_streams_ = true;
  CapacityDropTestHelper(1, true, 838645, 533);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropTwoStreamsWrapAlign) {
  align_streams_ = true;
  CapacityDropTestHelper(2, true, 810646, 433);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropThreeStreamsWrapAlign) {
  align_streams_ = true;
  CapacityDropTestHelper(3, true, 868522, 2067);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropThirteenStreamsWrap) {
  CapacityDropTestHelper(13, true, 918810, 433);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropNineteenStreamsWrap) {
  CapacityDropTestHelper(19, true, 919119, 433);
}

TEST_F(RemoteBitrateEstimatorMultiTest, CapacityDropThirtyStreamsWrap) {
  CapacityDropTestHelper(30, true, 918724, 433);
}
}  // namespace webrtc
