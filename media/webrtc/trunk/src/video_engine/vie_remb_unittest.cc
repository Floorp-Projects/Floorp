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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/utility/interface/process_thread.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "system_wrappers/interface/sleep.h"
#include "video_engine/vie_remb.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace webrtc {

// TODO(mflodman) Make a trigger function for this class to fake a clock and
// remove sleeps in the test.
class TestProcessThread : public ProcessThread {
 public:
  explicit TestProcessThread() {}
  ~TestProcessThread() {}
  virtual WebRtc_Word32 Start() { return 0; }
  virtual WebRtc_Word32 Stop() { return 0; }
  virtual WebRtc_Word32 RegisterModule(const Module* module) { return 0; }
  virtual WebRtc_Word32 DeRegisterModule(const Module* module) { return 0; }
};

class ViERembTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    process_thread_.reset(new TestProcessThread);
    vie_remb_.reset(new VieRemb(process_thread_.get()));
  }
  scoped_ptr<TestProcessThread> process_thread_;
  scoped_ptr<VieRemb> vie_remb_;
};

TEST_F(ViERembTest, OneModuleTestForSendingRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  const unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  EXPECT_CALL(rtp, RemoteSSRC())
      .WillRepeatedly(Return(ssrc[0]));

  // TODO(mflodman) Add fake clock and remove the lowered bitrate below.
  SleepMs(1010);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->Process();

  // Lower bitrate to send another REMB packet.
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate - 100);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate - 100, 1, _))
        .Times(1);
  vie_remb_->Process();

  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

TEST_F(ViERembTest, LowerEstimateToSendRemb) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  EXPECT_CALL(rtp, RemoteSSRC())
      .WillRepeatedly(Return(ssrc[0]));
  // Call process to get a first estimate.
  SleepMs(1010);
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
        .Times(1);
  vie_remb_->Process();

  // Lower the estimate with more than 3% to trigger a call to SetREMBData right
  // away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  vie_remb_->Process();
}

TEST_F(ViERembTest, VerifyCombinedBitrateEstimate) {
  MockRtpRtcp rtp_0;
  MockRtpRtcp rtp_1;
  vie_remb_->AddReceiveChannel(&rtp_0);
  vie_remb_->AddRembSender(&rtp_0);
  vie_remb_->AddReceiveChannel(&rtp_1);

  unsigned int bitrate_estimate[] = { 456, 789 };
  unsigned int ssrc[] = { 1234, 5678 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0]);
  EXPECT_CALL(rtp_0, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[0]));

  // Call process to get a first estimate.
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[0], 1, _))
        .Times(1);
  SleepMs(1010);
  vie_remb_->Process();

  vie_remb_->OnReceiveBitrateChanged(ssrc[1], bitrate_estimate[1] + 100);
  EXPECT_CALL(rtp_1, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[1]));

  // Lower the estimate to trigger a callback.
  int total_bitrate = bitrate_estimate[0] + bitrate_estimate[1];
  EXPECT_CALL(rtp_0, SetREMBData(total_bitrate, 2, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[1], bitrate_estimate[1]);
  vie_remb_->Process();

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

  unsigned int bitrate_estimate[] = { 456, 789 };
  unsigned int ssrc[] = { 1234, 5678 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0]);
  EXPECT_CALL(rtp_0, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrc[1], bitrate_estimate[1]);
  EXPECT_CALL(rtp_1, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[1]));

  // Trigger a first call to have a running state.
  // TODO(mflodman) Add fake clock.
  SleepMs(1010);
  EXPECT_CALL(rtp_0,
              SetREMBData(bitrate_estimate[0] + bitrate_estimate[1], 2, _))
      .Times(1);
  vie_remb_->Process();

  // Increased estimate shouldn't trigger a callback right away.
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0] + 1);
  EXPECT_CALL(rtp_0, SetREMBData(_, _, _))
      .Times(0);

  // Decresing the estimate less than 3% shouldn't trigger a new callback.
  int lower_estimate = bitrate_estimate[0] * 98 / 100;
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], lower_estimate);
  EXPECT_CALL(rtp_0, SetREMBData(_, _, _))
      .Times(0);

  vie_remb_->Process();
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

  unsigned int bitrate_estimate[] = { 456, 789 };
  unsigned int ssrc[] = { 1234, 5678 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0]);
  EXPECT_CALL(rtp_0, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[0]));

  vie_remb_->OnReceiveBitrateChanged(ssrc[1], bitrate_estimate[1]);
  EXPECT_CALL(rtp_1, RemoteSSRC())
      .Times(AnyNumber())
      .WillRepeatedly(Return(ssrc[1]));

  // Call process to get a first estimate.
  SleepMs(1010);
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[0] + bitrate_estimate[1], 2,
                                 _))
      .Times(1);
  vie_remb_->Process();

  // Decrease estimate to trigger a REMB.
  bitrate_estimate[0] = bitrate_estimate[0] - 100;
  EXPECT_CALL(rtp_0, SetREMBData(bitrate_estimate[0] + bitrate_estimate[1], 2,
                                 _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0]);
  vie_remb_->Process();

  // Remove the sending module, add it again -> should get remb on the second
  // module.
  vie_remb_->RemoveRembSender(&rtp_0);
  vie_remb_->AddRembSender(&rtp_1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate[0]);

  bitrate_estimate[1] = bitrate_estimate[1] - 100;
  EXPECT_CALL(rtp_1, SetREMBData(bitrate_estimate[0] + bitrate_estimate[1], 2,
                                 _))
        .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[1], bitrate_estimate[1]);
  vie_remb_->Process();

  vie_remb_->RemoveReceiveChannel(&rtp_0);
  vie_remb_->RemoveReceiveChannel(&rtp_1);
}

TEST_F(ViERembTest, OnlyOneRembForDoubleProcess) {
  MockRtpRtcp rtp;
  unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234 };

  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  EXPECT_CALL(rtp, RemoteSSRC())
      .WillRepeatedly(Return(ssrc[0]));

  // Call process to get a first estimate.
  SleepMs(1010);
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
        .Times(1);
  vie_remb_->Process();

  // Lower the estimate, should trigger a call to SetREMBData right away.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(bitrate_estimate, 1, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  vie_remb_->Process();

  // Call Process again, this should not trigger a new callback.
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(0);
  vie_remb_->Process();
  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

TEST_F(ViERembTest, NoOnReceivedBitrateChangedCall) {
  MockRtpRtcp rtp;
  EXPECT_CALL(rtp, RemoteSSRC())
        .WillRepeatedly(Return(1234));

  vie_remb_->AddReceiveChannel(&rtp);
  vie_remb_->AddRembSender(&rtp);
  // TODO(mflodman) Add fake clock.
  SleepMs(1010);
  // No bitrate estimate given, no callback expected.
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(0);
  vie_remb_->Process();

  vie_remb_->RemoveReceiveChannel(&rtp);
  vie_remb_->RemoveRembSender(&rtp);
}

// Only register receiving modules and make sure we fallback to trigger a REMB
// packet on this one.
TEST_F(ViERembTest, NoSendingRtpModule) {
  MockRtpRtcp rtp;
  vie_remb_->AddReceiveChannel(&rtp);

  unsigned int bitrate_estimate = 456;
  unsigned int ssrc[] = { 1234 };

  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  EXPECT_CALL(rtp, RemoteSSRC())
      .WillRepeatedly(Return(ssrc[0]));

  // Call process to get a first estimate.
  SleepMs(1010);
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(1);
  vie_remb_->Process();

  // Lower the estimate to trigger a new packet REMB packet.
  bitrate_estimate = bitrate_estimate - 100;
  EXPECT_CALL(rtp, SetREMBData(_, _, _))
      .Times(1);
  vie_remb_->OnReceiveBitrateChanged(ssrc[0], bitrate_estimate);
  vie_remb_->Process();
}

}  // namespace webrtc
