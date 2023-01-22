/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/sctp/dcsctp_transport.h"

#include <memory>
#include <utility>

#include "net/dcsctp/public/mock_dcsctp_socket.h"
#include "net/dcsctp/public/mock_dcsctp_socket_factory.h"
#include "p2p/base/fake_packet_transport.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnPointee;

namespace webrtc {

namespace {
class MockDataChannelObserver : public DataChannelSink {
 public:
  MOCK_METHOD(void, OnConnected, ());

  // DataChannelSink
  MOCK_METHOD(void,
              OnDataReceived,
              (int, DataMessageType, const rtc::CopyOnWriteBuffer&));
  MOCK_METHOD(void, OnChannelClosing, (int));
  MOCK_METHOD(void, OnChannelClosed, (int));
  MOCK_METHOD(void, OnReadyToSend, ());
  MOCK_METHOD(void, OnTransportClosed, (RTCError));
};

class Peer {
 public:
  Peer() : fake_packet_transport_("transport"), simulated_clock_(1000) {
    auto socket_ptr = std::make_unique<dcsctp::MockDcSctpSocket>();
    socket_ = socket_ptr.get();

    auto mock_dcsctp_socket_factory =
        std::make_unique<dcsctp::MockDcSctpSocketFactory>();
    EXPECT_CALL(*mock_dcsctp_socket_factory, Create)
        .Times(1)
        .WillOnce(Return(ByMove(std::move(socket_ptr))));

    sctp_transport_ = std::make_unique<webrtc::DcSctpTransport>(
        rtc::Thread::Current(), &fake_packet_transport_, &simulated_clock_,
        std::move(mock_dcsctp_socket_factory));
    sctp_transport_->SetDataChannelSink(&observer_);
    sctp_transport_->SetOnConnectedCallback(
        [this]() { observer_.OnConnected(); });
  }

  rtc::FakePacketTransport fake_packet_transport_;
  webrtc::SimulatedClock simulated_clock_;
  dcsctp::MockDcSctpSocket* socket_;
  std::unique_ptr<webrtc::DcSctpTransport> sctp_transport_;
  NiceMock<MockDataChannelObserver> observer_;
};
}  // namespace

TEST(DcSctpTransportTest, OpenSequence) {
  rtc::AutoThread main_thread;
  Peer peer_a;
  peer_a.fake_packet_transport_.SetWritable(true);

  EXPECT_CALL(*peer_a.socket_, Connect)
      .Times(1)
      .WillOnce(Invoke(peer_a.sctp_transport_.get(),
                       &dcsctp::DcSctpSocketCallbacks::OnConnected));
  EXPECT_CALL(peer_a.observer_, OnReadyToSend);
  EXPECT_CALL(peer_a.observer_, OnConnected);

  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);
}

// Tests that the close sequence invoked from one end results in the stream to
// be reset from both ends and all the proper signals are sent.
TEST(DcSctpTransportTest, CloseSequence) {
  rtc::AutoThread main_thread;
  Peer peer_a;
  Peer peer_b;
  peer_a.fake_packet_transport_.SetDestination(&peer_b.fake_packet_transport_,
                                               false);
  {
    InSequence sequence;

    EXPECT_CALL(*peer_a.socket_, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(Return(dcsctp::ResetStreamsStatus::kPerformed));

    EXPECT_CALL(*peer_b.socket_, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(Return(dcsctp::ResetStreamsStatus::kPerformed));

    EXPECT_CALL(peer_a.observer_, OnChannelClosing(1)).Times(0);
    EXPECT_CALL(peer_b.observer_, OnChannelClosing(1));
    EXPECT_CALL(peer_a.observer_, OnChannelClosed(1));
    EXPECT_CALL(peer_b.observer_, OnChannelClosed(1));
  }

  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);
  peer_b.sctp_transport_->Start(5000, 5000, 256 * 1024);
  peer_a.sctp_transport_->OpenStream(1);
  peer_b.sctp_transport_->OpenStream(1);
  peer_a.sctp_transport_->ResetStream(1);

  // Simulate the callbacks from the stream resets
  dcsctp::StreamID streams[1] = {dcsctp::StreamID(1)};
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_a.sctp_transport_.get())
      ->OnStreamsResetPerformed(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_b.sctp_transport_.get())
      ->OnIncomingStreamsReset(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_a.sctp_transport_.get())
      ->OnIncomingStreamsReset(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_b.sctp_transport_.get())
      ->OnStreamsResetPerformed(streams);
}

// Tests that the close sequence initiated from both peers at the same time
// terminates properly. Both peers will think they initiated it, so no
// OnClosingProcedureStartedRemotely should be called.
TEST(DcSctpTransportTest, CloseSequenceSimultaneous) {
  rtc::AutoThread main_thread;
  Peer peer_a;
  Peer peer_b;
  peer_a.fake_packet_transport_.SetDestination(&peer_b.fake_packet_transport_,
                                               false);
  {
    InSequence sequence;

    EXPECT_CALL(*peer_a.socket_, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(Return(dcsctp::ResetStreamsStatus::kPerformed));

    EXPECT_CALL(*peer_b.socket_, ResetStreams(ElementsAre(dcsctp::StreamID(1))))
        .WillOnce(Return(dcsctp::ResetStreamsStatus::kPerformed));

    EXPECT_CALL(peer_a.observer_, OnChannelClosing(1)).Times(0);
    EXPECT_CALL(peer_b.observer_, OnChannelClosing(1)).Times(0);
    EXPECT_CALL(peer_a.observer_, OnChannelClosed(1));
    EXPECT_CALL(peer_b.observer_, OnChannelClosed(1));
  }

  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);
  peer_b.sctp_transport_->Start(5000, 5000, 256 * 1024);
  peer_a.sctp_transport_->OpenStream(1);
  peer_b.sctp_transport_->OpenStream(1);
  peer_a.sctp_transport_->ResetStream(1);
  peer_b.sctp_transport_->ResetStream(1);

  // Simulate the callbacks from the stream resets
  dcsctp::StreamID streams[1] = {dcsctp::StreamID(1)};
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_a.sctp_transport_.get())
      ->OnStreamsResetPerformed(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_b.sctp_transport_.get())
      ->OnStreamsResetPerformed(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_a.sctp_transport_.get())
      ->OnIncomingStreamsReset(streams);
  static_cast<dcsctp::DcSctpSocketCallbacks*>(peer_b.sctp_transport_.get())
      ->OnIncomingStreamsReset(streams);
}

TEST(DcSctpTransportTest, DiscardMessageClosedChannel) {
  rtc::AutoThread main_thread;
  Peer peer_a;

  EXPECT_CALL(*peer_a.socket_, Send(_, _)).Times(0);

  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);

  cricket::SendDataResult result;
  SendDataParams params;
  rtc::CopyOnWriteBuffer payload;
  bool send_data_return =
      peer_a.sctp_transport_->SendData(1, params, payload, &result);
  EXPECT_FALSE(send_data_return);
  EXPECT_EQ(cricket::SDR_ERROR, result);
}

TEST(DcSctpTransportTest, DiscardMessageClosingChannel) {
  rtc::AutoThread main_thread;
  Peer peer_a;

  EXPECT_CALL(*peer_a.socket_, Send(_, _)).Times(0);

  peer_a.sctp_transport_->OpenStream(1);
  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);
  peer_a.sctp_transport_->ResetStream(1);

  cricket::SendDataResult result;
  SendDataParams params;
  rtc::CopyOnWriteBuffer payload;

  bool send_data_return =
      peer_a.sctp_transport_->SendData(1, params, payload, &result);
  EXPECT_FALSE(send_data_return);
  EXPECT_EQ(cricket::SDR_ERROR, result);
}

TEST(DcSctpTransportTest, SendDataOpenChannel) {
  rtc::AutoThread main_thread;
  Peer peer_a;
  dcsctp::DcSctpOptions options;

  EXPECT_CALL(*peer_a.socket_, Send(_, _)).Times(1);
  EXPECT_CALL(*peer_a.socket_, options()).WillOnce(ReturnPointee(&options));

  peer_a.sctp_transport_->OpenStream(1);
  peer_a.sctp_transport_->Start(5000, 5000, 256 * 1024);

  cricket::SendDataResult result;
  SendDataParams params;
  rtc::CopyOnWriteBuffer payload;

  bool send_data_return =
      peer_a.sctp_transport_->SendData(1, params, payload, &result);
  EXPECT_TRUE(send_data_return);
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
}

}  // namespace webrtc
