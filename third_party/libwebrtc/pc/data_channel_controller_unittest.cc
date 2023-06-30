/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/data_channel_controller.h"

#include <memory>

#include "pc/peer_connection_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/test/mock_peer_connection_internal.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

namespace webrtc {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

class DataChannelControllerTest : public ::testing::Test {
 protected:
  DataChannelControllerTest() {
    pc_ = rtc::make_ref_counted<NiceMock<MockPeerConnectionInternal>>();
    ON_CALL(*pc_, signaling_thread)
        .WillByDefault(Return(rtc::Thread::Current()));
  }

  ~DataChannelControllerTest() override { run_loop_.Flush(); }

  test::RunLoop run_loop_;
  rtc::scoped_refptr<NiceMock<MockPeerConnectionInternal>> pc_;
};

TEST_F(DataChannelControllerTest, CreateAndDestroy) {
  DataChannelController dcc(pc_.get());
}

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyRelease) {
  DataChannelController dcc(pc_.get());
  auto channel = dcc.InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  channel = nullptr;  // dcc holds a reference to channel, so not destroyed yet
}

TEST_F(DataChannelControllerTest, CreateDataChannelLateRelease) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto channel = dcc->InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  dcc.reset();
  channel = nullptr;
}

TEST_F(DataChannelControllerTest, CloseAfterControllerDestroyed) {
  auto dcc = std::make_unique<DataChannelController>(pc_.get());
  auto channel = dcc->InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  // Connect to provider
  auto inner_channel =
      DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
          channel.get());
  dcc->ConnectDataChannel(inner_channel);
  dcc.reset();
  channel->Close();
}

TEST_F(DataChannelControllerTest, AsyncChannelCloseTeardown) {
  DataChannelController dcc(pc_.get());
  rtc::scoped_refptr<DataChannelInterface> channel =
      dcc.InternalCreateDataChannelWithProxy(
          "label",
          std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  SctpDataChannel* inner_channel =
      DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
          channel.get());
  // Grab a reference for testing purposes.
  inner_channel->AddRef();

  channel = nullptr;  // dcc still holds a reference to `channel`.
  EXPECT_TRUE(dcc.HasDataChannels());

  // Make sure callbacks to dcc are set up.
  dcc.ConnectDataChannel(inner_channel);

  // Trigger a Close() for the channel. This will send events back to dcc,
  // eventually reaching `OnSctpDataChannelClosed` where dcc removes
  // the channel from the internal list of data channels, but does not release
  // the reference synchronously since that reference might be the last one.
  inner_channel->Close();
  // Now there should be no tracked data channels.
  EXPECT_FALSE(dcc.HasDataChannels());
  // But there should be an async operation queued that still holds a reference.
  // That means that the test reference, must not be the last one.
  ASSERT_NE(inner_channel->Release(),
            rtc::RefCountReleaseStatus::kDroppedLastRef);
  // Grab a reference again (using the pointer is safe since the object still
  // exists and we control the single-threaded environment manually).
  inner_channel->AddRef();
  // Now run the queued up async operations on the signaling (current) thread.
  // This time, the reference formerly owned by dcc, should be release and the
  // truly last reference is now held by the test.
  run_loop_.Flush();
  // Check that this is the last reference.
  EXPECT_EQ(inner_channel->Release(),
            rtc::RefCountReleaseStatus::kDroppedLastRef);
}

}  // namespace
}  // namespace webrtc
