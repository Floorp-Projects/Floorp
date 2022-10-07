/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_TX_STREAM_SCHEDULER_H_
#define NET_DCSCTP_TX_STREAM_SCHEDULER_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "net/dcsctp/public/dcsctp_message.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/tx/send_queue.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/strong_alias.h"

namespace dcsctp {

// A parameterized stream scheduler. Currently, it implements the round robin
// scheduling algorithm using virtual finish time. It is to be used as a part of
// a send queue and will track all active streams (streams that have any data
// that can be sent).
//
// The stream scheduler works with the concept of associating active streams
// with a "virtual finish time", which is the time when a stream is allowed to
// produce data. Streams are ordered by their virtual finish time, and the
// "current virtual time" will advance to the next following virtual finish time
// whenever a chunk is to be produced. In the initial round-robin scheduling
// algorithm, a stream's virtual finish time will just increment by one (1)
// after having produced a chunk, which results in a round-robin scheduling.
class StreamScheduler {
 private:
  class VirtualTime : public webrtc::StrongAlias<class VirtualTimeTag, double> {
   public:
    constexpr explicit VirtualTime(const UnderlyingType& v)
        : webrtc::StrongAlias<class VirtualTimeTag, double>(v) {}

    static constexpr VirtualTime Zero() { return VirtualTime(0); }
  };

 public:
  class StreamProducer {
   public:
    virtual ~StreamProducer() = default;

    // Produces a fragment of data to send. The current wall time is specified
    // as `now` and should be used to skip chunks with expired limited lifetime.
    // The parameter `max_size` specifies the maximum amount of actual payload
    // that may be returned. If these constraints prevents the stream from
    // sending some data, `absl::nullopt` should be returned.
    virtual absl::optional<SendQueue::DataToSend> Produce(TimeMs now,
                                                          size_t max_size) = 0;

    // Returns the number of payload bytes that is scheduled to be sent in the
    // next enqueued message, or zero if there are no enqueued messages or if
    // the stream has been actively paused.
    virtual size_t bytes_to_send_in_next_message() const = 0;
  };

  class Stream {
   public:
    StreamID stream_id() const { return stream_id_; }

    StreamPriority priority() const { return priority_; }
    void set_priority(StreamPriority priority);

    // Will activate the stream _if_ it has any data to send. That is, if the
    // callback to `bytes_to_send_in_next_message` returns non-zero. If the
    // callback returns zero, the stream will not be made active.
    void MaybeMakeActive();

    // Will remove the stream from the list of active streams, and will not try
    // to produce data from it. To make it active again, call `MaybeMakeActive`.
    void MakeInactive();

    // Make the scheduler move to another message, or another stream. This is
    // used to abort the scheduler from continuing producing fragments for the
    // current message in case it's deleted.
    void ForceReschedule() { parent_.ForceReschedule(); }

   private:
    friend class StreamScheduler;

    Stream(StreamScheduler* parent,
           StreamProducer* producer,
           StreamID stream_id,
           StreamPriority priority)
        : parent_(*parent),
          producer_(*producer),
          stream_id_(stream_id),
          priority_(priority) {}

    // Produces a message from this stream. This will only be called on streams
    // that have data.
    absl::optional<SendQueue::DataToSend> Produce(TimeMs now, size_t max_size);

    void MakeActive();
    void ForceMarkInactive();

    VirtualTime current_time() const { return current_virtual_time_; }
    VirtualTime next_finish_time() const { return next_finish_time_; }
    size_t bytes_to_send_in_next_message() const {
      return producer_.bytes_to_send_in_next_message();
    }

    // Returns the next virtual finish time for this stream.
    VirtualTime GetNextFinishTime() const;

    StreamScheduler& parent_;
    StreamProducer& producer_;
    const StreamID stream_id_;
    StreamPriority priority_;
    // This outgoing stream's "current" virtual_time.
    VirtualTime current_virtual_time_ = VirtualTime::Zero();
    VirtualTime next_finish_time_ = VirtualTime::Zero();
  };

  std::unique_ptr<Stream> CreateStream(StreamProducer* producer,
                                       StreamID stream_id,
                                       StreamPriority priority) {
    return absl::WrapUnique(new Stream(this, producer, stream_id, priority));
  }

  // Makes the scheduler stop producing message from the current stream and
  // re-evaluates which stream to produce from.
  void ForceReschedule() { currently_sending_a_message_ = false; }

  // Produces a fragment of data to send. The current wall time is specified as
  // `now` and will be used to skip chunks with expired limited lifetime. The
  // parameter `max_size` specifies the maximum amount of actual payload that
  // may be returned. If no data can be produced, `absl::nullopt` is returned.
  absl::optional<SendQueue::DataToSend> Produce(TimeMs now, size_t max_size);

  std::set<StreamID> ActiveStreamsForTesting() const;

 private:
  struct ActiveStreamComparator {
    // Ordered by virtual finish time (primary), stream-id (secondary).
    bool operator()(Stream* a, Stream* b) const {
      VirtualTime a_vft = a->next_finish_time();
      VirtualTime b_vft = b->next_finish_time();
      if (a_vft == b_vft) {
        return a->stream_id() < b->stream_id();
      }
      return a_vft < b_vft;
    }
  };

  bool IsConsistent() const;

  // The current virtual time, as defined in the WFQ algorithm.
  VirtualTime virtual_time_ = VirtualTime::Zero();

  // The current stream to send chunks from.
  Stream* current_stream_ = nullptr;

  // Indicates if the streams is currently sending a message, and should then
  // continue sending from this stream until that message has been sent in full.
  bool currently_sending_a_message_ = false;

  // The currently active streams, ordered by virtual finish time.
  webrtc::flat_set<Stream*, ActiveStreamComparator> active_streams_;
};

}  // namespace dcsctp

#endif  // NET_DCSCTP_TX_STREAM_SCHEDULER_H_
