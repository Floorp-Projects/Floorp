/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// This file includes unit tests for ViERemb.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <vector>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/video_engine/vie_remb.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

class TestProcessThread : public ProcessThread {
 public:
  explicit TestProcessThread() {}
  ~TestProcessThread() {}
  virtual int32_t Start() { return 0; }
  virtual int32_t Stop() { return 0; }
  virtual int32_t RegisterModule(const Module* module) { return 0; }
  virtual int32_t DeRegisterModule(const Module* module) { return 0; }
};

class ViERembTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    TickTime::UseFakeClock(12345);
    process_thread_.reset(new TestProcessThread);
    vie_remb_.reset(new VieRemb());
  }
  scoped_ptr<TestProcessThread> process_thread_;
  scoped_ptr<VieRemb> vie_remb_;
};

TEST_F(ViERembTest, OneModuleTestForSendingRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  const unsigned int bitrate_estimate = 456;
  unsigned int ssrc = 1234;
  std::vector<unsigned int> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower bitrate to send another REMB packet.
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate - 100, 1, _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate - 100);

  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

TEST_F(ViERembTest, LowerEstimateToSendRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc = 1234;
  std::vector<unsigned int> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate with more than 3% to trigger a call to SetREMBData right
  // away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
}

TEST_F(ViERembTest, VerifyIncreasingAndDecreasing) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  unsigned int bitrate_estimate[] = { 456, 789 };
  unsigned int ssrc[] = { 1234, 5678 };
  std::vector<unsigned int> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[0], 2, _))
        .Times(1);
  TickTime::AdvanceFakeClock(1000);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[0]);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1] + 100);

  // Lower the estimate to trigger a callback.
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[1], 2, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate[1]);

  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveRembSender(&rtp_0);
  vie_remb_->RemoveReceiveChannel(&rtp_1);
}

TEST_F(ViERembTest, NoRembForIncreasedBitrate) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234, 5678 };
  std::vector<unsigned int> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, 2, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Increased estimate shouldn't trigger a callback right away.
  EXPECT_CALL(rtp_0, SetREMBData(_, _, _))
      .Times(0);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate + 1);

  // Decreasing the estimate less than 3% shouldn't trigger a new callback.
  EXPECT_CALL(rtp_0, SetREMBData(_, _, _))
      .Times(0);
  int lower_estimate = bitrate_estimate * 98 / 100;
  vie_remb_->OnReceiveBitrateChanged(ssrcs, lower_estimate);

  vie_remb_->RemoveReceiveChannel(&rtp_1);
  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveRembSender(&rtp_0);
}

TEST_F(ViERembTest, ChangeSendRtpModule) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234, 5678 };
  std::vector<unsigned int> ssrcs(ssrc, ssrc + sizeof(ssrc) / sizeof(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, 2, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Decrease estimate to trigger a REMB.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate, 2, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Remove the sending module, add it again -> should get remb on the second
  // module.
  vie_remb_->RemoveRembSender(&rtp_0);
  vie_remb_->AddRembSender(&rtp_1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp_1, SetREMBData(bitrate_estimate, 2, _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveReceiveChannel(&rtp_1);
}

TEST_F(ViERembTest, OnlyOneRembForDoubleProcess) {
  MockRtpRtcp rtp;
  unsigned int bitrate_estimate = 456;
  unsigned int ssrc = 1234;
  std::vector<unsigned int> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  // Call OnReceiveBitrateChanged twice to get a first estimate.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate, should trigger a call to SetREMBData right away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged again, this should not trigger a new callback.
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(0);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

// Only register receiving modules and make sure we fallback to trigger a REMB
// packet on this one.
TEST_F(ViERembTest, NoSendingRtpModule) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc = 1234;
  std::vector<unsigned int> ssrcs(&ssrc, &ssrc + 1);

  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Call OnReceiveBitrateChanged twice to get a first estimate.
  TickTime::AdvanceFakeClock(1000);
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);

  // Lower the estimate to trigger a new packet REMB packet.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrcs, bitrate_estimate);
}

}  // namespace webrtc
