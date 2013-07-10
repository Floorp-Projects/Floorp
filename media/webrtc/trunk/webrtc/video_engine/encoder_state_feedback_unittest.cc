/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// This file includes unit tests for EncoderStateFeedback.
#include "video_engine/encoder_state_feedback.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "modules/utility/interface/process_thread.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "video_engine/vie_encoder.h"

namespace webrtc {

// TODO(mflodman) Create a common mock in module utility.
class TestProcessThread : public ProcessThread {
 public:
  TestProcessThread() {}
  ~TestProcessThread() {}
  virtual int32_t Start() { return 0; }
  virtual int32_t Stop() { return 0; }
  virtual int32_t RegisterModule(const Module* module) { return 0; }
  virtual int32_t DeRegisterModule(const Module* module) { return 0; }
};

class MockVieEncoder : public ViEEncoder {
 public:
  explicit MockVieEncoder(TestProcessThread* process_thread)
      : ViEEncoder(1, 1, 1, *process_thread, NULL) {}
  ~MockVieEncoder() {}

  MOCK_METHOD1(OnReceivedIntraFrameRequest,
               void(uint32_t));
  MOCK_METHOD2(OnReceivedSLI,
               void(uint32_t ssrc, uint8_t picture_id));
  MOCK_METHOD2(OnReceivedRPSI,
               void(uint32_t ssrc, uint64_t picture_id));
  MOCK_METHOD2(OnLocalSsrcChanged,
               void(uint32_t old_ssrc, uint32_t new_ssrc));
};

class VieKeyRequestTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    process_thread_.reset(new TestProcessThread());
    encoder_state_feedback_.reset(new EncoderStateFeedback());
  }
  scoped_ptr<TestProcessThread> process_thread_;
  scoped_ptr<EncoderStateFeedback> encoder_state_feedback_;
};

TEST_F(VieKeyRequestTest, CreateAndTriggerRequests) {
  const int ssrc = 1234;
  MockVieEncoder encoder(process_thread_.get());
  EXPECT_TRUE(encoder_state_feedback_->AddEncoder(ssrc, &encoder));

  EXPECT_CALL(encoder, OnReceivedIntraFrameRequest(ssrc))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->
      OnReceivedIntraFrameRequest(ssrc);

  const uint8_t sli_picture_id = 3;
  EXPECT_CALL(encoder, OnReceivedSLI(ssrc, sli_picture_id))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedSLI(
      ssrc, sli_picture_id);

  const uint64_t rpsi_picture_id = 9;
  EXPECT_CALL(encoder, OnReceivedRPSI(ssrc, rpsi_picture_id))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedRPSI(
      ssrc, rpsi_picture_id);

  encoder_state_feedback_->RemoveEncoder(&encoder);
}

// Register multiple encoders and make sure the request is relayed to correct
// ViEEncoder.
TEST_F(VieKeyRequestTest, MultipleEncoders) {
  const int ssrc_1 = 1234;
  const int ssrc_2 = 5678;
  MockVieEncoder encoder_1(process_thread_.get());
  MockVieEncoder encoder_2(process_thread_.get());
  EXPECT_TRUE(encoder_state_feedback_->AddEncoder(ssrc_1, &encoder_1));
  EXPECT_TRUE(encoder_state_feedback_->AddEncoder(ssrc_2, &encoder_2));

  EXPECT_CALL(encoder_1, OnReceivedIntraFrameRequest(ssrc_1))
      .Times(1);
  EXPECT_CALL(encoder_2, OnReceivedIntraFrameRequest(ssrc_2))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->
      OnReceivedIntraFrameRequest(ssrc_1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->
      OnReceivedIntraFrameRequest(ssrc_2);

  const uint8_t sli_pid_1 = 3;
  const uint8_t sli_pid_2 = 4;
  EXPECT_CALL(encoder_1, OnReceivedSLI(ssrc_1, sli_pid_1))
      .Times(1);
  EXPECT_CALL(encoder_2, OnReceivedSLI(ssrc_2, sli_pid_2))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedSLI(
      ssrc_1, sli_pid_1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedSLI(
      ssrc_2, sli_pid_2);

  const uint64_t rpsi_pid_1 = 9;
  const uint64_t rpsi_pid_2 = 10;
  EXPECT_CALL(encoder_1, OnReceivedRPSI(ssrc_1, rpsi_pid_1))
      .Times(1);
  EXPECT_CALL(encoder_2, OnReceivedRPSI(ssrc_2, rpsi_pid_2))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedRPSI(
      ssrc_1, rpsi_pid_1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->OnReceivedRPSI(
      ssrc_2, rpsi_pid_2);

  encoder_state_feedback_->RemoveEncoder(&encoder_1);
  EXPECT_CALL(encoder_2, OnReceivedIntraFrameRequest(ssrc_2))
      .Times(1);
  encoder_state_feedback_->GetRtcpIntraFrameObserver()->
      OnReceivedIntraFrameRequest(ssrc_2);
  encoder_state_feedback_->RemoveEncoder(&encoder_2);
}

TEST_F(VieKeyRequestTest, AddTwiceError) {
  const int ssrc = 1234;
  MockVieEncoder encoder(process_thread_.get());
  EXPECT_TRUE(encoder_state_feedback_->AddEncoder(ssrc, &encoder));
  EXPECT_FALSE(encoder_state_feedback_->AddEncoder(ssrc, &encoder));
  encoder_state_feedback_->RemoveEncoder(&encoder);
}

}  // namespace webrtc
