/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/transport/data_channel_transport_interface.h"
#include "media/base/media_channel.h"
#include "media/sctp/sctp_transport_internal.h"
#include "pc/sctp_data_channel.h"
#include "pc/sctp_utils.h"
#include "pc/test/fake_data_channel_controller.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

static constexpr int kDefaultTimeout = 10000;

class FakeDataChannelObserver : public DataChannelObserver {
 public:
  FakeDataChannelObserver()
      : messages_received_(0),
        on_state_change_count_(0),
        on_buffered_amount_change_count_(0) {}

  void OnStateChange() { ++on_state_change_count_; }

  void OnBufferedAmountChange(uint64_t previous_amount) {
    ++on_buffered_amount_change_count_;
  }

  void OnMessage(const DataBuffer& buffer) { ++messages_received_; }

  size_t messages_received() const { return messages_received_; }

  void ResetOnStateChangeCount() { on_state_change_count_ = 0; }

  void ResetOnBufferedAmountChangeCount() {
    on_buffered_amount_change_count_ = 0;
  }

  size_t on_state_change_count() const { return on_state_change_count_; }

  size_t on_buffered_amount_change_count() const {
    return on_buffered_amount_change_count_;
  }

 private:
  size_t messages_received_;
  size_t on_state_change_count_;
  size_t on_buffered_amount_change_count_;
};

// TODO(bugs.webrtc.org/11547): Incorporate a dedicated network thread.
class SctpDataChannelTest : public ::testing::Test {
 protected:
  SctpDataChannelTest()
      : controller_(new FakeDataChannelController()),
        webrtc_data_channel_(controller_->CreateDataChannel("test", init_)) {}

  void SetChannelReady() {
    controller_->set_transport_available(true);
    webrtc_data_channel_->OnTransportChannelCreated();
    if (!webrtc_data_channel_->sid().HasValue()) {
      webrtc_data_channel_->SetSctpSid(StreamId(0));
    }
    controller_->set_ready_to_send(true);
  }

  void AddObserver() {
    observer_.reset(new FakeDataChannelObserver());
    webrtc_data_channel_->RegisterObserver(observer_.get());
  }

  rtc::AutoThread main_thread_;
  InternalDataChannelInit init_;
  std::unique_ptr<FakeDataChannelController> controller_;
  std::unique_ptr<FakeDataChannelObserver> observer_;
  rtc::scoped_refptr<SctpDataChannel> webrtc_data_channel_;
};

TEST_F(SctpDataChannelTest, VerifyConfigurationGetters) {
  EXPECT_EQ(webrtc_data_channel_->label(), "test");
  EXPECT_EQ(webrtc_data_channel_->protocol(), init_.protocol);

  // Note that the `init_.reliable` field is deprecated, so we directly set
  // it here to match spec behavior for purposes of checking the `reliable()`
  // getter.
  init_.reliable = (!init_.maxRetransmits && !init_.maxRetransmitTime);
  EXPECT_EQ(webrtc_data_channel_->reliable(), init_.reliable);
  EXPECT_EQ(webrtc_data_channel_->ordered(), init_.ordered);
  EXPECT_EQ(webrtc_data_channel_->negotiated(), init_.negotiated);
  EXPECT_EQ(webrtc_data_channel_->priority(), Priority::kLow);
  EXPECT_EQ(webrtc_data_channel_->maxRetransmitTime(),
            static_cast<uint16_t>(-1));
  EXPECT_EQ(webrtc_data_channel_->maxPacketLifeTime(), init_.maxRetransmitTime);
  EXPECT_EQ(webrtc_data_channel_->maxRetransmits(), static_cast<uint16_t>(-1));
  EXPECT_EQ(webrtc_data_channel_->maxRetransmitsOpt(), init_.maxRetransmits);

  // Check the non-const part of the configuration.
  EXPECT_EQ(webrtc_data_channel_->id(), init_.id);
  EXPECT_EQ(webrtc_data_channel_->sid(), StreamId());

  SetChannelReady();
  EXPECT_EQ(webrtc_data_channel_->id(), 0);
  EXPECT_EQ(webrtc_data_channel_->sid(), StreamId(0));
}

// Verifies that the data channel is connected to the transport after creation.
TEST_F(SctpDataChannelTest, ConnectedToTransportOnCreated) {
  controller_->set_transport_available(true);
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", init_);

  EXPECT_TRUE(controller_->IsConnected(dc.get()));
  // The sid is not set yet, so it should not have added the streams.
  EXPECT_FALSE(controller_->IsStreamAdded(dc->sid()));

  dc->SetSctpSid(StreamId(0));
  EXPECT_TRUE(controller_->IsStreamAdded(dc->sid()));
}

// Tests the state of the data channel.
TEST_F(SctpDataChannelTest, StateTransition) {
  AddObserver();

  EXPECT_EQ(DataChannelInterface::kConnecting, webrtc_data_channel_->state());
  EXPECT_EQ(observer_->on_state_change_count(), 0u);
  SetChannelReady();

  EXPECT_EQ(DataChannelInterface::kOpen, webrtc_data_channel_->state());
  EXPECT_EQ(observer_->on_state_change_count(), 1u);

  // `Close()` should trigger two state changes, first `kClosing`, then
  // `kClose`.
  webrtc_data_channel_->Close();
  EXPECT_EQ(DataChannelInterface::kClosed, webrtc_data_channel_->state());
  EXPECT_EQ(observer_->on_state_change_count(), 3u);
  EXPECT_TRUE(webrtc_data_channel_->error().ok());
  // Verifies that it's disconnected from the transport.
  EXPECT_FALSE(controller_->IsConnected(webrtc_data_channel_.get()));
}

// Tests that DataChannel::buffered_amount() is correct after the channel is
// blocked.
TEST_F(SctpDataChannelTest, BufferedAmountWhenBlocked) {
  AddObserver();
  SetChannelReady();
  DataBuffer buffer("abcd");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  size_t successful_send_count = 1;

  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());

  controller_->set_send_blocked(true);

  const int number_of_packets = 3;
  for (int i = 0; i < number_of_packets; ++i) {
    EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  }
  EXPECT_EQ(buffer.data.size() * number_of_packets,
            webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());

  controller_->set_send_blocked(false);
  successful_send_count += number_of_packets;
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(successful_send_count,
            observer_->on_buffered_amount_change_count());
}

// Tests that the queued data are sent when the channel transitions from blocked
// to unblocked.
TEST_F(SctpDataChannelTest, QueuedDataSentWhenUnblocked) {
  AddObserver();
  SetChannelReady();
  DataBuffer buffer("abcd");
  controller_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  controller_->set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(1U, observer_->on_buffered_amount_change_count());
}

// Tests that no crash when the channel is blocked right away while trying to
// send queued data.
TEST_F(SctpDataChannelTest, BlockedWhenSendQueuedDataNoCrash) {
  AddObserver();
  SetChannelReady();
  DataBuffer buffer("abcd");
  controller_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  // Set channel ready while it is still blocked.
  SetChannelReady();
  EXPECT_EQ(buffer.size(), webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(0U, observer_->on_buffered_amount_change_count());

  // Unblock the channel to send queued data again, there should be no crash.
  controller_->set_send_blocked(false);
  SetChannelReady();
  EXPECT_EQ(0U, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(1U, observer_->on_buffered_amount_change_count());
}

// Tests that DataChannel::messages_sent() and DataChannel::bytes_sent() are
// correct, sending data both while unblocked and while blocked.
TEST_F(SctpDataChannelTest, VerifyMessagesAndBytesSent) {
  AddObserver();
  SetChannelReady();
  std::vector<DataBuffer> buffers({
      DataBuffer("message 1"),
      DataBuffer("msg 2"),
      DataBuffer("message three"),
      DataBuffer("quadra message"),
      DataBuffer("fifthmsg"),
      DataBuffer("message of the beast"),
  });

  // Default values.
  EXPECT_EQ(0U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_sent());

  // Send three buffers while not blocked.
  controller_->set_send_blocked(false);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[0]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[1]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[2]));
  size_t bytes_sent = buffers[0].size() + buffers[1].size() + buffers[2].size();
  EXPECT_EQ_WAIT(0U, webrtc_data_channel_->buffered_amount(), kDefaultTimeout);
  EXPECT_EQ(3U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());

  // Send three buffers while blocked, queuing the buffers.
  controller_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[3]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[4]));
  EXPECT_TRUE(webrtc_data_channel_->Send(buffers[5]));
  size_t bytes_queued =
      buffers[3].size() + buffers[4].size() + buffers[5].size();
  EXPECT_EQ(bytes_queued, webrtc_data_channel_->buffered_amount());
  EXPECT_EQ(3U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());

  // Unblock and make sure everything was sent.
  controller_->set_send_blocked(false);
  EXPECT_EQ_WAIT(0U, webrtc_data_channel_->buffered_amount(), kDefaultTimeout);
  bytes_sent += bytes_queued;
  EXPECT_EQ(6U, webrtc_data_channel_->messages_sent());
  EXPECT_EQ(bytes_sent, webrtc_data_channel_->bytes_sent());
}

// Tests that the queued control message is sent when channel is ready.
TEST_F(SctpDataChannelTest, OpenMessageSent) {
  // Initially the id is unassigned.
  EXPECT_EQ(-1, webrtc_data_channel_->id());

  SetChannelReady();
  EXPECT_GE(webrtc_data_channel_->id(), 0);
  EXPECT_EQ(DataMessageType::kControl,
            controller_->last_send_data_params().type);
  EXPECT_EQ(controller_->last_sid(), webrtc_data_channel_->id());
}

TEST_F(SctpDataChannelTest, QueuedOpenMessageSent) {
  controller_->set_send_blocked(true);
  SetChannelReady();
  controller_->set_send_blocked(false);

  EXPECT_EQ(DataMessageType::kControl,
            controller_->last_send_data_params().type);
  EXPECT_EQ(controller_->last_sid(), webrtc_data_channel_->id());
}

// Tests that the DataChannel created after transport gets ready can enter OPEN
// state.
TEST_F(SctpDataChannelTest, LateCreatedChannelTransitionToOpen) {
  SetChannelReady();
  InternalDataChannelInit init;
  init.id = 1;
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", init);
  EXPECT_EQ(DataChannelInterface::kConnecting, dc->state());
  EXPECT_TRUE_WAIT(DataChannelInterface::kOpen == dc->state(), 1000);
}

// Tests that an unordered DataChannel sends data as ordered until the OPEN_ACK
// message is received.
TEST_F(SctpDataChannelTest, SendUnorderedAfterReceivesOpenAck) {
  SetChannelReady();
  InternalDataChannelInit init;
  init.id = 1;
  init.ordered = false;
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", init);

  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, dc->state(), 1000);

  // Sends a message and verifies it's ordered.
  DataBuffer buffer("some data");
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_TRUE(controller_->last_send_data_params().ordered);

  // Emulates receiving an OPEN_ACK message.
  rtc::CopyOnWriteBuffer payload;
  WriteDataChannelOpenAckMessage(&payload);
  dc->OnDataReceived(DataMessageType::kControl, payload);

  // Sends another message and verifies it's unordered.
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_FALSE(controller_->last_send_data_params().ordered);
}

// Tests that an unordered DataChannel sends unordered data after any DATA
// message is received.
TEST_F(SctpDataChannelTest, SendUnorderedAfterReceiveData) {
  SetChannelReady();
  InternalDataChannelInit init;
  init.id = 1;
  init.ordered = false;
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", init);

  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, dc->state(), 1000);

  // Emulates receiving a DATA message.
  DataBuffer buffer("data");
  dc->OnDataReceived(DataMessageType::kText, buffer.data);

  // Sends a message and verifies it's unordered.
  ASSERT_TRUE(dc->Send(buffer));
  EXPECT_FALSE(controller_->last_send_data_params().ordered);
}

// Tests that the channel can't open until it's successfully sent the OPEN
// message.
TEST_F(SctpDataChannelTest, OpenWaitsForOpenMesssage) {
  DataBuffer buffer("foo");

  controller_->set_send_blocked(true);
  SetChannelReady();
  EXPECT_EQ(DataChannelInterface::kConnecting, webrtc_data_channel_->state());
  controller_->set_send_blocked(false);
  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, webrtc_data_channel_->state(),
                 1000);
  EXPECT_EQ(DataMessageType::kControl,
            controller_->last_send_data_params().type);
}

// Tests that close first makes sure all queued data gets sent.
TEST_F(SctpDataChannelTest, QueuedCloseFlushes) {
  DataBuffer buffer("foo");

  controller_->set_send_blocked(true);
  SetChannelReady();
  EXPECT_EQ(DataChannelInterface::kConnecting, webrtc_data_channel_->state());
  controller_->set_send_blocked(false);
  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, webrtc_data_channel_->state(),
                 1000);
  controller_->set_send_blocked(true);
  webrtc_data_channel_->Send(buffer);
  webrtc_data_channel_->Close();
  controller_->set_send_blocked(false);
  EXPECT_EQ_WAIT(DataChannelInterface::kClosed, webrtc_data_channel_->state(),
                 1000);
  EXPECT_TRUE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(DataMessageType::kText, controller_->last_send_data_params().type);
}

// Tests that messages are sent with the right id.
TEST_F(SctpDataChannelTest, SendDataId) {
  webrtc_data_channel_->SetSctpSid(StreamId(1));
  SetChannelReady();
  DataBuffer buffer("data");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(1, controller_->last_sid());
}

// Tests that the incoming messages with right ids are accepted.
TEST_F(SctpDataChannelTest, ReceiveDataWithValidId) {
  webrtc_data_channel_->SetSctpSid(StreamId(1));
  SetChannelReady();

  AddObserver();

  DataBuffer buffer("abcd");
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffer.data);
  EXPECT_EQ(1U, observer_->messages_received());
}

// Tests that no CONTROL message is sent if the datachannel is negotiated and
// not created from an OPEN message.
TEST_F(SctpDataChannelTest, NoMsgSentIfNegotiatedAndNotFromOpenMsg) {
  InternalDataChannelInit config;
  config.id = 1;
  config.negotiated = true;
  config.open_handshake_role = InternalDataChannelInit::kNone;

  SetChannelReady();
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", config);

  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, dc->state(), 1000);
  EXPECT_EQ(0, controller_->last_sid());
}

// Tests that DataChannel::messages_received() and DataChannel::bytes_received()
// are correct, receiving data both while not open and while open.
TEST_F(SctpDataChannelTest, VerifyMessagesAndBytesReceived) {
  AddObserver();
  std::vector<DataBuffer> buffers({
      DataBuffer("message 1"),
      DataBuffer("msg 2"),
      DataBuffer("message three"),
      DataBuffer("quadra message"),
      DataBuffer("fifthmsg"),
      DataBuffer("message of the beast"),
  });

  webrtc_data_channel_->SetSctpSid(StreamId(1));

  // Default values.
  EXPECT_EQ(0U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_received());

  // Receive three buffers while data channel isn't open.
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[0].data);
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[1].data);
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[2].data);
  EXPECT_EQ(0U, observer_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(0U, webrtc_data_channel_->bytes_received());

  // Open channel and make sure everything was received.
  SetChannelReady();
  size_t bytes_received =
      buffers[0].size() + buffers[1].size() + buffers[2].size();
  EXPECT_EQ(3U, observer_->messages_received());
  EXPECT_EQ(3U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(bytes_received, webrtc_data_channel_->bytes_received());

  // Receive three buffers while open.
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[3].data);
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[4].data);
  webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffers[5].data);
  bytes_received += buffers[3].size() + buffers[4].size() + buffers[5].size();
  EXPECT_EQ(6U, observer_->messages_received());
  EXPECT_EQ(6U, webrtc_data_channel_->messages_received());
  EXPECT_EQ(bytes_received, webrtc_data_channel_->bytes_received());
}

// Tests that OPEN_ACK message is sent if the datachannel is created from an
// OPEN message.
TEST_F(SctpDataChannelTest, OpenAckSentIfCreatedFromOpenMessage) {
  InternalDataChannelInit config;
  config.id = 1;
  config.negotiated = true;
  config.open_handshake_role = InternalDataChannelInit::kAcker;

  SetChannelReady();
  rtc::scoped_refptr<SctpDataChannel> dc =
      controller_->CreateDataChannel("test1", config);

  EXPECT_EQ_WAIT(DataChannelInterface::kOpen, dc->state(), 1000);

  EXPECT_EQ(config.id, controller_->last_sid());
  EXPECT_EQ(DataMessageType::kControl,
            controller_->last_send_data_params().type);
}

// Tests the OPEN_ACK role assigned by InternalDataChannelInit.
TEST_F(SctpDataChannelTest, OpenAckRoleInitialization) {
  InternalDataChannelInit init;
  EXPECT_EQ(InternalDataChannelInit::kOpener, init.open_handshake_role);
  EXPECT_FALSE(init.negotiated);

  DataChannelInit base;
  base.negotiated = true;
  InternalDataChannelInit init2(base);
  EXPECT_EQ(InternalDataChannelInit::kNone, init2.open_handshake_role);
}

// Tests that that Send() returns false if the sending buffer is full
// and the channel stays open.
TEST_F(SctpDataChannelTest, OpenWhenSendBufferFull) {
  SetChannelReady();

  const size_t packetSize = 1024;

  rtc::CopyOnWriteBuffer buffer(packetSize);
  memset(buffer.MutableData(), 0, buffer.size());

  DataBuffer packet(buffer, true);
  controller_->set_send_blocked(true);

  for (size_t i = 0; i < DataChannelInterface::MaxSendQueueSize() / packetSize;
       ++i) {
    EXPECT_TRUE(webrtc_data_channel_->Send(packet));
  }

  // The sending buffer shoul be full, send returns false.
  EXPECT_FALSE(webrtc_data_channel_->Send(packet));

  EXPECT_TRUE(DataChannelInterface::kOpen == webrtc_data_channel_->state());
}

// Tests that the DataChannel is closed on transport errors.
TEST_F(SctpDataChannelTest, ClosedOnTransportError) {
  SetChannelReady();
  DataBuffer buffer("abcd");
  controller_->set_transport_error();

  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));

  EXPECT_EQ(DataChannelInterface::kClosed, webrtc_data_channel_->state());
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(RTCErrorType::NETWORK_ERROR, webrtc_data_channel_->error().type());
  EXPECT_EQ(RTCErrorDetailType::NONE,
            webrtc_data_channel_->error().error_detail());
}

// Tests that the DataChannel is closed if the received buffer is full.
TEST_F(SctpDataChannelTest, ClosedWhenReceivedBufferFull) {
  SetChannelReady();
  rtc::CopyOnWriteBuffer buffer(1024);
  memset(buffer.MutableData(), 0, buffer.size());

  // Receiving data without having an observer will overflow the buffer.
  for (size_t i = 0; i < 16 * 1024 + 1; ++i) {
    webrtc_data_channel_->OnDataReceived(DataMessageType::kText, buffer);
  }
  EXPECT_EQ(DataChannelInterface::kClosed, webrtc_data_channel_->state());
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(RTCErrorType::RESOURCE_EXHAUSTED,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(RTCErrorDetailType::NONE,
            webrtc_data_channel_->error().error_detail());
}

// Tests that sending empty data returns no error and keeps the channel open.
TEST_F(SctpDataChannelTest, SendEmptyData) {
  webrtc_data_channel_->SetSctpSid(StreamId(1));
  SetChannelReady();
  EXPECT_EQ(DataChannelInterface::kOpen, webrtc_data_channel_->state());

  DataBuffer buffer("");
  EXPECT_TRUE(webrtc_data_channel_->Send(buffer));
  EXPECT_EQ(DataChannelInterface::kOpen, webrtc_data_channel_->state());
}

// Tests that a channel can be closed without being opened or assigned an sid.
TEST_F(SctpDataChannelTest, NeverOpened) {
  controller_->set_transport_available(true);
  webrtc_data_channel_->OnTransportChannelCreated();
  webrtc_data_channel_->Close();
}

// Tests that a data channel that's not connected to a transport can transition
// directly to the `kClosed` state when closed.
// See also chromium:1421534.
TEST_F(SctpDataChannelTest, UnusedTransitionsDirectlyToClosed) {
  webrtc_data_channel_->Close();
  EXPECT_EQ(DataChannelInterface::kClosed, webrtc_data_channel_->state());
}

// Test that the data channel goes to the "closed" state (and doesn't crash)
// when its transport goes away, even while data is buffered.
TEST_F(SctpDataChannelTest, TransportDestroyedWhileDataBuffered) {
  SetChannelReady();

  rtc::CopyOnWriteBuffer buffer(1024);
  memset(buffer.MutableData(), 0, buffer.size());
  DataBuffer packet(buffer, true);

  // Send a packet while sending is blocked so it ends up buffered.
  controller_->set_send_blocked(true);
  EXPECT_TRUE(webrtc_data_channel_->Send(packet));

  // Tell the data channel that its transport is being destroyed.
  // It should then stop using the transport (allowing us to delete it) and
  // transition to the "closed" state.
  RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA, "");
  error.set_error_detail(RTCErrorDetailType::SCTP_FAILURE);
  webrtc_data_channel_->OnTransportChannelClosed(error);
  controller_.reset(nullptr);
  EXPECT_EQ_WAIT(DataChannelInterface::kClosed, webrtc_data_channel_->state(),
                 kDefaultTimeout);
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(RTCErrorType::OPERATION_ERROR_WITH_DATA,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(RTCErrorDetailType::SCTP_FAILURE,
            webrtc_data_channel_->error().error_detail());
}

TEST_F(SctpDataChannelTest, TransportGotErrorCode) {
  SetChannelReady();

  // Tell the data channel that its transport is being destroyed with an
  // error code.
  // It should then report that error code.
  RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA,
                 "Transport channel closed");
  error.set_error_detail(RTCErrorDetailType::SCTP_FAILURE);
  error.set_sctp_cause_code(
      static_cast<uint16_t>(cricket::SctpErrorCauseCode::kProtocolViolation));
  webrtc_data_channel_->OnTransportChannelClosed(error);
  controller_.reset(nullptr);
  EXPECT_EQ_WAIT(DataChannelInterface::kClosed, webrtc_data_channel_->state(),
                 kDefaultTimeout);
  EXPECT_FALSE(webrtc_data_channel_->error().ok());
  EXPECT_EQ(RTCErrorType::OPERATION_ERROR_WITH_DATA,
            webrtc_data_channel_->error().type());
  EXPECT_EQ(RTCErrorDetailType::SCTP_FAILURE,
            webrtc_data_channel_->error().error_detail());
  EXPECT_EQ(
      static_cast<uint16_t>(cricket::SctpErrorCauseCode::kProtocolViolation),
      webrtc_data_channel_->error().sctp_cause_code());
}

class SctpSidAllocatorTest : public ::testing::Test {
 protected:
  SctpSidAllocator allocator_;
};

// Verifies that an even SCTP id is allocated for SSL_CLIENT and an odd id for
// SSL_SERVER.
TEST_F(SctpSidAllocatorTest, SctpIdAllocationBasedOnRole) {
  EXPECT_EQ(allocator_.AllocateSid(rtc::SSL_SERVER), StreamId(1));
  EXPECT_EQ(allocator_.AllocateSid(rtc::SSL_CLIENT), StreamId(0));
  EXPECT_EQ(allocator_.AllocateSid(rtc::SSL_SERVER), StreamId(3));
  EXPECT_EQ(allocator_.AllocateSid(rtc::SSL_CLIENT), StreamId(2));
}

// Verifies that SCTP ids of existing DataChannels are not reused.
TEST_F(SctpSidAllocatorTest, SctpIdAllocationNoReuse) {
  StreamId old_id(1);
  EXPECT_TRUE(allocator_.ReserveSid(old_id));

  StreamId new_id = allocator_.AllocateSid(rtc::SSL_SERVER);
  EXPECT_TRUE(new_id.HasValue());
  EXPECT_NE(old_id, new_id);

  old_id = StreamId(0);
  EXPECT_TRUE(allocator_.ReserveSid(old_id));
  new_id = allocator_.AllocateSid(rtc::SSL_CLIENT);
  EXPECT_TRUE(new_id.HasValue());
  EXPECT_NE(old_id, new_id);
}

// Verifies that SCTP ids of removed DataChannels can be reused.
TEST_F(SctpSidAllocatorTest, SctpIdReusedForRemovedDataChannel) {
  StreamId odd_id(1);
  StreamId even_id(0);
  EXPECT_TRUE(allocator_.ReserveSid(odd_id));
  EXPECT_TRUE(allocator_.ReserveSid(even_id));

  StreamId allocated_id = allocator_.AllocateSid(rtc::SSL_SERVER);
  EXPECT_EQ(odd_id.stream_id_int() + 2, allocated_id.stream_id_int());

  allocated_id = allocator_.AllocateSid(rtc::SSL_CLIENT);
  EXPECT_EQ(even_id.stream_id_int() + 2, allocated_id.stream_id_int());

  allocated_id = allocator_.AllocateSid(rtc::SSL_SERVER);
  EXPECT_EQ(odd_id.stream_id_int() + 4, allocated_id.stream_id_int());

  allocated_id = allocator_.AllocateSid(rtc::SSL_CLIENT);
  EXPECT_EQ(even_id.stream_id_int() + 4, allocated_id.stream_id_int());

  allocator_.ReleaseSid(odd_id);
  allocator_.ReleaseSid(even_id);

  // Verifies that removed ids are reused.
  allocated_id = allocator_.AllocateSid(rtc::SSL_SERVER);
  EXPECT_EQ(odd_id, allocated_id);

  allocated_id = allocator_.AllocateSid(rtc::SSL_CLIENT);
  EXPECT_EQ(even_id, allocated_id);

  // Verifies that used higher ids are not reused.
  allocated_id = allocator_.AllocateSid(rtc::SSL_SERVER);
  EXPECT_EQ(odd_id.stream_id_int() + 6, allocated_id.stream_id_int());

  allocated_id = allocator_.AllocateSid(rtc::SSL_CLIENT);
  EXPECT_EQ(even_id.stream_id_int() + 6, allocated_id.stream_id_int());
}

}  // namespace
}  // namespace webrtc
