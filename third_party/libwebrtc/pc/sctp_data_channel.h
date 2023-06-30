/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SCTP_DATA_CHANNEL_H_
#define PC_SCTP_DATA_CHANNEL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "absl/types/optional.h"
#include "api/data_channel_interface.h"
#include "api/priority.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/transport/data_channel_transport_interface.h"
#include "pc/data_channel_utils.h"
#include "pc/sctp_utils.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"  // For SSLRole
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/weak_ptr.h"

namespace webrtc {

class SctpDataChannel;

class SctpDataChannelControllerInterface {
 public:
  // Sends the data to the transport.
  virtual RTCError SendData(StreamId sid,
                            const SendDataParams& params,
                            const rtc::CopyOnWriteBuffer& payload) = 0;
  // Adds the data channel SID to the transport for SCTP.
  virtual void AddSctpDataStream(StreamId sid) = 0;
  // Begins the closing procedure by sending an outgoing stream reset. Still
  // need to wait for callbacks to tell when this completes.
  virtual void RemoveSctpDataStream(StreamId sid) = 0;
  // Returns true if the transport channel is ready to send data.
  virtual bool ReadyToSendData() const = 0;
  // Notifies the controller of state changes.
  virtual void OnChannelStateChanged(SctpDataChannel* data_channel,
                                     DataChannelInterface::DataState state) = 0;

 protected:
  virtual ~SctpDataChannelControllerInterface() {}
};

struct InternalDataChannelInit : public DataChannelInit {
  enum OpenHandshakeRole { kOpener, kAcker, kNone };
  // The default role is kOpener because the default `negotiated` is false.
  InternalDataChannelInit() : open_handshake_role(kOpener) {}
  explicit InternalDataChannelInit(const DataChannelInit& base);

  // Does basic validation to determine if a data channel instance can be
  // constructed using the configuration.
  bool IsValid() const;

  OpenHandshakeRole open_handshake_role;
};

// Helper class to allocate unique IDs for SCTP DataChannels.
class SctpSidAllocator {
 public:
  SctpSidAllocator();
  // Gets the first unused odd/even id based on the DTLS role. If `role` is
  // SSL_CLIENT, the allocated id starts from 0 and takes even numbers;
  // otherwise, the id starts from 1 and takes odd numbers.
  // If a `StreamId` cannot be allocated, `StreamId::HasValue()` will be false.
  StreamId AllocateSid(rtc::SSLRole role);

  // Attempts to reserve a specific sid. Returns false if it's unavailable.
  bool ReserveSid(StreamId sid);

  // Indicates that `sid` isn't in use any more, and is thus available again.
  void ReleaseSid(StreamId sid);

 private:
  flat_set<StreamId> used_sids_ RTC_GUARDED_BY(&sequence_checker_);
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

// SctpDataChannel is an implementation of the DataChannelInterface based on
// SctpTransport. It provides an implementation of unreliable or
// reliable data channels.

// DataChannel states:
// kConnecting: The channel has been created the transport might not yet be
//              ready.
// kOpen: The open handshake has been performed (if relevant) and the data
//        channel is able to send messages.
// kClosing: DataChannelInterface::Close has been called, or the remote side
//           initiated the closing procedure, but the closing procedure has not
//           yet finished.
// kClosed: The closing handshake is finished (possibly initiated from this,
//          side, possibly from the peer).
//
// How the closing procedure works for SCTP:
// 1. Alice calls Close(), state changes to kClosing.
// 2. Alice finishes sending any queued data.
// 3. Alice calls RemoveSctpDataStream, sends outgoing stream reset.
// 4. Bob receives incoming stream reset; OnClosingProcedureStartedRemotely
//    called.
// 5. Bob sends outgoing stream reset.
// 6. Alice receives incoming reset, Bob receives acknowledgement. Both receive
//    OnClosingProcedureComplete callback and transition to kClosed.
class SctpDataChannel : public DataChannelInterface {
 public:
  static rtc::scoped_refptr<SctpDataChannel> Create(
      rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
      const std::string& label,
      bool connected_to_transport,
      const InternalDataChannelInit& config,
      rtc::Thread* signaling_thread,
      rtc::Thread* network_thread);

  // Instantiates an API proxy for a SctpDataChannel instance that will be
  // handed out to external callers.
  static rtc::scoped_refptr<DataChannelInterface> CreateProxy(
      rtc::scoped_refptr<SctpDataChannel> channel);

  void RegisterObserver(DataChannelObserver* observer) override;
  void UnregisterObserver() override;

  std::string label() const override;
  bool reliable() const override;
  bool ordered() const override;

  // Backwards compatible accessors
  uint16_t maxRetransmitTime() const override;
  uint16_t maxRetransmits() const override;

  absl::optional<int> maxPacketLifeTime() const override;
  absl::optional<int> maxRetransmitsOpt() const override;
  std::string protocol() const override;
  bool negotiated() const override;
  int id() const override;
  Priority priority() const override;

  uint64_t buffered_amount() const override;
  void Close() override;
  DataState state() const override;
  RTCError error() const override;
  uint32_t messages_sent() const override;
  uint64_t bytes_sent() const override;
  uint32_t messages_received() const override;
  uint64_t bytes_received() const override;
  bool Send(const DataBuffer& buffer) override;

  // Close immediately, ignoring any queued data or closing procedure.
  // This is called when the underlying SctpTransport is being destroyed.
  // It is also called by the PeerConnection if SCTP ID assignment fails.
  void CloseAbruptlyWithError(RTCError error);
  // Specializations of CloseAbruptlyWithError
  void CloseAbruptlyWithDataChannelFailure(const std::string& message);

  // Slots for controller to connect signals to.
  //
  // TODO(deadbeef): Make these private once we're hooking up signals ourselves,
  // instead of relying on SctpDataChannelControllerInterface.

  // Called when the SctpTransport's ready to use. That can happen when we've
  // finished negotiation, or if the channel was created after negotiation has
  // already finished.
  void OnTransportReady();

  void OnDataReceived(DataMessageType type,
                      const rtc::CopyOnWriteBuffer& payload);

  // Sets the SCTP sid and adds to transport layer if not set yet. Should only
  // be called once.
  void SetSctpSid(const StreamId& sid);

  // The remote side started the closing procedure by resetting its outgoing
  // stream (our incoming stream). Sets state to kClosing.
  void OnClosingProcedureStartedRemotely();
  // The closing procedure is complete; both incoming and outgoing stream
  // resets are done and the channel can transition to kClosed. Called
  // asynchronously after RemoveSctpDataStream.
  void OnClosingProcedureComplete();
  // Called when the transport channel is created.
  void OnTransportChannelCreated();
  // Called when the transport channel is unusable.
  // This method makes sure the DataChannel is disconnected and changes state
  // to kClosed.
  void OnTransportChannelClosed(RTCError error);

  DataChannelStats GetStats() const;

  const StreamId& sid() const { return id_; }

  // Reset the allocator for internal ID values for testing, so that
  // the internal IDs generated are predictable. Test only.
  static void ResetInternalIdAllocatorForTesting(int new_value);

 protected:
  SctpDataChannel(const InternalDataChannelInit& config,
                  rtc::WeakPtr<SctpDataChannelControllerInterface> controller,
                  const std::string& label,
                  bool connected_to_transport,
                  rtc::Thread* signaling_thread,
                  rtc::Thread* network_thread);
  ~SctpDataChannel() override;

 private:
  // The OPEN(_ACK) signaling state.
  enum HandshakeState {
    kHandshakeInit,
    kHandshakeShouldSendOpen,
    kHandshakeShouldSendAck,
    kHandshakeWaitingForAck,
    kHandshakeReady
  };

  void UpdateState();
  void SetState(DataState state);

  void DeliverQueuedReceivedData();

  void SendQueuedDataMessages();
  bool SendDataMessage(const DataBuffer& buffer, bool queue_if_blocked);
  bool QueueSendDataMessage(const DataBuffer& buffer);

  void SendQueuedControlMessages();
  void QueueControlMessage(const rtc::CopyOnWriteBuffer& buffer);
  bool SendControlMessage(const rtc::CopyOnWriteBuffer& buffer);

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const network_thread_;
  StreamId id_;
  const int internal_id_;
  const std::string label_;
  const std::string protocol_;
  const absl::optional<int> max_retransmit_time_;
  const absl::optional<int> max_retransmits_;
  const absl::optional<Priority> priority_;
  const bool negotiated_;
  const bool ordered_;

  DataChannelObserver* observer_ RTC_GUARDED_BY(signaling_thread_) = nullptr;
  DataState state_ RTC_GUARDED_BY(signaling_thread_) = kConnecting;
  RTCError error_ RTC_GUARDED_BY(signaling_thread_);
  uint32_t messages_sent_ RTC_GUARDED_BY(signaling_thread_) = 0;
  uint64_t bytes_sent_ RTC_GUARDED_BY(signaling_thread_) = 0;
  uint32_t messages_received_ RTC_GUARDED_BY(signaling_thread_) = 0;
  uint64_t bytes_received_ RTC_GUARDED_BY(signaling_thread_) = 0;
  rtc::WeakPtr<SctpDataChannelControllerInterface> controller_
      RTC_GUARDED_BY(signaling_thread_);
  HandshakeState handshake_state_ RTC_GUARDED_BY(signaling_thread_) =
      kHandshakeInit;
  bool connected_to_transport_ RTC_GUARDED_BY(signaling_thread_) = false;
  bool writable_ RTC_GUARDED_BY(signaling_thread_) = false;
  // Did we already start the graceful SCTP closing procedure?
  bool started_closing_procedure_ RTC_GUARDED_BY(signaling_thread_) = false;
  // Control messages that always have to get sent out before any queued
  // data.
  PacketQueue queued_control_data_ RTC_GUARDED_BY(signaling_thread_);
  PacketQueue queued_received_data_ RTC_GUARDED_BY(signaling_thread_);
  PacketQueue queued_send_data_ RTC_GUARDED_BY(signaling_thread_);
};

// Downcast a PeerConnectionInterface that points to a proxy object
// to its underlying SctpDataChannel object. For testing only.
SctpDataChannel* DowncastProxiedDataChannelInterfaceToSctpDataChannelForTesting(
    DataChannelInterface* channel);

}  // namespace webrtc

#endif  // PC_SCTP_DATA_CHANNEL_H_
