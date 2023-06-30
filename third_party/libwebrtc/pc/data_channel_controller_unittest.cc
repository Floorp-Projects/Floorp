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

class MockDataChannelTransport : public webrtc::DataChannelTransportInterface {
 public:
  ~MockDataChannelTransport() override {}

  MOCK_METHOD(RTCError, OpenChannel, (int channel_id), (override));
  MOCK_METHOD(RTCError,
              SendData,
              (int channel_id,
               const SendDataParams& params,
               const rtc::CopyOnWriteBuffer& buffer),
              (override));
  MOCK_METHOD(RTCError, CloseChannel, (int channel_id), (override));
  MOCK_METHOD(void, SetDataSink, (DataChannelSink * sink), (override));
  MOCK_METHOD(bool, IsReadyToSend, (), (const, override));
};

class DataChannelControllerTest : public ::testing::Test {
 protected:
  DataChannelControllerTest() {
    pc_ = rtc::make_ref_counted<NiceMock<MockPeerConnectionInternal>>();
    ON_CALL(*pc_, signaling_thread)
        .WillByDefault(Return(rtc::Thread::Current()));
    // TODO(tommi): Return a dedicated thread.
    ON_CALL(*pc_, network_thread).WillByDefault(Return(rtc::Thread::Current()));
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

TEST_F(DataChannelControllerTest, CreateDataChannelEarlyClose) {
  DataChannelController dcc(pc_.get());
  EXPECT_FALSE(dcc.HasDataChannels());
  EXPECT_FALSE(dcc.HasUsedDataChannels());
  auto channel = dcc.InternalCreateDataChannelWithProxy(
      "label",
      std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());
  EXPECT_TRUE(dcc.HasDataChannels());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
  channel->Close();
  EXPECT_FALSE(dcc.HasDataChannels());
  EXPECT_TRUE(dcc.HasUsedDataChannels());
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

// Allocate the maximum number of data channels and then one more.
// The last allocation should fail.
TEST_F(DataChannelControllerTest, MaxChannels) {
  NiceMock<MockDataChannelTransport> transport;
  int channel_id = 0;

  ON_CALL(*pc_, GetSctpSslRole).WillByDefault([&](rtc::SSLRole* role) {
    *role = (channel_id & 1) ? rtc::SSL_SERVER : rtc::SSL_CLIENT;
    return true;
  });

  DataChannelController dcc(pc_.get());
  pc_->network_thread()->BlockingCall(
      [&] { dcc.set_data_channel_transport(&transport); });

  // Allocate the maximum number of channels + 1. Inside the loop, the creation
  // process will allocate a stream id for each channel.
  for (channel_id = 0; channel_id <= cricket::kMaxSctpStreams; ++channel_id) {
    rtc::scoped_refptr<DataChannelInterface> channel =
        dcc.InternalCreateDataChannelWithProxy(
            "label",
            std::make_unique<InternalDataChannelInit>(DataChannelInit()).get());

    if (channel_id == cricket::kMaxSctpStreams) {
      // We've reached the maximum and the previous call should have failed.
      EXPECT_FALSE(channel.get());
    } else {
      // We're still working on saturating the pool. Things should be working.
      EXPECT_TRUE(channel.get());
    }
  }
}

}  // namespace
}  // namespace webrtc
