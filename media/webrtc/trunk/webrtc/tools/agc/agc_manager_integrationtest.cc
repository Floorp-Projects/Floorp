/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_processing/agc/mock_agc.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;

namespace webrtc {

class AgcManagerTest : public ::testing::Test {
 protected:
  AgcManagerTest()
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        agc_(new MockAgc()),
        manager_(new AgcManager(VoEExternalMedia::GetInterface(voe_),
                                VoEVolumeControl::GetInterface(voe_),
                                agc_,
                                AudioProcessing::Create())),
        channel_(-1) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(voe_ != NULL);
    ASSERT_TRUE(base_ != NULL);
    ASSERT_EQ(0, base_->Init());
    channel_ = base_->CreateChannel();
    ASSERT_NE(-1, channel_);

    VoENetwork* network = VoENetwork::GetInterface(voe_);
    ASSERT_TRUE(network != NULL);
    channel_transport_.reset(
        new test::VoiceChannelTransport(network, channel_));
    ASSERT_EQ(0, channel_transport_->SetSendDestination("127.0.0.1", 1234));
    network->Release();
  }

  virtual void TearDown() {
    channel_transport_.reset(NULL);
    ASSERT_EQ(0, base_->DeleteChannel(channel_));
    ASSERT_EQ(0, base_->Terminate());
    delete manager_;
    // Test that the manager has released all VoE interfaces. The last
    // reference is released in VoiceEngine::Delete.
    EXPECT_EQ(1, base_->Release());
    ASSERT_TRUE(VoiceEngine::Delete(voe_));
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  MockAgc* agc_;
  rtc::scoped_ptr<test::VoiceChannelTransport> channel_transport_;
  // We use a pointer for the manager, so we can tear it down and test
  // base_->Release() in the destructor.
  AgcManager* manager_;
  int channel_;
};

TEST_F(AgcManagerTest, DISABLED_ON_ANDROID(EnableSucceeds)) {
  EXPECT_EQ(0, manager_->Enable(true));
  EXPECT_TRUE(manager_->enabled());
  EXPECT_EQ(0, manager_->Enable(false));
  EXPECT_FALSE(manager_->enabled());
}

TEST_F(AgcManagerTest, DISABLED_ON_ANDROID(ProcessIsNotCalledByDefault)) {
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _)).Times(0);
  EXPECT_CALL(*agc_, Process(_, _, _)).Times(0);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_)).Times(0);
  ASSERT_EQ(0, base_->StartSend(channel_));
  SleepMs(100);
  ASSERT_EQ(0, base_->StopSend(channel_));
}

TEST_F(AgcManagerTest, DISABLED_ProcessIsCalledOnlyWhenEnabled) {
  EXPECT_CALL(*agc_, Reset());
  EXPECT_CALL(*agc_, AnalyzePreproc(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*agc_, Process(_, _, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*agc_, GetRmsErrorDb(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(false));
  EXPECT_EQ(0, manager_->Enable(true));
  ASSERT_EQ(0, base_->StartSend(channel_));
  SleepMs(100);
  EXPECT_EQ(0, manager_->Enable(false));
  SleepMs(100);
  Mock::VerifyAndClearExpectations(agc_);

  EXPECT_CALL(*agc_, AnalyzePreproc(_, _)).Times(0);
  EXPECT_CALL(*agc_, Process(_, _, _)).Times(0);
  EXPECT_CALL(*agc_, GetRmsErrorDb(_)).Times(0);
  SleepMs(100);
  ASSERT_EQ(0, base_->StopSend(channel_));
}

}  // namespace webrtc
