/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/include/paced_sender.h"

#include <assert.h>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace {
// Time limit in milliseconds between packet bursts.
const int kMinPacketLimitMs = 5;

// Upper cap on process interval, in case process has not been called in a long
// time.
const int kMaxIntervalTimeMs = 30;

// Max time that the first packet in the queue can sit in the queue if no
// packets are sent, regardless of buffer state. In practice only in effect at
// low bitrates (less than 320 kbits/s).
const int kMaxQueueTimeWithoutSendingMs = 30;
}  // namespace

namespace webrtc {

bool PacedSender::PacketList::empty() const {
  return packet_list_.empty();
}

PacedSender::Packet PacedSender::PacketList::front() const {
  return packet_list_.front();
}

void PacedSender::PacketList::pop_front() {
  PacedSender::Packet& packet = packet_list_.front();
  uint16_t sequence_number = packet.sequence_number_;
  packet_list_.pop_front();
  sequence_number_set_.erase(sequence_number);
}

void PacedSender::PacketList::push_back(const PacedSender::Packet& packet) {
  if (sequence_number_set_.find(packet.sequence_number_) ==
      sequence_number_set_.end()) {
    // Don't insert duplicates.
    packet_list_.push_back(packet);
    sequence_number_set_.insert(packet.sequence_number_);
  }
}

PacedSender::PacedSender(Callback* callback, int target_bitrate_kbps,
                         float pace_multiplier)
    : callback_(callback),
      pace_multiplier_(pace_multiplier),
      enable_(false),
      paused_(false),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      target_bitrate_kbytes_per_s_(target_bitrate_kbps >> 3),  // Divide by 8.
      bytes_remaining_interval_(0),
      padding_bytes_remaining_interval_(0),
      time_last_update_(TickTime::Now()),
      capture_time_ms_last_queued_(0),
      capture_time_ms_last_sent_(0) {
  UpdateBytesPerInterval(kMinPacketLimitMs);
}

PacedSender::~PacedSender() {
}

void PacedSender::Pause() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = true;
}

void PacedSender::Resume() {
  CriticalSectionScoped cs(critsect_.get());
  paused_ = false;
}

void PacedSender::SetStatus(bool enable) {
  CriticalSectionScoped cs(critsect_.get());
  enable_ = enable;
}

void PacedSender::UpdateBitrate(int target_bitrate_kbps) {
  CriticalSectionScoped cs(critsect_.get());
  target_bitrate_kbytes_per_s_ = target_bitrate_kbps >> 3;  // Divide by 8.
}

bool PacedSender::SendPacket(Priority priority, uint32_t ssrc,
    uint16_t sequence_number, int64_t capture_time_ms, int bytes) {
  CriticalSectionScoped cs(critsect_.get());

  if (!enable_) {
    UpdateState(bytes);
    return true;  // We can send now.
  }
  if (capture_time_ms < 0) {
    capture_time_ms = TickTime::MillisecondTimestamp();
  }
  if (paused_) {
    // Queue all packets when we are paused.
    switch (priority) {
      case kHighPriority:
        high_priority_packets_.push_back(
            Packet(ssrc, sequence_number, capture_time_ms, bytes));
        break;
      case kNormalPriority:
        if (capture_time_ms > capture_time_ms_last_queued_) {
          capture_time_ms_last_queued_ = capture_time_ms;
          TRACE_EVENT_ASYNC_BEGIN1("webrtc_rtp", "PacedSend", capture_time_ms,
                                   "capture_time_ms", capture_time_ms);
        }
      case kLowPriority:
        // Queue the low priority packets in the normal priority queue when we
        // are paused to avoid starvation.
        normal_priority_packets_.push_back(
            Packet(ssrc, sequence_number, capture_time_ms, bytes));
        break;
    }
    return false;
  }

  switch (priority) {
    case kHighPriority:
      if (high_priority_packets_.empty() &&
          bytes_remaining_interval_ > 0) {
        UpdateState(bytes);
        return true;  // We can send now.
      }
      high_priority_packets_.push_back(
          Packet(ssrc, sequence_number, capture_time_ms, bytes));
      return false;
    case kNormalPriority:
      if (high_priority_packets_.empty() &&
          normal_priority_packets_.empty() &&
          bytes_remaining_interval_ > 0) {
        UpdateState(bytes);
        return true;  // We can send now.
      }
      if (capture_time_ms > capture_time_ms_last_queued_) {
        capture_time_ms_last_queued_ = capture_time_ms;
        TRACE_EVENT_ASYNC_BEGIN1("webrtc_rtp", "PacedSend", capture_time_ms,
                                 "capture_time_ms", capture_time_ms);
      }
      normal_priority_packets_.push_back(
          Packet(ssrc, sequence_number, capture_time_ms, bytes));
      return false;
    case kLowPriority:
      if (high_priority_packets_.empty() &&
          normal_priority_packets_.empty() &&
          low_priority_packets_.empty() &&
          bytes_remaining_interval_ > 0) {
        UpdateState(bytes);
        return true;  // We can send now.
      }
      low_priority_packets_.push_back(
          Packet(ssrc, sequence_number, capture_time_ms, bytes));
      return false;
  }
  return false;
}

int PacedSender::QueueInMs() const {
  CriticalSectionScoped cs(critsect_.get());
  int64_t now_ms = TickTime::MillisecondTimestamp();
  int64_t oldest_packet_capture_time = now_ms;
  if (!high_priority_packets_.empty()) {
    oldest_packet_capture_time = std::min(
        oldest_packet_capture_time,
        high_priority_packets_.front().capture_time_ms_);
  }
  if (!normal_priority_packets_.empty()) {
    oldest_packet_capture_time = std::min(
        oldest_packet_capture_time,
        normal_priority_packets_.front().capture_time_ms_);
  }
  if (!low_priority_packets_.empty()) {
    oldest_packet_capture_time = std::min(
        oldest_packet_capture_time,
        low_priority_packets_.front().capture_time_ms_);
  }
  return now_ms - oldest_packet_capture_time;
}

int32_t PacedSender::TimeUntilNextProcess() {
  CriticalSectionScoped cs(critsect_.get());
  int64_t elapsed_time_ms =
      (TickTime::Now() - time_last_update_).Milliseconds();
  if (elapsed_time_ms <= 0) {
    return kMinPacketLimitMs;
  }
  if (elapsed_time_ms >= kMinPacketLimitMs) {
    return 0;
  }
  return kMinPacketLimitMs - elapsed_time_ms;
}

int32_t PacedSender::Process() {
  TickTime now = TickTime::Now();
  CriticalSectionScoped cs(critsect_.get());
  int elapsed_time_ms = (now - time_last_update_).Milliseconds();
  time_last_update_ = now;
  if (!paused_ && elapsed_time_ms > 0) {
    uint32_t delta_time_ms = std::min(kMaxIntervalTimeMs, elapsed_time_ms);
    UpdateBytesPerInterval(delta_time_ms);
    uint32_t ssrc;
    uint16_t sequence_number;
    int64_t capture_time_ms;
    Priority priority;
    bool last_packet;
    while (GetNextPacket(&ssrc, &sequence_number, &capture_time_ms,
                         &priority, &last_packet)) {
      if (priority == kNormalPriority) {
        if (capture_time_ms > capture_time_ms_last_sent_) {
          capture_time_ms_last_sent_ = capture_time_ms;
        } else if (capture_time_ms == capture_time_ms_last_sent_ &&
                   last_packet) {
          TRACE_EVENT_ASYNC_END0("webrtc_rtp", "PacedSend", capture_time_ms);
        }
      }
      critsect_->Leave();
      callback_->TimeToSendPacket(ssrc, sequence_number, capture_time_ms);
      critsect_->Enter();
    }
    if (high_priority_packets_.empty() &&
        normal_priority_packets_.empty() &&
        low_priority_packets_.empty() &&
        padding_bytes_remaining_interval_ > 0) {
      critsect_->Leave();
      callback_->TimeToSendPadding(padding_bytes_remaining_interval_);
      critsect_->Enter();
      padding_bytes_remaining_interval_ = 0;
      bytes_remaining_interval_ -= padding_bytes_remaining_interval_;
    }
  }
  return 0;
}

// MUST have critsect_ when calling.
void PacedSender::UpdateBytesPerInterval(uint32_t delta_time_ms) {
  uint32_t bytes_per_interval = target_bitrate_kbytes_per_s_ * delta_time_ms;

  if (bytes_remaining_interval_ < 0) {
    // We overused last interval, compensate this interval.
    bytes_remaining_interval_ += pace_multiplier_ * bytes_per_interval;
  } else {
    // If we underused last interval we can't use it this interval.
    bytes_remaining_interval_ = pace_multiplier_ * bytes_per_interval;
  }
  if (padding_bytes_remaining_interval_ < 0) {
    // We overused last interval, compensate this interval.
    padding_bytes_remaining_interval_ += bytes_per_interval;
  } else {
    // If we underused last interval we can't use it this interval.
    padding_bytes_remaining_interval_ = bytes_per_interval;
  }
}

// MUST have critsect_ when calling.
bool PacedSender::GetNextPacket(uint32_t* ssrc, uint16_t* sequence_number,
                                int64_t* capture_time_ms, Priority* priority,
                                bool* last_packet) {
  if (bytes_remaining_interval_ <= 0) {
    // All bytes consumed for this interval.
    // Check if we have not sent in a too long time.
    if ((TickTime::Now() - time_last_send_).Milliseconds() >
        kMaxQueueTimeWithoutSendingMs) {
      if (!high_priority_packets_.empty()) {
        *priority = kHighPriority;
        GetNextPacketFromList(&high_priority_packets_, ssrc, sequence_number,
                              capture_time_ms, last_packet);
        return true;
      }
      if (!normal_priority_packets_.empty()) {
        *priority = kNormalPriority;
        GetNextPacketFromList(&normal_priority_packets_, ssrc, sequence_number,
                              capture_time_ms, last_packet);
        return true;
      }
    }
    return false;
  }
  if (!high_priority_packets_.empty()) {
    *priority = kHighPriority;
    GetNextPacketFromList(&high_priority_packets_, ssrc, sequence_number,
                          capture_time_ms, last_packet);
    return true;
  }
  if (!normal_priority_packets_.empty()) {
    *priority = kNormalPriority;
    GetNextPacketFromList(&normal_priority_packets_, ssrc, sequence_number,
                          capture_time_ms, last_packet);
    return true;
  }
  if (!low_priority_packets_.empty()) {
    *priority = kLowPriority;
    GetNextPacketFromList(&low_priority_packets_, ssrc, sequence_number,
                          capture_time_ms, last_packet);
    return true;
  }
  return false;
}

void PacedSender::GetNextPacketFromList(PacketList* list,
    uint32_t* ssrc, uint16_t* sequence_number, int64_t* capture_time_ms,
    bool* last_packet) {
  Packet packet = list->front();
  UpdateState(packet.bytes_);
  *sequence_number = packet.sequence_number_;
  *ssrc = packet.ssrc_;
  *capture_time_ms = packet.capture_time_ms_;
  list->pop_front();
  *last_packet = list->empty() ||
      list->front().capture_time_ms_ > *capture_time_ms;
}

// MUST have critsect_ when calling.
void PacedSender::UpdateState(int num_bytes) {
  time_last_send_ = TickTime::Now();
  bytes_remaining_interval_ -= num_bytes;
  padding_bytes_remaining_interval_ -= num_bytes;
}

}  // namespace webrtc
