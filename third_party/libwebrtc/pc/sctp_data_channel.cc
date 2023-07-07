/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sctp_data_channel.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "media/sctp/sctp_transport_internal.h"
#include "pc/proxy.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/unused.h"
#include "rtc_base/thread.h"

namespace webrtc {

namespace {

static size_t kMaxQueuedReceivedDataBytes = 16 * 1024 * 1024;

static std::atomic<int> g_unique_id{0};

int GenerateUniqueId() {
  return ++g_unique_id;
}

// Define proxy for DataChannelInterface.
BEGIN_PRIMARY_PROXY_MAP(DataChannel)
PROXY_PRIMARY_THREAD_DESTRUCTOR()
PROXY_METHOD1(void, RegisterObserver, DataChannelObserver*)
PROXY_METHOD0(void, UnregisterObserver)
BYPASS_PROXY_CONSTMETHOD0(std::string, label)
BYPASS_PROXY_CONSTMETHOD0(bool, reliable)
BYPASS_PROXY_CONSTMETHOD0(bool, ordered)
BYPASS_PROXY_CONSTMETHOD0(uint16_t, maxRetransmitTime)
BYPASS_PROXY_CONSTMETHOD0(uint16_t, maxRetransmits)
BYPASS_PROXY_CONSTMETHOD0(absl::optional<int>, maxRetransmitsOpt)
BYPASS_PROXY_CONSTMETHOD0(absl::optional<int>, maxPacketLifeTime)
BYPASS_PROXY_CONSTMETHOD0(std::string, protocol)
BYPASS_PROXY_CONSTMETHOD0(bool, negotiated)
// Can't bypass the proxy since the id may change.
PROXY_CONSTMETHOD0(int, id)
BYPASS_PROXY_CONSTMETHOD0(Priority, priority)
PROXY_CONSTMETHOD0(DataState, state)
PROXY_CONSTMETHOD0(RTCError, error)
PROXY_CONSTMETHOD0(uint32_t, messages_sent)
PROXY_CONSTMETHOD0(uint64_t, bytes_sent)
PROXY_CONSTMETHOD0(uint32_t, messages_received)
PROXY_CONSTMETHOD0(uint64_t, bytes_received)
PROXY_CONSTMETHOD0(uint64_t, buffered_amount)
PROXY_METHOD0(void, Close)
// TODO(bugs.webrtc.org/11547): Change to run on the network thread.
PROXY_METHOD1(bool, Send, const DataBuffer&)
END_PROXY_MAP(DataChannel)

}  // namespace

InternalDataChannelInit::InternalDataChannelInit(const DataChannelInit& base)
    : DataChannelInit(base), open_handshake_role(kOpener) {
  // If the channel is externally negotiated, do not send the OPEN message.
  if (base.negotiated) {
    open_handshake_role = kNone;
  } else {
    // Datachannel is externally negotiated. Ignore the id value.
    // Specified in createDataChannel, WebRTC spec section 6.1 bullet 13.
    id = -1;
  }
  // Backwards compatibility: If maxRetransmits or maxRetransmitTime
  // are negative, the feature is not enabled.
  // Values are clamped to a 16bit range.
  if (maxRetransmits) {
    if (*maxRetransmits < 0) {
      RTC_LOG(LS_ERROR)
          << "Accepting maxRetransmits < 0 for backwards compatibility";
      maxRetransmits = absl::nullopt;
    } else if (*maxRetransmits > std::numeric_limits<uint16_t>::max()) {
      maxRetransmits = std::numeric_limits<uint16_t>::max();
    }
  }

  if (maxRetransmitTime) {
    if (*maxRetransmitTime < 0) {
      RTC_LOG(LS_ERROR)
          << "Accepting maxRetransmitTime < 0 for backwards compatibility";
      maxRetransmitTime = absl::nullopt;
    } else if (*maxRetransmitTime > std::numeric_limits<uint16_t>::max()) {
      maxRetransmitTime = std::numeric_limits<uint16_t>::max();
    }
  }
}

bool InternalDataChannelInit::IsValid() const {
  if (id < -1)
    return false;

  if (maxRetransmits.has_value() && maxRetransmits.value() < 0)
    return false;

  if (maxRetransmitTime.has_value() && maxRetransmitTime.value() < 0)
    return false;

  // Only one of these can be set.
  if (maxRetransmits.has_value() && maxRetransmitTime.has_value())
    return false;

  return true;
}

SctpSidAllocator::SctpSidAllocator() {
  sequence_checker_.Detach();
}

StreamId SctpSidAllocator::AllocateSid(rtc::SSLRole role) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  int potential_sid = (role == rtc::SSL_CLIENT) ? 0 : 1;
  while (potential_sid <= static_cast<int>(cricket::kMaxSctpSid)) {
    StreamId sid(potential_sid);
    if (used_sids_.insert(sid).second)
      return sid;
    potential_sid += 2;
  }
  RTC_LOG(LS_ERROR) << "SCTP sid allocation pool exhausted.";
  return StreamId();
}

bool SctpSidAllocator::ReserveSid(StreamId sid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  if (!sid.HasValue() || sid.stream_id_int() > cricket::kMaxSctpSid)
    return false;
  return used_sids_.insert(sid).second;
}

void SctpSidAllocator::ReleaseSid(StreamId sid) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);
  used_sids_.erase(sid);
}

// static
rtc::scoped_refptr<SctpDataChannel> SctpDataChannel::Create(
    rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
    const std::string& label,
    bool connected_to_transport,
    const InternalDataChannelInit& config,
    rtc::Thread* signaling_thread,
    rtc::Thread* network_thread) {
  RTC_DCHECK(controller);
  RTC_DCHECK(config.IsValid());
  return rtc::make_ref_counted<SctpDataChannel>(
      config, std::move(controller), label, connected_to_transport,
      signaling_thread, network_thread);
}

// static
rtc::scoped_refptr<DataChannelInterface> SctpDataChannel::CreateProxy(
    rtc::scoped_refptr<SctpDataChannel> channel) {
  // TODO(bugs.webrtc.org/11547): incorporate the network thread in the proxy.
  auto* signaling_thread = channel->signaling_thread_;
  return DataChannelProxy::Create(signaling_thread, std::move(channel));
}

SctpDataChannel::SctpDataChannel(
    const InternalDataChannelInit& config,
    rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
    const std::string& label,
    bool connected_to_transport,
    rtc::Thread* signaling_thread,
    rtc::Thread* network_thread)
    : signaling_thread_(signaling_thread),
      network_thread_(network_thread),
      id_(config.id),
      internal_id_(GenerateUniqueId()),
      label_(label),
      protocol_(config.protocol),
      max_retransmit_time_(config.maxRetransmitTime),
      max_retransmits_(config.maxRetransmits),
      priority_(config.priority),
      negotiated_(config.negotiated),
      ordered_(config.ordered),
      observer_(nullptr),
      controller_(std::move(controller)),
      connected_to_transport_(connected_to_transport) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_UNUSED(network_thread_);
  RTC_DCHECK(config.IsValid());
  RTC_DCHECK(controller_);

  switch (config.open_handshake_role) {
    case InternalDataChannelInit::kNone:  // pre-negotiated
      handshake_state_ = kHandshakeReady;
      break;
    case InternalDataChannelInit::kOpener:
      handshake_state_ = kHandshakeShouldSendOpen;
      break;
    case InternalDataChannelInit::kAcker:
      handshake_state_ = kHandshakeShouldSendAck;
      break;
  }

  // Try to connect to the transport in case the transport channel already
  // exists.
  if (id_.HasValue()) {
    controller_->AddSctpDataStream(id_);
  }
}

SctpDataChannel::~SctpDataChannel() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
}

void SctpDataChannel::RegisterObserver(DataChannelObserver* observer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  observer_ = observer;
  DeliverQueuedReceivedData();
}

void SctpDataChannel::UnregisterObserver() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  observer_ = nullptr;
}

std::string SctpDataChannel::label() const {
  return label_;
}

bool SctpDataChannel::reliable() const {
  // May be called on any thread.
  return !max_retransmits_ && !max_retransmit_time_;
}

bool SctpDataChannel::ordered() const {
  return ordered_;
}

uint16_t SctpDataChannel::maxRetransmitTime() const {
  return max_retransmit_time_ ? *max_retransmit_time_
                              : static_cast<uint16_t>(-1);
}

uint16_t SctpDataChannel::maxRetransmits() const {
  return max_retransmits_ ? *max_retransmits_ : static_cast<uint16_t>(-1);
}

absl::optional<int> SctpDataChannel::maxPacketLifeTime() const {
  return max_retransmit_time_;
}

absl::optional<int> SctpDataChannel::maxRetransmitsOpt() const {
  return max_retransmits_;
}

std::string SctpDataChannel::protocol() const {
  return protocol_;
}

bool SctpDataChannel::negotiated() const {
  return negotiated_;
}

int SctpDataChannel::id() const {
  return id_.stream_id_int();
}

Priority SctpDataChannel::priority() const {
  return priority_ ? *priority_ : Priority::kLow;
}

uint64_t SctpDataChannel::buffered_amount() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return queued_send_data_.byte_count();
}

void SctpDataChannel::Close() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (state_ == kClosing || state_ == kClosed)
    return;
  SetState(kClosing);
  // Will send queued data before beginning the underlying closing procedure.
  UpdateState();
}

SctpDataChannel::DataState SctpDataChannel::state() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return state_;
}

RTCError SctpDataChannel::error() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return error_;
}

uint32_t SctpDataChannel::messages_sent() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return messages_sent_;
}

uint64_t SctpDataChannel::bytes_sent() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return bytes_sent_;
}

uint32_t SctpDataChannel::messages_received() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return messages_received_;
}

uint64_t SctpDataChannel::bytes_received() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  return bytes_received_;
}

bool SctpDataChannel::Send(const DataBuffer& buffer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  // TODO(bugs.webrtc.org/11547): Expect this method to be called on the network
  // thread. Bring buffer management etc to the network thread and keep the
  // operational state management on the signaling thread.

  if (state_ != kOpen) {
    return false;
  }

  // If the queue is non-empty, we're waiting for SignalReadyToSend,
  // so just add to the end of the queue and keep waiting.
  if (!queued_send_data_.Empty()) {
    return QueueSendDataMessage(buffer);
  }

  SendDataMessage(buffer, true);

  // Always return true for SCTP DataChannel per the spec.
  return true;
}

void SctpDataChannel::SetSctpSid(const StreamId& sid) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(!id_.HasValue());
  RTC_DCHECK(sid.HasValue());
  RTC_DCHECK_NE(handshake_state_, kHandshakeWaitingForAck);
  RTC_DCHECK_EQ(state_, kConnecting);

  id_ = sid;
  controller_->AddSctpDataStream(sid);
}

void SctpDataChannel::OnClosingProcedureStartedRemotely() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (state_ != kClosing && state_ != kClosed) {
    // Don't bother sending queued data since the side that initiated the
    // closure wouldn't receive it anyway. See crbug.com/559394 for a lengthy
    // discussion about this.
    queued_send_data_.Clear();
    queued_control_data_.Clear();
    // Just need to change state to kClosing, SctpTransport will handle the
    // rest of the closing procedure and OnClosingProcedureComplete will be
    // called later.
    started_closing_procedure_ = true;
    SetState(kClosing);
  }
}

void SctpDataChannel::OnClosingProcedureComplete() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  // If the closing procedure is complete, we should have finished sending
  // all pending data and transitioned to kClosing already.
  RTC_DCHECK_EQ(state_, kClosing);
  RTC_DCHECK(queued_send_data_.Empty());
  SetState(kClosed);
}

void SctpDataChannel::OnTransportChannelCreated() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(controller_);

  connected_to_transport_ = true;

  // The sid may have been unassigned when controller_->ConnectDataChannel was
  // done. So always add the streams even if connected_to_transport_ is true.
  if (id_.HasValue()) {
    controller_->AddSctpDataStream(id_);
  }
}

void SctpDataChannel::OnTransportChannelClosed(RTCError error) {
  // The SctpTransport is unusable, which could come from multiple reasons:
  // - the SCTP m= section was rejected
  // - the DTLS transport is closed
  // - the SCTP transport is closed
  CloseAbruptlyWithError(std::move(error));
}

DataChannelStats SctpDataChannel::GetStats() const {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  DataChannelStats stats{internal_id_,        id(),         label(),
                         protocol(),          state(),      messages_sent(),
                         messages_received(), bytes_sent(), bytes_received()};
  return stats;
}

void SctpDataChannel::OnDataReceived(DataMessageType type,
                                     const rtc::CopyOnWriteBuffer& payload) {
  RTC_DCHECK_RUN_ON(signaling_thread_);

  if (type == DataMessageType::kControl) {
    if (handshake_state_ != kHandshakeWaitingForAck) {
      // Ignore it if we are not expecting an ACK message.
      RTC_LOG(LS_WARNING)
          << "DataChannel received unexpected CONTROL message, sid = "
          << id_.stream_id_int();
      return;
    }
    if (ParseDataChannelOpenAckMessage(payload)) {
      // We can send unordered as soon as we receive the ACK message.
      handshake_state_ = kHandshakeReady;
      RTC_LOG(LS_INFO) << "DataChannel received OPEN_ACK message, sid = "
                       << id_.stream_id_int();
    } else {
      RTC_LOG(LS_WARNING)
          << "DataChannel failed to parse OPEN_ACK message, sid = "
          << id_.stream_id_int();
    }
    return;
  }

  RTC_DCHECK(type == DataMessageType::kBinary ||
             type == DataMessageType::kText);

  RTC_DLOG(LS_VERBOSE) << "DataChannel received DATA message, sid = "
                       << id_.stream_id_int();
  // We can send unordered as soon as we receive any DATA message since the
  // remote side must have received the OPEN (and old clients do not send
  // OPEN_ACK).
  if (handshake_state_ == kHandshakeWaitingForAck) {
    handshake_state_ = kHandshakeReady;
  }

  bool binary = (type == DataMessageType::kBinary);
  auto buffer = std::make_unique<DataBuffer>(payload, binary);
  if (state_ == kOpen && observer_) {
    ++messages_received_;
    bytes_received_ += buffer->size();
    observer_->OnMessage(*buffer.get());
  } else {
    if (queued_received_data_.byte_count() + payload.size() >
        kMaxQueuedReceivedDataBytes) {
      RTC_LOG(LS_ERROR) << "Queued received data exceeds the max buffer size.";

      queued_received_data_.Clear();
      CloseAbruptlyWithError(
          RTCError(RTCErrorType::RESOURCE_EXHAUSTED,
                   "Queued received data exceeds the max buffer size."));

      return;
    }
    queued_received_data_.PushBack(std::move(buffer));
  }
}

void SctpDataChannel::OnTransportReady() {
  RTC_DCHECK_RUN_ON(signaling_thread_);

  // TODO(tommi, hta): We don't need the `writable_` flag for SCTP datachannels.
  // Remove it and just rely on `connected_to_transport_` instead.
  // In practice the transport is configured inside
  // `PeerConnection::SetupDataChannelTransport_n`, which results in
  // `SctpDataChannel` getting the OnTransportChannelCreated callback, and then
  // that's immediately followed by calling `transport->SetDataSink` which is
  // what triggers the callback to `OnTransportReady()`.
  // These steps are currently accomplished via two separate PostTask calls to
  // the signaling thread, but could simply be done in single method call on
  // the network thread (which incidentally is the thread that we'll need to
  // be on for the below `Send*` calls, which currently do a BlockingCall
  // from the signaling thread to the network thread.
  RTC_DCHECK(connected_to_transport_);
  writable_ = true;

  SendQueuedControlMessages();
  SendQueuedDataMessages();

  UpdateState();
}

void SctpDataChannel::CloseAbruptlyWithError(RTCError error) {
  RTC_DCHECK_RUN_ON(signaling_thread_);

  if (state_ == kClosed) {
    return;
  }

  connected_to_transport_ = false;

  // Closing abruptly means any queued data gets thrown away.
  queued_send_data_.Clear();
  queued_control_data_.Clear();

  // Still go to "kClosing" before "kClosed", since observers may be expecting
  // that.
  SetState(kClosing);
  error_ = std::move(error);
  SetState(kClosed);
}

void SctpDataChannel::CloseAbruptlyWithDataChannelFailure(
    const std::string& message) {
  RTCError error(RTCErrorType::OPERATION_ERROR_WITH_DATA, message);
  error.set_error_detail(RTCErrorDetailType::DATA_CHANNEL_FAILURE);
  CloseAbruptlyWithError(std::move(error));
}

void SctpDataChannel::UpdateState() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  // UpdateState determines what to do from a few state variables. Include
  // all conditions required for each state transition here for
  // clarity. OnTransportReady(true) will send any queued data and then invoke
  // UpdateState().

  switch (state_) {
    case kConnecting: {
      if (connected_to_transport_) {
        if (handshake_state_ == kHandshakeShouldSendOpen) {
          rtc::CopyOnWriteBuffer payload;
          WriteDataChannelOpenMessage(label_, protocol_, priority_, ordered_,
                                      max_retransmits_, max_retransmit_time_,
                                      &payload);
          SendControlMessage(payload);
        } else if (handshake_state_ == kHandshakeShouldSendAck) {
          rtc::CopyOnWriteBuffer payload;
          WriteDataChannelOpenAckMessage(&payload);
          SendControlMessage(payload);
        }
        if (writable_ && (handshake_state_ == kHandshakeReady ||
                          handshake_state_ == kHandshakeWaitingForAck)) {
          SetState(kOpen);
          // If we have received buffers before the channel got writable.
          // Deliver them now.
          DeliverQueuedReceivedData();
        }
      } else {
        RTC_DCHECK(!id_.HasValue());
      }
      break;
    }
    case kOpen: {
      break;
    }
    case kClosing: {
      if (connected_to_transport_) {
        // Wait for all queued data to be sent before beginning the closing
        // procedure.
        if (queued_send_data_.Empty() && queued_control_data_.Empty()) {
          // For SCTP data channels, we need to wait for the closing procedure
          // to complete; after calling RemoveSctpDataStream,
          // OnClosingProcedureComplete will end up called asynchronously
          // afterwards.
          if (!started_closing_procedure_ && controller_ && id_.HasValue()) {
            started_closing_procedure_ = true;
            controller_->RemoveSctpDataStream(id_);
          }
        }
      } else {
        // When we're not connected to a transport, we'll transition
        // directly to the `kClosed` state from here.
        queued_send_data_.Clear();
        queued_control_data_.Clear();
        SetState(kClosed);
      }
      break;
    }
    case kClosed:
      break;
  }
}

void SctpDataChannel::SetState(DataState state) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (state_ == state) {
    return;
  }

  state_ = state;
  if (observer_) {
    observer_->OnStateChange();
  }

  if (controller_)
    controller_->OnChannelStateChanged(this, state_);
}

void SctpDataChannel::DeliverQueuedReceivedData() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (!observer_) {
    return;
  }

  while (!queued_received_data_.Empty()) {
    std::unique_ptr<DataBuffer> buffer = queued_received_data_.PopFront();
    ++messages_received_;
    bytes_received_ += buffer->size();
    observer_->OnMessage(*buffer);
  }
}

void SctpDataChannel::SendQueuedDataMessages() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  if (queued_send_data_.Empty()) {
    return;
  }

  RTC_DCHECK(state_ == kOpen || state_ == kClosing);

  while (!queued_send_data_.Empty()) {
    std::unique_ptr<DataBuffer> buffer = queued_send_data_.PopFront();
    if (!SendDataMessage(*buffer, false)) {
      // Return the message to the front of the queue if sending is aborted.
      queued_send_data_.PushFront(std::move(buffer));
      break;
    }
  }
}

bool SctpDataChannel::SendDataMessage(const DataBuffer& buffer,
                                      bool queue_if_blocked) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  SendDataParams send_params;
  if (!controller_) {
    return false;
  }

  send_params.ordered = ordered_;
  // Send as ordered if it is still going through OPEN/ACK signaling.
  if (handshake_state_ != kHandshakeReady && !ordered_) {
    send_params.ordered = true;
    RTC_DLOG(LS_VERBOSE)
        << "Sending data as ordered for unordered DataChannel "
           "because the OPEN_ACK message has not been received.";
  }

  send_params.max_rtx_count = max_retransmits_;
  send_params.max_rtx_ms = max_retransmit_time_;
  send_params.type =
      buffer.binary ? DataMessageType::kBinary : DataMessageType::kText;

  RTCError error = controller_->SendData(id_, send_params, buffer.data);

  if (error.ok()) {
    ++messages_sent_;
    bytes_sent_ += buffer.size();

    if (observer_ && buffer.size() > 0) {
      observer_->OnBufferedAmountChange(buffer.size());
    }
    return true;
  }

  if (error.type() == RTCErrorType::RESOURCE_EXHAUSTED) {
    if (!queue_if_blocked || QueueSendDataMessage(buffer)) {
      return false;
    }
  }
  // Close the channel if the error is not SDR_BLOCK, or if queuing the
  // message failed.
  RTC_LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send data, "
                       "send_result = "
                    << ToString(error.type()) << ":" << error.message();
  CloseAbruptlyWithError(
      RTCError(RTCErrorType::NETWORK_ERROR, "Failure to send data"));

  return false;
}

bool SctpDataChannel::QueueSendDataMessage(const DataBuffer& buffer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  size_t start_buffered_amount = queued_send_data_.byte_count();
  if (start_buffered_amount + buffer.size() >
      DataChannelInterface::MaxSendQueueSize()) {
    RTC_LOG(LS_ERROR) << "Can't buffer any more data for the data channel.";
    return false;
  }
  queued_send_data_.PushBack(std::make_unique<DataBuffer>(buffer));
  return true;
}

void SctpDataChannel::SendQueuedControlMessages() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  PacketQueue control_packets;
  control_packets.Swap(&queued_control_data_);

  while (!control_packets.Empty()) {
    std::unique_ptr<DataBuffer> buf = control_packets.PopFront();
    SendControlMessage(buf->data);
  }
}

void SctpDataChannel::QueueControlMessage(
    const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  queued_control_data_.PushBack(std::make_unique<DataBuffer>(buffer, true));
}

bool SctpDataChannel::SendControlMessage(const rtc::CopyOnWriteBuffer& buffer) {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  RTC_DCHECK(writable_);
  RTC_DCHECK(connected_to_transport_);
  RTC_DCHECK(id_.HasValue());

  if (!controller_) {
    return false;
  }
  bool is_open_message = handshake_state_ == kHandshakeShouldSendOpen;
  RTC_DCHECK(!is_open_message || !negotiated_);

  SendDataParams send_params;
  // Send data as ordered before we receive any message from the remote peer to
  // make sure the remote peer will not receive any data before it receives the
  // OPEN message.
  send_params.ordered = ordered_ || is_open_message;
  send_params.type = DataMessageType::kControl;

  RTCError err = controller_->SendData(id_, send_params, buffer);
  if (err.ok()) {
    RTC_DLOG(LS_VERBOSE) << "Sent CONTROL message on channel "
                         << id_.stream_id_int();

    if (handshake_state_ == kHandshakeShouldSendAck) {
      handshake_state_ = kHandshakeReady;
    } else if (handshake_state_ == kHandshakeShouldSendOpen) {
      handshake_state_ = kHandshakeWaitingForAck;
    }
  } else if (err.type() == RTCErrorType::RESOURCE_EXHAUSTED) {
    QueueControlMessage(buffer);
  } else {
    RTC_LOG(LS_ERROR) << "Closing the DataChannel due to a failure to send"
                         " the CONTROL message, send_result = "
                      << ToString(err.type());
    err.set_message("Failed to send a CONTROL message");
    CloseAbruptlyWithError(err);
  }
  return err.ok();
}

// static
void SctpDataChannel::ResetInternalIdAllocatorForTesting(int new_value) {
  g_unique_id = new_value;
}

SctpDataChannel* DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
    DataChannelInterface* channel) {
  return static_cast<SctpDataChannel*>(
      static_cast<DataChannelProxy*>(channel)->internal());
}

}  // namespace webrtc
