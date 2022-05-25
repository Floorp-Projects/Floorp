/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_RR_SEND_QUEUE_H_
#define NET_DCSCTP_TX_RR_SEND_QUEUE_H_

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/common/pair_hash.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"

namespace dcsctp {

// The Round Robin SendQueue holds all messages that the client wants to send,
// but that haven't yet been split into chunks and fully sent on the wire.
//
// As defined in https://datatracker.ietf.org/doc/html/rfc8260#section-3.2,
// it will cycle to send messages from different streams. It will send all
// fragments from one message before continuing with a different message on
// possibly a different stream, until support for message interleaving has been
// implemented.
//
// As messages can be (requested to be) sent before the connection is properly
// established, this send queue is always present - even for closed connections.
class RRSendQueue : public SendQueue {
 public:
  // How small a data chunk's payload may be, if having to fragment a message.
  static constexpr size_t kMinimumFragmentedPayload = 10;

  RRSendQueue(absl::string_view log_prefix, size_t buffer_size)
      : log_prefix_(std::string(log_prefix) + "fcfs: "),
        buffer_size_(buffer_size) {}

  // Indicates if the buffer is full. Note that it's up to the caller to ensure
  // that the buffer is not full prior to adding new items to it.
  bool IsFull() const;
  // Indicates if the buffer is empty.
  bool IsEmpty() const;

  // Adds the message to be sent using the `send_options` provided. The current
  // time should be in `now`. Note that it's the responsibility of the caller to
  // ensure that the buffer is not full (by calling `IsFull`) before adding
  // messages to it.
  void Add(TimeMs now,
           DcSctpMessage message,
           const SendOptions& send_options = {});

  // Implementation of `SendQueue`.
  absl::optional<DataToSend> Produce(TimeMs now, size_t max_size) override;
  void Discard(IsUnordered unordered,
               StreamID stream_id,
               MID message_id) override;
  void PrepareResetStreams(rtc::ArrayView<const StreamID> streams) override;
  bool CanResetStreams() const override;
  void CommitResetStreams() override;
  void RollbackResetStreams() override;
  void Reset() override;

  // The size of the buffer, in "payload bytes".
  size_t total_bytes() const;

 private:
  // Per-stream information.
  class OutgoingStream {
   public:
    // Enqueues a message to this stream.
    void Add(DcSctpMessage message,
             absl::optional<TimeMs> expires_at,
             const SendOptions& send_options);

    // Possibly produces a data chunk to send.
    absl::optional<DataToSend> Produce(TimeMs now, size_t max_size);

    // The amount of data enqueued on this stream.
    size_t buffered_amount() const;

    // Discards a partially sent message, see `SendQueue::Discard`.
    void Discard(IsUnordered unordered, MID message_id);

    // Pauses this stream, which is used before resetting it.
    void Pause();

    // Resumes a paused stream.
    void Resume() { is_paused_ = false; }

    bool is_paused() const { return is_paused_; }

    // Resets this stream, meaning MIDs and SSNs are set to zero.
    void Reset();

    // Indicates if this stream has a partially sent message in it.
    bool has_partially_sent_message() const;

   private:
    // An enqueued message and metadata.
    struct Item {
      explicit Item(DcSctpMessage msg,
                    absl::optional<TimeMs> expires_at,
                    const SendOptions& send_options)
          : message(std::move(msg)),
            expires_at(expires_at),
            send_options(send_options),
            remaining_offset(0),
            remaining_size(message.payload().size()) {}
      DcSctpMessage message;
      absl::optional<TimeMs> expires_at;
      SendOptions send_options;
      // The remaining payload (offset and size) to be sent, when it has been
      // fragmented.
      size_t remaining_offset;
      size_t remaining_size;
      // If set, an allocated Message ID and SSN. Will be allocated when the
      // first fragment is sent.
      absl::optional<MID> message_id = absl::nullopt;
      absl::optional<SSN> ssn = absl::nullopt;
      // The current Fragment Sequence Number, incremented for each fragment.
      FSN current_fsn = FSN(0);
    };

    // Returns the first non-expired message, or nullptr if there isn't one.
    Item* GetFirstNonExpiredMessage(TimeMs now);

    // Streams are pause when they are about to be reset.
    bool is_paused_ = false;
    // MIDs are different for unordered and ordered messages sent on a stream.
    MID next_unordered_mid_ = MID(0);
    MID next_ordered_mid_ = MID(0);

    SSN next_ssn_ = SSN(0);
    // Enqueued messages, and metadata.
    std::deque<Item> items_;
  };

  OutgoingStream& GetOrCreateStreamInfo(StreamID stream_id);
  absl::optional<DataToSend> Produce(
      std::map<StreamID, OutgoingStream>::iterator it,
      TimeMs now,
      size_t max_size);

  const std::string log_prefix_;
  const size_t buffer_size_;

  // The next stream to send chunks from.
  StreamID next_stream_id_ = StreamID(0);

  // All streams, and messages added to those.
  std::map<StreamID, OutgoingStream> streams_;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_RR_SEND_QUEUE_H_
