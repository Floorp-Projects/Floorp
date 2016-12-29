/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/paced_sender.h"

#include <map>
#include <queue>
#include <set>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/pacing/bitrate_prober.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace {
// Time limit in milliseconds between packet bursts.
const int64_t kMinPacketLimitMs = 5;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int64_t kMaxIntervalTimeMs = 30;

}  // namespace

// TODO(sprang): Move at least PacketQueue and MediaBudget out to separate
// files, so that we can more easily test them.

namespace webrtc {
namespace paced_sender {
struct Packet {
  Packet(RtpPacketSender::Priority priority,
         uint32_t ssrc,
         uint16_t seq_number,
         int64_t capture_time_ms,
         int64_t enqueue_time_ms,
         size_t length_in_bytes,
         bool retransmission,
         uint64_t enqueue_order)
      : priority(priority),
        ssrc(ssrc),
        sequence_number(seq_number),
        capture_time_ms(capture_time_ms),
        enqueue_time_ms(enqueue_time_ms),
        bytes(length_in_bytes),
        retransmission(retransmission),
        enqueue_order(enqueue_order) {}

  RtpPacketSender::Priority priority;
  uint32_t ssrc;
  uint16_t sequence_number;
  int64_t capture_time_ms;
  int64_t enqueue_time_ms;
  size_t bytes;
  bool retransmission;
  uint64_t enqueue_order;
  std::list<Packet>::iterator this_it;
};

// Used by priority queue to sort packets.
struct Comparator {
  bool operator()(const Packet* first, const Packet* second) {
    // Highest prio = 0.
    if (first->priority != second->priority)
      return first->priority > second->priority;

    // Retransmissions go first.
    if (second->retransmission && !first->retransmission)
      return true;

    // Older frames have higher prio.
    if (first->capture_time_ms != second->capture_time_ms)
      return first->capture_time_ms > second->capture_time_ms;

    return first->enqueue_order > second->enqueue_order;
  }
};

// Class encapsulating a priority queue with some extensions.
class PacketQueue {
 public:
  explicit PacketQueue(Clock* clock)
      : bytes_(0),
        clock_(clock),
        queue_time_sum_(0),
        time_last_updated_(clock_->TimeInMilliseconds()) {}
  virtual ~PacketQueue() {}

  void Push(const Packet& packet) {
    if (!AddToDupeSet(packet))
      return;

    UpdateQueueTime(packet.enqueue_time_ms);

    // Store packet in list, use pointers in priority queue for cheaper moves.
    // Packets have a handle to its own iterator in the list, for easy removal
    // when popping from queue.
    packet_list_.push_front(packet);
    std::list<Packet>::iterator it = packet_list_.begin();
    it->this_it = it;          // Handle for direct removal from list.
    prio_queue_.push(&(*it));  // Pointer into list.
    bytes_ += packet.bytes;
  }

  const Packet& BeginPop() {
    const Packet& packet = *prio_queue_.top();
    prio_queue_.pop();
    return packet;
  }

  void CancelPop(const Packet& packet) { prio_queue_.push(&(*packet.this_it)); }

  void FinalizePop(const Packet& packet) {
    RemoveFromDupeSet(packet);
    bytes_ -= packet.bytes;
    queue_time_sum_ -= (time_last_updated_ - packet.enqueue_time_ms);
    packet_list_.erase(packet.this_it);
    RTC_DCHECK_EQ(packet_list_.size(), prio_queue_.size());
    if (packet_list_.empty())
      RTC_DCHECK_EQ(0u, queue_time_sum_);
  }

  bool Empty() const { return prio_queue_.empty(); }

  size_t SizeInPackets() const { return prio_queue_.size(); }

  uint64_t SizeInBytes() const { return bytes_; }

  int64_t OldestEnqueueTimeMs() const {
    auto it = packet_list_.rbegin();
    if (it == packet_list_.rend())
      return 0;
    return it->enqueue_time_ms;
  }

  void UpdateQueueTime(int64_t timestamp_ms) {
    RTC_DCHECK_GE(timestamp_ms, time_last_updated_);
    int64_t delta = timestamp_ms - time_last_updated_;
    // Use packet packet_list_.size() not prio_queue_.size() here, as there
    // might be an outstanding element popped from prio_queue_ currently in the
    // SendPacket() call, while packet_list_ will always be correct.
    queue_time_sum_ += delta * packet_list_.size();
    time_last_updated_ = timestamp_ms;
  }

  int64_t AverageQueueTimeMs() const {
    if (prio_queue_.empty())
      return 0;
    return queue_time_sum_ / packet_list_.size();
  }

 private:
  // Try to add a packet to the set of ssrc/seqno identifiers currently in the
  // queue. Return true if inserted, false if this is a duplicate.
  bool AddToDupeSet(const Packet& packet) {
    SsrcSeqNoMap::iterator it = dupe_map_.find(packet.ssrc);
    if (it == dupe_map_.end()) {
      // First for this ssrc, just insert.
      dupe_map_[packet.ssrc].insert(packet.sequence_number);
      return true;
    }

    // Insert returns a pair, where second is a bool set to true if new element.
    return it->second.insert(packet.sequence_number).second;
  }

  void RemoveFromDupeSet(const Packet& packet) {
    SsrcSeqNoMap::iterator it = dupe_map_.find(packet.ssrc);
    RTC_DCHECK(it != dupe_map_.end());
    it->second.erase(packet.sequence_number);
    if (it->second.empty()) {
      dupe_map_.erase(it);
    }
  }

  // List of packets, in the order the were enqueued. Since dequeueing may
  // occur out of order, use list instead of vector.
  std::list<Packet> packet_list_;
  // Priority queue of the packets, sorted according to Comparator.
  // Use pointers into list, to avoid moving whole struct within heap.
  std::priority_queue<Packet*, std::vector<Packet*>, Comparator> prio_queue_;
  // Total number of bytes in the queue.
  uint64_t bytes_;
  // Map<ssrc, set<seq_no> >, for checking duplicates.
  typedef std::map<uint32_t, std::set<uint16_t> > SsrcSeqNoMap;
  SsrcSeqNoMap dupe_map_;
  Clock* const clock_;
  int64_t queue_time_sum_;
  int64_t time_last_updated_;
};

class IntervalBudget {
 public:
  explicit IntervalBudget(int initial_target_rate_kbps)
      : target_rate_kbps_(initial_target_rate_kbps),
        bytes_remaining_(0) {}

  void set_target_rate_kbps(int target_rate_kbps) {
    target_rate_kbps_ = target_rate_kbps;
    bytes_remaining_ =
        std::max(-kWindowMs * target_rate_kbps_ / 8, bytes_remaining_);
  }

  void IncreaseBudget(int64_t delta_time_ms) {
    int64_t bytes = target_rate_kbps_ * delta_time_ms / 8;
    if (bytes_remaining_ < 0) {
      // We overused last interval, compensate this interval.
      bytes_remaining_ = bytes_remaining_ + bytes;
    } else {
      // If we underused last interval we can't use it this interval.
      bytes_remaining_ = bytes;
    }
  }

  void UseBudget(size_t bytes) {
    bytes_remaining_ = std::max(bytes_remaining_ - static_cast<int>(bytes),
                                -kWindowMs * target_rate_kbps_ / 8);
  }

  size_t bytes_remaining() const {
    return static_cast<size_t>(std::max(0, bytes_remaining_));
  }

  int target_rate_kbps() const { return target_rate_kbps_; }

 private:
  static const int kWindowMs = 500;

  int target_rate_kbps_;
  int bytes_remaining_;
};
}  // namespace paced_sender

const int64_t PacedSender::kMaxQueueLengthMs = 2000;
const float PacedSender::kDefaultPaceMultiplier = 2.5f;

PacedSender::PacedSender(Clock* clock,
                         Callback* callback,
                         int bitrate_kbps,
                         int max_bitrate_kbps,
                         int min_bitrate_kbps)
    : clock_(clock),
      callback_(callback),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      paused_(false),
      probing_enabled_(true),
      media_budget_(new paced_sender::IntervalBudget(max_bitrate_kbps)),
      padding_budget_(new paced_sender::IntervalBudget(min_bitrate_kbps)),
      prober_(new BitrateProber()),
      bitrate_bps_(1000 * bitrate_kbps),
      max_bitrate_kbps_(max_bitrate_kbps),
      time_last_update_us_(clock->TimeInMicroseconds()),
      packets_(new paced_sender::PacketQueue(clock)),
      packet_counter_(0) {
  UpdateBytesPerInterval(kMinPacketLimitMs);
}

PacedSender::~PacedSender() {}

void PacedSender::Pause() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = true;
}

void PacedSender::Resume() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = false;
}

void PacedSender::SetProbingEnabled(bool enabled) {
  RTC_CHECK_EQ(0u, packet_counter_);
  probing_enabled_ = enabled;
}

void PacedSender::UpdateBitrate(int bitrate_kbps,
                                int max_bitrate_kbps,
                                int min_bitrate_kbps) {
  CriticalSectionScoped cs(critsect_.get());
  // Don't set media bitrate here as it may be boosted in order to meet max
  // queue time constraint. Just update max_bitrate_kbps_ and let media_budget_
  // be updated in Process().
  padding_budget_->set_target_rate_kbps(min_bitrate_kbps);
  bitrate_bps_ = 1000 * bitrate_kbps;
  max_bitrate_kbps_ = max_bitrate_kbps;
}

void PacedSender::InsertPacket(RtpPacketSender::Priority priority,
                               uint32_t ssrc,
                               uint16_t sequence_number,
                               int64_t capture_time_ms,
                               size_t bytes,
                               bool retransmission) {
  CriticalSectionScoped cs(critsect_.get());

  if (probing_enabled_ && !prober_->IsProbing())
    prober_->SetEnabled(true);
  prober_->MaybeInitializeProbe(bitrate_bps_);

  int64_t now_ms = clock_->TimeInMilliseconds();
  if (capture_time_ms < 0)
    capture_time_ms = now_ms;

  packets_->Push(paced_sender::Packet(priority, ssrc, sequence_number,
                                      capture_time_ms, now_ms, bytes,
                                      retransmission, packet_counter_++));
}

int64_t PacedSender::ExpectedQueueTimeMs() const {
  CriticalSectionScoped cs(critsect_.get());
  RTC_DCHECK_GT(max_bitrate_kbps_, 0);
  return static_cast<int64_t>(packets_->SizeInBytes() * 8 / max_bitrate_kbps_);
}

size_t PacedSender::QueueSizePackets() const {
  CriticalSectionScoped cs(critsect_.get());
  return packets_->SizeInPackets();
}

int64_t PacedSender::QueueInMs() const {
  CriticalSectionScoped cs(critsect_.get());

  int64_t oldest_packet = packets_->OldestEnqueueTimeMs();
  if (oldest_packet == 0)
    return 0;

  return clock_->TimeInMilliseconds() - oldest_packet;
}

int64_t PacedSender::AverageQueueTimeMs() {
  CriticalSectionScoped cs(critsect_.get());
  packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
  return packets_->AverageQueueTimeMs();
}

int64_t PacedSender::TimeUntilNextProcess() {
  CriticalSectionScoped cs(critsect_.get());
  if (prober_->IsProbing()) {
    int64_t ret = prober_->TimeUntilNextProbe(clock_->TimeInMilliseconds());
    if (ret >= 0)
      return ret;
  }
  int64_t elapsed_time_us = clock_->TimeInMicroseconds() - time_last_update_us_;
  int64_t elapsed_time_ms = (elapsed_time_us + 500) / 1000;
  return std::max<int64_t>(kMinPacketLimitMs - elapsed_time_ms, 0);
}

int32_t PacedSender::Process() {
  int64_t now_us = clock_->TimeInMicroseconds();
  CriticalSectionScoped cs(critsect_.get());
  int64_t elapsed_time_ms = (now_us - time_last_update_us_ + 500) / 1000;
  time_last_update_us_ = now_us;
  int target_bitrate_kbps = max_bitrate_kbps_;
  // TODO(holmer): Remove the !paused_ check when issue 5307 has been fixed.
  if (!paused_ && elapsed_time_ms > 0) {
    size_t queue_size_bytes = packets_->SizeInBytes();
    if (queue_size_bytes > 0) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packets_->UpdateQueueTime(clock_->TimeInMilliseconds());
      int64_t avg_time_left_ms = std::max<int64_t>(
          1, kMaxQueueLengthMs - packets_->AverageQueueTimeMs());
      int min_bitrate_needed_kbps =
          static_cast<int>(queue_size_bytes * 8 / avg_time_left_ms);
      if (min_bitrate_needed_kbps > target_bitrate_kbps)
        target_bitrate_kbps = min_bitrate_needed_kbps;
    }

    media_budget_->set_target_rate_kbps(target_bitrate_kbps);

    int64_t delta_time_ms = std::min(kMaxIntervalTimeMs, elapsed_time_ms);
    UpdateBytesPerInterval(delta_time_ms);
  }
  while (!packets_->Empty()) {
    if (media_budget_->bytes_remaining() == 0 && !prober_->IsProbing())
      return 0;

    // Since we need to release the lock in order to send, we first pop the
    // element from the priority queue but keep it in storage, so that we can
    // reinsert it if send fails.
    const paced_sender::Packet& packet = packets_->BeginPop();

    if (SendPacket(packet)) {
      // Send succeeded, remove it from the queue.
      packets_->FinalizePop(packet);
      if (prober_->IsProbing())
        return 0;
    } else {
      // Send failed, put it back into the queue.
      packets_->CancelPop(packet);
      return 0;
    }
  }

  // TODO(holmer): Remove the paused_ check when issue 5307 has been fixed.
  if (paused_ || !packets_->Empty())
    return 0;

  size_t padding_needed;
  if (prober_->IsProbing()) {
    padding_needed = prober_->RecommendedPacketSize();
  } else {
    padding_needed = padding_budget_->bytes_remaining();
  }

  if (padding_needed > 0)
    SendPadding(static_cast<size_t>(padding_needed));
  return 0;
}

bool PacedSender::SendPacket(const paced_sender::Packet& packet) {
  // TODO(holmer): Because of this bug issue 5307 we have to send audio
  // packets even when the pacer is paused. Here we assume audio packets are
  // always high priority and that they are the only high priority packets.
  if (paused_ && packet.priority != kHighPriority)
    return false;
  critsect_->Leave();
  const bool success = callback_->TimeToSendPacket(packet.ssrc,
                                                   packet.sequence_number,
                                                   packet.capture_time_ms,
                                                   packet.retransmission);
  critsect_->Enter();

  // TODO(holmer): High priority packets should only be accounted for if we are
  // allocating bandwidth for audio.
  if (success && packet.priority != kHighPriority) {
    // Update media bytes sent.
    prober_->PacketSent(clock_->TimeInMilliseconds(), packet.bytes);
    media_budget_->UseBudget(packet.bytes);
    padding_budget_->UseBudget(packet.bytes);
  }

  return success;
}

void PacedSender::SendPadding(size_t padding_needed) {
  critsect_->Leave();
  size_t bytes_sent = callback_->TimeToSendPadding(padding_needed);
  critsect_->Enter();

  if (bytes_sent > 0) {
    prober_->PacketSent(clock_->TimeInMilliseconds(), bytes_sent);
    media_budget_->UseBudget(bytes_sent);
    padding_budget_->UseBudget(bytes_sent);
  }
}

void PacedSender::UpdateBytesPerInterval(int64_t delta_time_ms) {
  media_budget_->IncreaseBudget(delta_time_ms);
  padding_budget_->IncreaseBudget(delta_time_ms);
}
}  // namespace webrtc
