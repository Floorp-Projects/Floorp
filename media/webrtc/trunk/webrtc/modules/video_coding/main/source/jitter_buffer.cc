/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_coding/main/source/jitter_buffer.h"

#include <algorithm>
#include <cassert>

#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/inter_frame_delay.h"
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer_common.h"
#include "webrtc/modules/video_coding/main/source/jitter_estimator.h"
#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

// Use this rtt if no value has been reported.
static const uint32_t kDefaultRtt = 200;

typedef std::pair<uint32_t, VCMFrameBuffer*> FrameListPair;

bool IsKeyFrame(FrameListPair pair) {
  return pair.second->FrameType() == kVideoFrameKey;
}

bool HasNonEmptyState(FrameListPair pair) {
  return pair.second->GetState() != kStateEmpty;
}

void FrameList::InsertFrame(VCMFrameBuffer* frame) {
  insert(rbegin().base(), FrameListPair(frame->TimeStamp(), frame));
}

VCMFrameBuffer* FrameList::FindFrame(uint32_t timestamp) const {
  FrameList::const_iterator it = find(timestamp);
  if (it == end())
    return NULL;
  return it->second;
}

VCMFrameBuffer* FrameList::PopFrame(uint32_t timestamp) {
  FrameList::iterator it = find(timestamp);
  if (it == end())
    return NULL;
  VCMFrameBuffer* frame = it->second;
  erase(it);
  return frame;
}

VCMFrameBuffer* FrameList::Front() const {
  return begin()->second;
}

VCMFrameBuffer* FrameList::Back() const {
  return rbegin()->second;
}

int FrameList::RecycleFramesUntilKeyFrame(FrameList::iterator* key_frame_it) {
  int drop_count = 0;
  FrameList::iterator it = begin();
  while (!empty()) {
    // Throw at least one frame.
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding, -1,
                 "Recycling: type=%s, low seqnum=%u",
                 it->second->FrameType() == kVideoFrameKey ?
                 "key" : "delta", it->second->GetLowSeqNum());
    if (it->second->GetState() != kStateDecoding) {
      it->second->SetState(kStateFree);
    }
    erase(it++);
    ++drop_count;
    if (it != end() && it->second->FrameType() == kVideoFrameKey) {
      *key_frame_it = it;
      return drop_count;
    }
  }
  *key_frame_it = end();
  return drop_count;
}

int FrameList::CleanUpOldOrEmptyFrames(VCMDecodingState* decoding_state) {
  int drop_count = 0;
  while (!empty()) {
    VCMFrameBuffer* oldest_frame = Front();
    bool remove_frame = false;
    if (oldest_frame->GetState() == kStateEmpty && size() > 1) {
      // This frame is empty, try to update the last decoded state and drop it
      // if successful.
      remove_frame = decoding_state->UpdateEmptyFrame(oldest_frame);
    } else {
      remove_frame = decoding_state->IsOldFrame(oldest_frame);
    }
    if (!remove_frame) {
      break;
    }
    if (oldest_frame->GetState() != kStateDecoding) {
      oldest_frame->SetState(kStateFree);
    }
    ++drop_count;
    TRACE_EVENT_INSTANT1("webrtc", "JB::OldOrEmptyFrameDropped", "timestamp",
                         oldest_frame->TimeStamp());
    erase(begin());
  }
  if (empty()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied",
                         "type", "CleanUpOldOrEmptyFrames");
  }
  return drop_count;
}

VCMJitterBuffer::VCMJitterBuffer(Clock* clock,
                                 EventFactory* event_factory,
                                 int vcm_id,
                                 int receiver_id,
                                 bool master)
    : vcm_id_(vcm_id),
      receiver_id_(receiver_id),
      clock_(clock),
      running_(false),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      master_(master),
      frame_event_(event_factory->CreateEvent()),
      packet_event_(event_factory->CreateEvent()),
      max_number_of_frames_(kStartNumberOfFrames),
      frame_buffers_(),
      decodable_frames_(),
      incomplete_frames_(),
      last_decoded_state_(),
      first_packet_since_reset_(true),
      num_not_decodable_packets_(0),
      receive_statistics_(),
      incoming_frame_rate_(0),
      incoming_frame_count_(0),
      time_last_incoming_frame_count_(0),
      incoming_bit_count_(0),
      incoming_bit_rate_(0),
      drop_count_(0),
      num_consecutive_old_frames_(0),
      num_consecutive_old_packets_(0),
      num_discarded_packets_(0),
      jitter_estimate_(vcm_id, receiver_id),
      inter_frame_delay_(clock_->TimeInMilliseconds()),
      rtt_ms_(kDefaultRtt),
      nack_mode_(kNoNack),
      low_rtt_nack_threshold_ms_(-1),
      high_rtt_nack_threshold_ms_(-1),
      missing_sequence_numbers_(SequenceNumberLessThan()),
      nack_seq_nums_(),
      max_nack_list_size_(0),
      max_packet_age_to_nack_(0),
      max_incomplete_time_ms_(0),
      decode_with_errors_(false) {
  memset(frame_buffers_, 0, sizeof(frame_buffers_));
  memset(receive_statistics_, 0, sizeof(receive_statistics_));

  for (int i = 0; i < kStartNumberOfFrames; i++) {
    frame_buffers_[i] = new VCMFrameBuffer();
  }
}

VCMJitterBuffer::~VCMJitterBuffer() {
  Stop();
  for (int i = 0; i < kMaxNumberOfFrames; i++) {
    if (frame_buffers_[i]) {
      delete frame_buffers_[i];
    }
  }
  delete crit_sect_;
}

void VCMJitterBuffer::CopyFrom(const VCMJitterBuffer& rhs) {
  if (this != &rhs) {
    crit_sect_->Enter();
    rhs.crit_sect_->Enter();
    vcm_id_ = rhs.vcm_id_;
    receiver_id_ = rhs.receiver_id_;
    running_ = rhs.running_;
    master_ = !rhs.master_;
    max_number_of_frames_ = rhs.max_number_of_frames_;
    incoming_frame_rate_ = rhs.incoming_frame_rate_;
    incoming_frame_count_ = rhs.incoming_frame_count_;
    time_last_incoming_frame_count_ = rhs.time_last_incoming_frame_count_;
    incoming_bit_count_ = rhs.incoming_bit_count_;
    incoming_bit_rate_ = rhs.incoming_bit_rate_;
    drop_count_ = rhs.drop_count_;
    num_consecutive_old_frames_ = rhs.num_consecutive_old_frames_;
    num_consecutive_old_packets_ = rhs.num_consecutive_old_packets_;
    num_discarded_packets_ = rhs.num_discarded_packets_;
    jitter_estimate_ = rhs.jitter_estimate_;
    inter_frame_delay_ = rhs.inter_frame_delay_;
    waiting_for_completion_ = rhs.waiting_for_completion_;
    rtt_ms_ = rhs.rtt_ms_;
    first_packet_since_reset_ = rhs.first_packet_since_reset_;
    last_decoded_state_ =  rhs.last_decoded_state_;
    num_not_decodable_packets_ = rhs.num_not_decodable_packets_;
    decode_with_errors_ = rhs.decode_with_errors_;
    assert(max_nack_list_size_ == rhs.max_nack_list_size_);
    assert(max_packet_age_to_nack_ == rhs.max_packet_age_to_nack_);
    assert(max_incomplete_time_ms_ == rhs.max_incomplete_time_ms_);
    memcpy(receive_statistics_, rhs.receive_statistics_,
           sizeof(receive_statistics_));
    nack_seq_nums_.resize(rhs.nack_seq_nums_.size());
    missing_sequence_numbers_ = rhs.missing_sequence_numbers_;
    latest_received_sequence_number_ = rhs.latest_received_sequence_number_;
    for (int i = 0; i < kMaxNumberOfFrames; i++) {
      if (frame_buffers_[i] != NULL) {
        delete frame_buffers_[i];
        frame_buffers_[i] = NULL;
      }
    }
    decodable_frames_.clear();
    int i = 0;
    for (FrameList::const_iterator it = rhs.decodable_frames_.begin();
         it != rhs.decodable_frames_.end(); ++it, ++i) {
      frame_buffers_[i] = new VCMFrameBuffer(*it->second);
      decodable_frames_.insert(decodable_frames_.rbegin().base(),
          FrameListPair(frame_buffers_[i]->TimeStamp(), frame_buffers_[i]));
    }
    incomplete_frames_.clear();
    for (FrameList::const_iterator it = rhs.incomplete_frames_.begin();
         it != rhs.incomplete_frames_.end(); ++it, ++i) {
      frame_buffers_[i] = new VCMFrameBuffer(*it->second);
      incomplete_frames_.insert(incomplete_frames_.rbegin().base(),
          FrameListPair(frame_buffers_[i]->TimeStamp(), frame_buffers_[i]));
    }
    rhs.crit_sect_->Leave();
    crit_sect_->Leave();
  }
}

void VCMJitterBuffer::Start() {
  CriticalSectionScoped cs(crit_sect_);
  running_ = true;
  incoming_frame_count_ = 0;
  incoming_frame_rate_ = 0;
  incoming_bit_count_ = 0;
  incoming_bit_rate_ = 0;
  time_last_incoming_frame_count_ = clock_->TimeInMilliseconds();
  memset(receive_statistics_, 0, sizeof(receive_statistics_));

  num_consecutive_old_frames_ = 0;
  num_consecutive_old_packets_ = 0;
  num_discarded_packets_ = 0;

  // Start in a non-signaled state.
  frame_event_->Reset();
  packet_event_->Reset();
  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_since_reset_ = true;
  rtt_ms_ = kDefaultRtt;
  num_not_decodable_packets_ = 0;
  last_decoded_state_.Reset();

  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: start",
               this);
}

void VCMJitterBuffer::Stop() {
  crit_sect_->Enter();
  running_ = false;
  last_decoded_state_.Reset();
  decodable_frames_.clear();
  incomplete_frames_.clear();
  TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied", "type", "Stop");
  for (int i = 0; i < kMaxNumberOfFrames; i++) {
    if (frame_buffers_[i] != NULL) {
      static_cast<VCMFrameBuffer*>(frame_buffers_[i])->SetState(kStateFree);
    }
  }

  crit_sect_->Leave();
  // Make sure we wake up any threads waiting on these events.
  frame_event_->Set();
  packet_event_->Set();
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: stop",
               this);
}

bool VCMJitterBuffer::Running() const {
  CriticalSectionScoped cs(crit_sect_);
  return running_;
}

void VCMJitterBuffer::Flush() {
  CriticalSectionScoped cs(crit_sect_);
  // Erase all frames from the sorted list and set their state to free.
  decodable_frames_.clear();
  incomplete_frames_.clear();
  TRACE_EVENT_INSTANT2("webrtc", "JB::FrameListEmptied", "type", "Flush",
                       "frames", max_number_of_frames_);
  for (int i = 0; i < max_number_of_frames_; i++) {
    ReleaseFrameIfNotDecoding(frame_buffers_[i]);
  }
  last_decoded_state_.Reset();  // TODO(mikhal): sync reset.
  num_not_decodable_packets_ = 0;
  frame_event_->Reset();
  packet_event_->Reset();
  num_consecutive_old_frames_ = 0;
  num_consecutive_old_packets_ = 0;
  // Also reset the jitter and delay estimates
  jitter_estimate_.Reset();
  inter_frame_delay_.Reset(clock_->TimeInMilliseconds());
  waiting_for_completion_.frame_size = 0;
  waiting_for_completion_.timestamp = 0;
  waiting_for_completion_.latest_packet_time = -1;
  first_packet_since_reset_ = true;
  missing_sequence_numbers_.clear();
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_), "JB(0x%x): Jitter buffer: flush",
               this);
}

// Get received key and delta frames
void VCMJitterBuffer::FrameStatistics(uint32_t* received_delta_frames,
                                      uint32_t* received_key_frames) const {
  assert(received_delta_frames);
  assert(received_key_frames);
  CriticalSectionScoped cs(crit_sect_);
  *received_delta_frames = receive_statistics_[1] + receive_statistics_[3];
  *received_key_frames = receive_statistics_[0] + receive_statistics_[2];
}

int VCMJitterBuffer::num_not_decodable_packets() const {
  CriticalSectionScoped cs(crit_sect_);
  return num_not_decodable_packets_;
}

int VCMJitterBuffer::num_discarded_packets() const {
  CriticalSectionScoped cs(crit_sect_);
  return num_discarded_packets_;
}

// Calculate framerate and bitrate.
void VCMJitterBuffer::IncomingRateStatistics(unsigned int* framerate,
                                             unsigned int* bitrate) {
  assert(framerate);
  assert(bitrate);
  CriticalSectionScoped cs(crit_sect_);
  const int64_t now = clock_->TimeInMilliseconds();
  int64_t diff = now - time_last_incoming_frame_count_;
  if (diff < 1000 && incoming_frame_rate_ > 0 && incoming_bit_rate_ > 0) {
    // Make sure we report something even though less than
    // 1 second has passed since last update.
    *framerate = incoming_frame_rate_;
    *bitrate = incoming_bit_rate_;
  } else if (incoming_frame_count_ != 0) {
    // We have received frame(s) since last call to this function

    // Prepare calculations
    if (diff <= 0) {
      diff = 1;
    }
    // we add 0.5f for rounding
    float rate = 0.5f + ((incoming_frame_count_ * 1000.0f) / diff);
    if (rate < 1.0f) {
      rate = 1.0f;
    }

    // Calculate frame rate
    // Let r be rate.
    // r(0) = 1000*framecount/delta_time.
    // (I.e. frames per second since last calculation.)
    // frame_rate = r(0)/2 + r(-1)/2
    // (I.e. fr/s average this and the previous calculation.)
    *framerate = (incoming_frame_rate_ + static_cast<unsigned int>(rate)) / 2;
    incoming_frame_rate_ = static_cast<unsigned int>(rate);

    // Calculate bit rate
    if (incoming_bit_count_ == 0) {
      *bitrate = 0;
    } else {
      *bitrate = 10 * ((100 * incoming_bit_count_) /
                       static_cast<unsigned int>(diff));
    }
    incoming_bit_rate_ = *bitrate;

    // Reset count
    incoming_frame_count_ = 0;
    incoming_bit_count_ = 0;
    time_last_incoming_frame_count_ = now;

  } else {
    // No frames since last call
    time_last_incoming_frame_count_ = clock_->TimeInMilliseconds();
    *framerate = 0;
    *bitrate = 0;
    incoming_frame_rate_ = 0;
    incoming_bit_rate_ = 0;
  }
  TRACE_COUNTER1("webrtc", "JBIncomingFramerate", incoming_frame_rate_);
  TRACE_COUNTER1("webrtc", "JBIncomingBitrate", incoming_bit_rate_);
}

// Answers the question:
// Will the packet sequence be complete if the next frame is grabbed for
// decoding right now? That is, have we lost a frame between the last decoded
// frame and the next, or is the next
// frame missing one or more packets?
bool VCMJitterBuffer::CompleteSequenceWithNextFrame() {
  CriticalSectionScoped cs(crit_sect_);
  // Finding oldest frame ready for decoder, check sequence number and size
  CleanUpOldOrEmptyFrames();
  if (!decodable_frames_.empty())
    return true;
  if (incomplete_frames_.size() <= 1) {
    // Frame not ready to be decoded.
    return true;
  }
  return false;
}

// Returns immediately or a |max_wait_time_ms| ms event hang waiting for a
// complete frame, |max_wait_time_ms| decided by caller.
bool VCMJitterBuffer::NextCompleteTimestamp(
    uint32_t max_wait_time_ms, uint32_t* timestamp) {
  TRACE_EVENT0("webrtc", "JB::NextCompleteTimestamp");
  crit_sect_->Enter();
  if (!running_) {
    crit_sect_->Leave();
    return false;
  }
  CleanUpOldOrEmptyFrames();

  if (decodable_frames_.empty()) {
    const int64_t end_wait_time_ms = clock_->TimeInMilliseconds() +
        max_wait_time_ms;
    int64_t wait_time_ms = max_wait_time_ms;
    while (wait_time_ms > 0) {
      crit_sect_->Leave();
      const EventTypeWrapper ret =
        frame_event_->Wait(static_cast<uint32_t>(wait_time_ms));
      crit_sect_->Enter();
      if (ret == kEventSignaled) {
        // Are we shutting down the jitter buffer?
        if (!running_) {
          crit_sect_->Leave();
          return false;
        }
        // Finding oldest frame ready for decoder, but check
        // sequence number and size
        CleanUpOldOrEmptyFrames();
        if (decodable_frames_.empty()) {
          wait_time_ms = end_wait_time_ms - clock_->TimeInMilliseconds();
        } else {
          break;
        }
      } else {
        break;
      }
    }
    // Inside |crit_sect_|.
  } else {
    // We already have a frame, reset the event.
    frame_event_->Reset();
  }
  if (decodable_frames_.empty()) {
    crit_sect_->Leave();
    return false;
  }
  *timestamp = decodable_frames_.Front()->TimeStamp();
  crit_sect_->Leave();
  return true;
}

bool VCMJitterBuffer::NextMaybeIncompleteTimestamp(uint32_t* timestamp) {
  TRACE_EVENT0("webrtc", "JB::NextMaybeIncompleteTimestamp");
  CriticalSectionScoped cs(crit_sect_);
  if (!running_) {
    return false;
  }
  if (!decode_with_errors_) {
    // No point to continue, as we are not decoding with errors.
    return false;
  }

  CleanUpOldOrEmptyFrames();

  VCMFrameBuffer* oldest_frame = NextFrame();
  if (!oldest_frame) {
    return false;
  }
  if (decodable_frames_.empty() && incomplete_frames_.size() <= 1 &&
      oldest_frame->GetState() == kStateIncomplete) {
    // If we have only one frame in the buffer, release it only if it is
    // complete.
    return false;
  }
  // Always start with a complete key frame.
  if (last_decoded_state_.in_initial_state() &&
      oldest_frame->FrameType() != kVideoFrameKey) {
    return false;
  }

  *timestamp = oldest_frame->TimeStamp();
  return true;
}

VCMEncodedFrame* VCMJitterBuffer::ExtractAndSetDecode(uint32_t timestamp) {
  TRACE_EVENT0("webrtc", "JB::ExtractAndSetDecode");
  CriticalSectionScoped cs(crit_sect_);

  if (!running_) {
    return NULL;
  }
  // Extract the frame with the desired timestamp.
  VCMFrameBuffer* frame = decodable_frames_.PopFrame(timestamp);
  if (!frame) {
    frame = incomplete_frames_.PopFrame(timestamp);
    if (!frame)
      return NULL;
  }
  if (!NextFrame()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied",
                         "type", "ExtractAndSetDecode");
  }
  // Frame pulled out from jitter buffer, update the jitter estimate.
  const bool retransmitted = (frame->GetNackCount() > 0);
  if (retransmitted) {
    jitter_estimate_.FrameNacked();
  } else if (frame->Length() > 0) {
    // Ignore retransmitted and empty frames.
    if (waiting_for_completion_.latest_packet_time >= 0) {
      UpdateJitterEstimate(waiting_for_completion_, true);
    }
    if (frame->GetState() == kStateComplete) {
      UpdateJitterEstimate(*frame, false);
    } else {
      // Wait for this one to get complete.
      waiting_for_completion_.frame_size = frame->Length();
      waiting_for_completion_.latest_packet_time =
          frame->LatestPacketTimeMs();
      waiting_for_completion_.timestamp = frame->TimeStamp();
    }
  }
  // Look for previous frame loss.
  VerifyAndSetPreviousFrameLost(frame);

  // The state must be changed to decoding before cleaning up zero sized
  // frames to avoid empty frames being cleaned up and then given to the
  // decoder. Propagates the missing_frame bit.
  frame->SetState(kStateDecoding);

  num_not_decodable_packets_ += frame->NotDecodablePackets();

  // We have a frame - update the last decoded state and nack list.
  last_decoded_state_.SetState(frame);
  DropPacketsFromNackList(last_decoded_state_.sequence_num());
  return frame;
}

// Release frame when done with decoding. Should never be used to release
// frames from within the jitter buffer.
void VCMJitterBuffer::ReleaseFrame(VCMEncodedFrame* frame) {
  CriticalSectionScoped cs(crit_sect_);
  VCMFrameBuffer* frame_buffer = static_cast<VCMFrameBuffer*>(frame);
  if (frame_buffer)
    frame_buffer->SetState(kStateFree);
}

// Gets frame to use for this timestamp. If no match, get empty frame.
VCMFrameBufferEnum VCMJitterBuffer::GetFrame(const VCMPacket& packet,
                                             VCMFrameBuffer** frame) {
  // Does this packet belong to an old frame?
  if (last_decoded_state_.IsOldPacket(&packet)) {
    // Account only for media packets.
    if (packet.sizeBytes > 0) {
      num_discarded_packets_++;
      num_consecutive_old_packets_++;
      TRACE_EVENT_INSTANT2("webrtc", "JB::OldPacketDropped",
                           "seqnum", packet.seqNum,
                           "timestamp", packet.timestamp);
      TRACE_COUNTER1("webrtc", "JBDroppedOldPackets", num_discarded_packets_);
    }
    // Update last decoded sequence number if the packet arrived late and
    // belongs to a frame with a timestamp equal to the last decoded
    // timestamp.
    last_decoded_state_.UpdateOldPacket(&packet);
    DropPacketsFromNackList(last_decoded_state_.sequence_num());

    if (num_consecutive_old_packets_ > kMaxConsecutiveOldPackets) {
      Flush();
      return kFlushIndicator;
    }
    return kOldPacket;
  }
  num_consecutive_old_packets_ = 0;

  *frame = incomplete_frames_.FindFrame(packet.timestamp);
  if (*frame) {
    return kNoError;
  }
  *frame = decodable_frames_.FindFrame(packet.timestamp);
  if (*frame) {
    return kNoError;
  }
  // No match, return empty frame.
  *frame = GetEmptyFrame();
  if (*frame != NULL) {
    return kNoError;
  }
  // No free frame! Try to reclaim some...
  LOG_F(LS_INFO) << "Unable to get empty frame; Recycling.";
  RecycleFramesUntilKeyFrame();

  *frame = GetEmptyFrame();
  if (*frame != NULL) {
    return kNoError;
  }
  return kGeneralError;
}

int64_t VCMJitterBuffer::LastPacketTime(const VCMEncodedFrame* frame,
                                        bool* retransmitted) const {
  assert(retransmitted);
  CriticalSectionScoped cs(crit_sect_);
  const VCMFrameBuffer* frame_buffer =
      static_cast<const VCMFrameBuffer*>(frame);
  *retransmitted = (frame_buffer->GetNackCount() > 0);
  return frame_buffer->LatestPacketTimeMs();
}

VCMFrameBufferEnum VCMJitterBuffer::InsertPacket(const VCMPacket& packet,
                                                 bool* retransmitted) {
  CriticalSectionScoped cs(crit_sect_);
  int64_t now_ms = clock_->TimeInMilliseconds();
  VCMFrameBufferEnum buffer_return = kSizeError;
  VCMFrameBufferEnum ret = kSizeError;
  VCMFrameBuffer* frame = NULL;
  const VCMFrameBufferEnum error = GetFrame(packet, &frame);
  if (error != kNoError) {
    return error;
  }

  // We are keeping track of the first seq num, the latest seq num and
  // the number of wraps to be able to calculate how many packets we expect.
  if (first_packet_since_reset_) {
    // Now it's time to start estimating jitter
    // reset the delay estimate.
    inter_frame_delay_.Reset(clock_->TimeInMilliseconds());
  }
  if (last_decoded_state_.IsOldPacket(&packet)) {
    // This packet belongs to an old, already decoded frame, we want to update
    // the last decoded sequence number.
    last_decoded_state_.UpdateOldPacket(&packet);
    frame->SetState(kStateFree);
    TRACE_EVENT_INSTANT1("webrtc", "JB::DropLateFrame",
                         "timestamp", frame->TimeStamp());
    drop_count_++;
    // Flush() if this happens consistently.
    num_consecutive_old_frames_++;
    if (num_consecutive_old_frames_ > kMaxConsecutiveOldFrames) {
      Flush();
      return kFlushIndicator;
    }
    return kNoError;
  }
  num_consecutive_old_frames_ = 0;

  // Empty packets may bias the jitter estimate (lacking size component),
  // therefore don't let empty packet trigger the following updates:
  if (packet.frameType != kFrameEmpty) {
    if (waiting_for_completion_.timestamp == packet.timestamp) {
      // This can get bad if we have a lot of duplicate packets,
      // we will then count some packet multiple times.
      waiting_for_completion_.frame_size += packet.sizeBytes;
      waiting_for_completion_.latest_packet_time = now_ms;
    } else if (waiting_for_completion_.latest_packet_time >= 0 &&
               waiting_for_completion_.latest_packet_time + 2000 <= now_ms) {
      // A packet should never be more than two seconds late
      UpdateJitterEstimate(waiting_for_completion_, true);
      waiting_for_completion_.latest_packet_time = -1;
      waiting_for_completion_.frame_size = 0;
      waiting_for_completion_.timestamp = 0;
    }
  }

  VCMFrameBufferStateEnum previous_state = frame->GetState();
  // Insert packet.
  // Check for first packet. High sequence number will be -1 if neither an empty
  // packet nor a media packet has been inserted.
  bool first = (frame->GetHighSeqNum() == -1);
  // When in Hybrid mode, we allow for a decodable state
  // Note: Under current version, a decodable frame will never be
  // triggered, as the body of the function is empty.
  // TODO(mikhal): Update when decodable is enabled.
  buffer_return = frame->InsertPacket(packet, now_ms,
                                      decode_with_errors_,
                                      rtt_ms_);
  ret = buffer_return;
  if (buffer_return > 0) {
    incoming_bit_count_ += packet.sizeBytes << 3;
    if (first_packet_since_reset_) {
      latest_received_sequence_number_ = packet.seqNum;
      first_packet_since_reset_ = false;
    } else {
      if (IsPacketRetransmitted(packet)) {
        frame->IncrementNackCount();
      }
      if (!UpdateNackList(packet.seqNum)) {
        LOG_F(LS_INFO) << "Requesting key frame due to flushed NACK list.";
        buffer_return = kFlushIndicator;
      }
      latest_received_sequence_number_ = LatestSequenceNumber(
          latest_received_sequence_number_, packet.seqNum);
    }
  }
  switch (buffer_return) {
    case kGeneralError:
    case kTimeStampError:
    case kSizeError: {
      if (frame != NULL) {
        frame->Reset();
        frame->SetState(kStateFree);
      }
      break;
    }
    case kCompleteSession: {
      // Don't let the first packet be overridden by a complete session.
      ret = kCompleteSession;
      // Only update return value for a JB flush indicator.
      UpdateFrameState(frame);
      *retransmitted = (frame->GetNackCount() > 0);
      if (IsContinuous(*frame) && previous_state != kStateComplete) {
        if (!first) {
          incomplete_frames_.PopFrame(packet.timestamp);
        }
        decodable_frames_.InsertFrame(frame);
        FindAndInsertContinuousFrames(*frame);
        // Signal that we have a decodable frame.
        frame_event_->Set();
      } else if (first) {
        incomplete_frames_.InsertFrame(frame);
      }
      // Signal that we have a received packet.
      packet_event_->Set();
      break;
    }
    case kDecodableSession:
    case kIncomplete: {
      // No point in storing empty continuous frames.
      if (frame->GetState() == kStateEmpty &&
          last_decoded_state_.UpdateEmptyFrame(frame)) {
        frame->SetState(kStateFree);
        ret = kNoError;
      } else if (first) {
        ret = kFirstPacket;
        incomplete_frames_.InsertFrame(frame);
        // Signal that we have received a packet.
        packet_event_->Set();
      }
      break;
    }
    case kNoError:
    case kDuplicatePacket: {
      break;
    }
    case kFlushIndicator:
      ret = kFlushIndicator;
      break;
    default: {
      assert(false && "JitterBuffer::InsertPacket: Undefined value");
    }
  }
  return ret;
}

bool VCMJitterBuffer::IsContinuousInState(const VCMFrameBuffer& frame,
    const VCMDecodingState& decoding_state) const {
  // Is this frame complete or decodable and continuous?
  if ((frame.GetState() == kStateComplete ||
       (decode_with_errors_ && frame.GetState() == kStateDecodable)) &&
       decoding_state.ContinuousFrame(&frame)) {
    return true;
  } else {
    return false;
  }
}

bool VCMJitterBuffer::IsContinuous(const VCMFrameBuffer& frame) const {
  if (IsContinuousInState(frame, last_decoded_state_)) {
    return true;
  }
  VCMDecodingState decoding_state;
  decoding_state.CopyFrom(last_decoded_state_);
  for (FrameList::const_iterator it = decodable_frames_.begin();
       it != decodable_frames_.end(); ++it)  {
    VCMFrameBuffer* decodable_frame = it->second;
    if (IsNewerTimestamp(decodable_frame->TimeStamp(), frame.TimeStamp())) {
      break;
    }
    decoding_state.SetState(decodable_frame);
    if (IsContinuousInState(frame, decoding_state)) {
      return true;
    }
  }
  return false;
}

void VCMJitterBuffer::FindAndInsertContinuousFrames(
    const VCMFrameBuffer& new_frame) {
  VCMDecodingState decoding_state;
  decoding_state.CopyFrom(last_decoded_state_);
  decoding_state.SetState(&new_frame);
  // When temporal layers are available, we search for a complete or decodable
  // frame until we hit one of the following:
  // 1. Continuous base or sync layer.
  // 2. The end of the list was reached.
  for (FrameList::iterator it = incomplete_frames_.begin();
       it != incomplete_frames_.end();)  {
    VCMFrameBuffer* frame = it->second;
    if (IsNewerTimestamp(new_frame.TimeStamp(), frame->TimeStamp())) {
      ++it;
      continue;
    }
    if (IsContinuousInState(*frame, decoding_state)) {
      decodable_frames_.InsertFrame(frame);
      incomplete_frames_.erase(it++);
      decoding_state.SetState(frame);
    } else if (frame->TemporalId() <= 0) {
      break;
    } else {
      ++it;
    }
  }
}

void VCMJitterBuffer::SetMaxJitterEstimate(bool enable) {
  CriticalSectionScoped cs(crit_sect_);
  jitter_estimate_.SetMaxJitterEstimate(enable);
}

uint32_t VCMJitterBuffer::EstimatedJitterMs() {
  CriticalSectionScoped cs(crit_sect_);
  // Compute RTT multiplier for estimation.
  // low_rtt_nackThresholdMs_ == -1 means no FEC.
  double rtt_mult = 1.0f;
  if (low_rtt_nack_threshold_ms_ >= 0 &&
      static_cast<int>(rtt_ms_) >= low_rtt_nack_threshold_ms_) {
    // For RTTs above low_rtt_nack_threshold_ms_ we don't apply extra delay
    // when waiting for retransmissions.
    rtt_mult = 0.0f;
  }
  return jitter_estimate_.GetJitterEstimate(rtt_mult);
}

void VCMJitterBuffer::UpdateRtt(uint32_t rtt_ms) {
  CriticalSectionScoped cs(crit_sect_);
  rtt_ms_ = rtt_ms;
  jitter_estimate_.UpdateRtt(rtt_ms);
}

void VCMJitterBuffer::SetNackMode(VCMNackMode mode,
                                  int low_rtt_nack_threshold_ms,
                                  int high_rtt_nack_threshold_ms) {
  CriticalSectionScoped cs(crit_sect_);
  nack_mode_ = mode;
  if (mode == kNoNack) {
    missing_sequence_numbers_.clear();
  }
  assert(low_rtt_nack_threshold_ms >= -1 && high_rtt_nack_threshold_ms >= -1);
  assert(high_rtt_nack_threshold_ms == -1 ||
         low_rtt_nack_threshold_ms <= high_rtt_nack_threshold_ms);
  assert(low_rtt_nack_threshold_ms > -1 || high_rtt_nack_threshold_ms == -1);
  low_rtt_nack_threshold_ms_ = low_rtt_nack_threshold_ms;
  high_rtt_nack_threshold_ms_ = high_rtt_nack_threshold_ms;
  // Don't set a high start rtt if high_rtt_nack_threshold_ms_ is used, to not
  // disable NACK in hybrid mode.
  if (rtt_ms_ == kDefaultRtt && high_rtt_nack_threshold_ms_ != -1) {
    rtt_ms_ = 0;
  }
  if (!WaitForRetransmissions()) {
    jitter_estimate_.ResetNackCount();
  }
}

void VCMJitterBuffer::SetNackSettings(size_t max_nack_list_size,
                                      int max_packet_age_to_nack,
                                      int max_incomplete_time_ms) {
  CriticalSectionScoped cs(crit_sect_);
  assert(max_packet_age_to_nack >= 0);
  assert(max_incomplete_time_ms_ >= 0);
  max_nack_list_size_ = max_nack_list_size;
  max_packet_age_to_nack_ = max_packet_age_to_nack;
  max_incomplete_time_ms_ = max_incomplete_time_ms;
  nack_seq_nums_.resize(max_nack_list_size_);
}

VCMNackMode VCMJitterBuffer::nack_mode() const {
  CriticalSectionScoped cs(crit_sect_);
  return nack_mode_;
}

int VCMJitterBuffer::NonContinuousOrIncompleteDuration() {
  if (incomplete_frames_.empty()) {
    return 0;
  }
  uint32_t start_timestamp = incomplete_frames_.Front()->TimeStamp();
  if (!decodable_frames_.empty()) {
    start_timestamp = decodable_frames_.Back()->TimeStamp();
  }
  return incomplete_frames_.Back()->TimeStamp() - start_timestamp;
}

uint16_t VCMJitterBuffer::EstimatedLowSequenceNumber(
    const VCMFrameBuffer& frame) const {
  assert(frame.GetLowSeqNum() >= 0);
  if (frame.HaveFirstPacket())
    return frame.GetLowSeqNum();

  // This estimate is not accurate if more than one packet with lower sequence
  // number is lost.
  return frame.GetLowSeqNum() - 1;
}

uint16_t* VCMJitterBuffer::GetNackList(uint16_t* nack_list_size,
                                       bool* request_key_frame) {
  CriticalSectionScoped cs(crit_sect_);
  *request_key_frame = false;
  if (nack_mode_ == kNoNack) {
    *nack_list_size = 0;
    return NULL;
  }
  if (last_decoded_state_.in_initial_state()) {
    const bool first_frame_is_key = NextFrame() &&
        NextFrame()->FrameType() == kVideoFrameKey &&
        NextFrame()->HaveFirstPacket();
    if (!first_frame_is_key) {
      bool have_non_empty_frame = decodable_frames_.end() != find_if(
          decodable_frames_.begin(), decodable_frames_.end(),
          HasNonEmptyState);
      if (!have_non_empty_frame) {
        have_non_empty_frame = incomplete_frames_.end() != find_if(
            incomplete_frames_.begin(), incomplete_frames_.end(),
            HasNonEmptyState);
      }
      if (have_non_empty_frame) {
        LOG_F(LS_INFO) << "First frame is not key; Recycling.";
      }
      bool found_key_frame = RecycleFramesUntilKeyFrame();
      if (!found_key_frame) {
        *request_key_frame = have_non_empty_frame;
        *nack_list_size = 0;
        return NULL;
      }
    }
  }
  if (TooLargeNackList()) {
    TRACE_EVENT_INSTANT1("webrtc", "JB::NackListTooLarge",
                         "size", missing_sequence_numbers_.size());
    *request_key_frame = !HandleTooLargeNackList();
  }
  if (max_incomplete_time_ms_ > 0) {
    int non_continuous_incomplete_duration =
        NonContinuousOrIncompleteDuration();
    if (non_continuous_incomplete_duration > 90 * max_incomplete_time_ms_) {
      TRACE_EVENT_INSTANT1("webrtc", "JB::NonContinuousOrIncompleteDuration",
                           "duration", non_continuous_incomplete_duration);
      LOG_F(LS_INFO) << "Too long non-decodable duration: " <<
          non_continuous_incomplete_duration << " > " <<
          90 * max_incomplete_time_ms_;
      FrameList::reverse_iterator rit = find_if(incomplete_frames_.rbegin(),
          incomplete_frames_.rend(), IsKeyFrame);
      if (rit == incomplete_frames_.rend()) {
        // Request a key frame if we don't have one already.
        *request_key_frame = true;
        *nack_list_size = 0;
        return NULL;
      } else {
        // Skip to the last key frame. If it's incomplete we will start
        // NACKing it.
        // Note that the estimated low sequence number is correct for VP8
        // streams because only the first packet of a key frame is marked.
        last_decoded_state_.Reset();
        DropPacketsFromNackList(EstimatedLowSequenceNumber(*rit->second));
      }
    }
  }
  unsigned int i = 0;
  SequenceNumberSet::iterator it = missing_sequence_numbers_.begin();
  for (; it != missing_sequence_numbers_.end(); ++it, ++i) {
    nack_seq_nums_[i] = *it;
  }
  *nack_list_size = i;
  return &nack_seq_nums_[0];
}

VCMFrameBuffer* VCMJitterBuffer::NextFrame() const {
  if (!decodable_frames_.empty())
    return decodable_frames_.Front();
  if (!incomplete_frames_.empty())
    return incomplete_frames_.Front();
  return NULL;
}

bool VCMJitterBuffer::UpdateNackList(uint16_t sequence_number) {
  if (nack_mode_ == kNoNack) {
    return true;
  }
  // Make sure we don't add packets which are already too old to be decoded.
  if (!last_decoded_state_.in_initial_state()) {
    latest_received_sequence_number_ = LatestSequenceNumber(
        latest_received_sequence_number_,
        last_decoded_state_.sequence_num());
  }
  if (IsNewerSequenceNumber(sequence_number,
                            latest_received_sequence_number_)) {
    // Push any missing sequence numbers to the NACK list.
    for (uint16_t i = latest_received_sequence_number_ + 1;
         IsNewerSequenceNumber(sequence_number, i); ++i) {
      missing_sequence_numbers_.insert(missing_sequence_numbers_.end(), i);
      TRACE_EVENT_INSTANT1("webrtc", "AddNack", "seqnum", i);
    }
    if (TooLargeNackList() && !HandleTooLargeNackList()) {
      return false;
    }
    if (MissingTooOldPacket(sequence_number) &&
        !HandleTooOldPackets(sequence_number)) {
      return false;
    }
  } else {
    missing_sequence_numbers_.erase(sequence_number);
    TRACE_EVENT_INSTANT1("webrtc", "RemoveNack", "seqnum", sequence_number);
  }
  return true;
}

bool VCMJitterBuffer::TooLargeNackList() const {
  return missing_sequence_numbers_.size() > max_nack_list_size_;
}

bool VCMJitterBuffer::HandleTooLargeNackList() {
  // Recycle frames until the NACK list is small enough. It is likely cheaper to
  // request a key frame than to retransmit this many missing packets.
  LOG_F(LS_INFO) << "NACK list has grown too large: " <<
      missing_sequence_numbers_.size() << " > " << max_nack_list_size_;
  bool key_frame_found = false;
  while (TooLargeNackList()) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

bool VCMJitterBuffer::MissingTooOldPacket(
    uint16_t latest_sequence_number) const {
  if (missing_sequence_numbers_.empty()) {
    return false;
  }
  const uint16_t age_of_oldest_missing_packet = latest_sequence_number -
      *missing_sequence_numbers_.begin();
  // Recycle frames if the NACK list contains too old sequence numbers as
  // the packets may have already been dropped by the sender.
  return age_of_oldest_missing_packet > max_packet_age_to_nack_;
}

bool VCMJitterBuffer::HandleTooOldPackets(uint16_t latest_sequence_number) {
  bool key_frame_found = false;
  const uint16_t age_of_oldest_missing_packet = latest_sequence_number -
      *missing_sequence_numbers_.begin();
  LOG_F(LS_INFO) << "NACK list contains too old sequence numbers: " <<
      age_of_oldest_missing_packet << " > " << max_packet_age_to_nack_;
  while (MissingTooOldPacket(latest_sequence_number)) {
    key_frame_found = RecycleFramesUntilKeyFrame();
  }
  return key_frame_found;
}

void VCMJitterBuffer::DropPacketsFromNackList(
    uint16_t last_decoded_sequence_number) {
 TRACE_EVENT_INSTANT1("webrtc", "JB::DropPacketsFromNackList",
                      "seqnum", last_decoded_sequence_number);
  // Erase all sequence numbers from the NACK list which we won't need any
  // longer.
  missing_sequence_numbers_.erase(missing_sequence_numbers_.begin(),
                                  missing_sequence_numbers_.upper_bound(
                                      last_decoded_sequence_number));
}

int64_t VCMJitterBuffer::LastDecodedTimestamp() const {
  CriticalSectionScoped cs(crit_sect_);
  return last_decoded_state_.time_stamp();
}

void VCMJitterBuffer::RenderBufferSize(uint32_t* timestamp_start,
                                       uint32_t* timestamp_end) {
  CriticalSectionScoped cs(crit_sect_);
  CleanUpOldOrEmptyFrames();
  *timestamp_start = 0;
  *timestamp_end = 0;
  if (decodable_frames_.empty()) {
    return;
  }
  *timestamp_start = decodable_frames_.Front()->TimeStamp();
  *timestamp_end = decodable_frames_.Back()->TimeStamp();
}

// Set the frame state to free and remove it from the sorted
// frame list. Must be called from inside the critical section crit_sect_.
void VCMJitterBuffer::ReleaseFrameIfNotDecoding(VCMFrameBuffer* frame) {
  if (frame != NULL && frame->GetState() != kStateDecoding) {
    frame->SetState(kStateFree);
  }
}

VCMFrameBuffer* VCMJitterBuffer::GetEmptyFrame() {
  for (int i = 0; i < max_number_of_frames_; ++i) {
    if (kStateFree == frame_buffers_[i]->GetState()) {
      // found a free buffer
      frame_buffers_[i]->SetState(kStateEmpty);
      return frame_buffers_[i];
    }
  }

  // Check if we can increase JB size
  if (max_number_of_frames_ < kMaxNumberOfFrames) {
    VCMFrameBuffer* ptr_new_buffer = new VCMFrameBuffer();
    ptr_new_buffer->SetState(kStateEmpty);
    frame_buffers_[max_number_of_frames_] = ptr_new_buffer;
    max_number_of_frames_++;

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "JB(0x%x) FB(0x%x): Jitter buffer  increased to:%d frames",
                 this, ptr_new_buffer, max_number_of_frames_);
    TRACE_COUNTER1("webrtc", "JBMaxFrames", max_number_of_frames_);
    return ptr_new_buffer;
  }

  // We have reached max size, cannot increase JB size
  return NULL;
}

// Recycle oldest frames up to a key frame, used if jitter buffer is completely
// full.
bool VCMJitterBuffer::RecycleFramesUntilKeyFrame() {
  // First release incomplete frames, and only release decodable frames if there
  // are no incomplete ones.
  FrameList::iterator key_frame_it;
  bool key_frame_found = false;
  int dropped_frames = 0;
  dropped_frames += incomplete_frames_.RecycleFramesUntilKeyFrame(
      &key_frame_it);
  key_frame_found = key_frame_it != incomplete_frames_.end();
  if (dropped_frames == 0) {
    dropped_frames += decodable_frames_.RecycleFramesUntilKeyFrame(
        &key_frame_it);
    key_frame_found = key_frame_it != decodable_frames_.end();
    if (!key_frame_found) {
      TRACE_EVENT_INSTANT1("webrtc", "JB::FrameListEmptied", "type",
                           "RecycleFramesUntilKeyFrame");
    }
  }
  drop_count_ += dropped_frames;
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_),
               "Jitter buffer drop count:%u", drop_count_);
  TRACE_EVENT_INSTANT0("webrtc", "JB::RecycleFramesUntilKeyFrame");
  if (key_frame_found) {
    // Reset last decoded state to make sure the next frame decoded is a key
    // frame, and start NACKing from here.
    last_decoded_state_.Reset();
    DropPacketsFromNackList(EstimatedLowSequenceNumber(*key_frame_it->second));
  } else if (decodable_frames_.empty()) {
    last_decoded_state_.Reset();  // TODO(mikhal): No sync.
    missing_sequence_numbers_.clear();
  }
  return key_frame_found;
}

// Must be called under the critical section |crit_sect_|.
void VCMJitterBuffer::UpdateFrameState(VCMFrameBuffer* frame) {
  if (master_) {
    // Only trace the primary jitter buffer to make it possible to parse
    // and plot the trace file.
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "JB(0x%x) FB(0x%x): Complete frame added to jitter buffer,"
                 " size:%d type %d",
                 this, frame, frame->Length(), frame->FrameType());
  }

  bool frame_counted = false;
  if (!frame->GetCountedFrame()) {
    // Ignore ACK frames.
    incoming_frame_count_++;
    frame->SetCountedFrame(true);
    frame_counted = true;
  }

  frame->SetState(kStateComplete);
  if (frame->FrameType() == kVideoFrameKey) {
    TRACE_EVENT_INSTANT2("webrtc", "JB::AddKeyFrame",
                         "timestamp", frame->TimeStamp(),
                         "retransmit", !frame_counted);
  } else {
    TRACE_EVENT_INSTANT2("webrtc", "JB::AddFrame",
                         "timestamp", frame->TimeStamp(),
                         "retransmit", !frame_counted);
  }

  // Update receive statistics. We count all layers, thus when you use layers
  // adding all key and delta frames might differ from frame count.
  if (frame->IsSessionComplete()) {
    switch (frame->FrameType()) {
      case kVideoFrameKey: {
        receive_statistics_[0]++;
        break;
      }
      case kVideoFrameDelta: {
        receive_statistics_[1]++;
        break;
      }
      case kVideoFrameGolden: {
        receive_statistics_[2]++;
        break;
      }
      case kVideoFrameAltRef: {
        receive_statistics_[3]++;
        break;
      }
      default:
        assert(false);
    }
  }
}

// Must be called under the critical section |crit_sect_|.
void VCMJitterBuffer::CleanUpOldOrEmptyFrames() {
  drop_count_ +=
      decodable_frames_.CleanUpOldOrEmptyFrames(&last_decoded_state_);
  drop_count_ +=
      incomplete_frames_.CleanUpOldOrEmptyFrames(&last_decoded_state_);
  TRACE_COUNTER1("webrtc", "JBDroppedLateFrames", drop_count_);
  if (!last_decoded_state_.in_initial_state()) {
    DropPacketsFromNackList(last_decoded_state_.sequence_num());
  }
}

void VCMJitterBuffer::VerifyAndSetPreviousFrameLost(VCMFrameBuffer* frame) {
  assert(frame);
  frame->MakeSessionDecodable();  // Make sure the session can be decoded.
  if (frame->FrameType() == kVideoFrameKey)
    return;

  if (!last_decoded_state_.ContinuousFrame(frame))
    frame->SetPreviousFrameLoss();
}

// Must be called from within |crit_sect_|.
bool VCMJitterBuffer::IsPacketRetransmitted(const VCMPacket& packet) const {
  return missing_sequence_numbers_.find(packet.seqNum) !=
      missing_sequence_numbers_.end();
}

// Must be called under the critical section |crit_sect_|. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(const VCMJitterSample& sample,
                                           bool incomplete_frame) {
  if (sample.latest_packet_time == -1) {
    return;
  }
  if (incomplete_frame) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received incomplete frame "
                 "timestamp %u frame size %u at time %u",
                 sample.timestamp, sample.frame_size,
                 MaskWord64ToUWord32(sample.latest_packet_time));
  } else {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received complete frame "
                 "timestamp %u frame size %u at time %u",
                 sample.timestamp, sample.frame_size,
                 MaskWord64ToUWord32(sample.latest_packet_time));
  }
  UpdateJitterEstimate(sample.latest_packet_time, sample.timestamp,
                       sample.frame_size, incomplete_frame);
}

// Must be called under the critical section crit_sect_. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(const VCMFrameBuffer& frame,
                                           bool incomplete_frame) {
  if (frame.LatestPacketTimeMs() == -1) {
    return;
  }
  // No retransmitted frames should be a part of the jitter
  // estimate.
  if (incomplete_frame) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_),
                 "Received incomplete frame timestamp %u frame type %d "
                 "frame size %u at time %u, jitter estimate was %u",
                 frame.TimeStamp(), frame.FrameType(), frame.Length(),
                 MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                 EstimatedJitterMs());
  } else {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(vcm_id_, receiver_id_), "Received complete frame "
                 "timestamp %u frame type %d frame size %u at time %u, "
                 "jitter estimate was %u",
                 frame.TimeStamp(), frame.FrameType(), frame.Length(),
                 MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                 EstimatedJitterMs());
  }
  UpdateJitterEstimate(frame.LatestPacketTimeMs(), frame.TimeStamp(),
                       frame.Length(), incomplete_frame);
}

// Must be called under the critical section |crit_sect_|. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void VCMJitterBuffer::UpdateJitterEstimate(
    int64_t latest_packet_time_ms,
    uint32_t timestamp,
    unsigned int frame_size,
    bool incomplete_frame) {
  if (latest_packet_time_ms == -1) {
    return;
  }
  int64_t frame_delay;
  // Calculate the delay estimate
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
               VCMId(vcm_id_, receiver_id_),
               "Packet received and sent to jitter estimate with: "
               "timestamp=%u wall_clock=%u", timestamp,
               MaskWord64ToUWord32(latest_packet_time_ms));
  bool not_reordered = inter_frame_delay_.CalculateDelay(timestamp,
                                                      &frame_delay,
                                                      latest_packet_time_ms);
  // Filter out frames which have been reordered in time by the network
  if (not_reordered) {
    // Update the jitter estimate with the new samples
    jitter_estimate_.UpdateEstimate(frame_delay, frame_size, incomplete_frame);
  }
}

bool VCMJitterBuffer::WaitForRetransmissions() {
  if (nack_mode_ == kNoNack) {
    // NACK disabled -> don't wait for retransmissions.
    return false;
  }
  // Evaluate if the RTT is higher than |high_rtt_nack_threshold_ms_|, and in
  // that case we don't wait for retransmissions.
  if (high_rtt_nack_threshold_ms_ >= 0 &&
      rtt_ms_ >= static_cast<unsigned int>(high_rtt_nack_threshold_ms_)) {
    return false;
  }
  return true;
}
}  // namespace webrtc
