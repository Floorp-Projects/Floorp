/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

#include <stdio.h>

#include <sstream>

namespace webrtc {
namespace testing {
namespace bwe {
class DelayCapHelper {
 public:
  DelayCapHelper() : max_delay_us_(0), delay_stats_() {}

  void SetMaxDelay(int max_delay_ms) {
    BWE_TEST_LOGGING_ENABLE(false);
    BWE_TEST_LOGGING_LOG1("Max Delay", "%d ms", static_cast<int>(max_delay_ms));
    assert(max_delay_ms >= 0);
    max_delay_us_ = max_delay_ms * 1000;
  }

  bool ShouldSendPacket(int64_t send_time_us, int64_t arrival_time_us) {
    int64_t packet_delay_us = send_time_us - arrival_time_us;
    delay_stats_.Push(std::min(packet_delay_us, max_delay_us_) / 1000);
    return (max_delay_us_ == 0 || max_delay_us_ >= packet_delay_us);
  }

  const Stats<double>& delay_stats() const {
    return delay_stats_;
  }

 private:
  int64_t max_delay_us_;
  Stats<double> delay_stats_;

  DISALLOW_COPY_AND_ASSIGN(DelayCapHelper);
};

const FlowIds CreateFlowIds(const int *flow_ids_array, size_t num_flow_ids) {
  FlowIds flow_ids(&flow_ids_array[0], flow_ids_array + num_flow_ids);
  return flow_ids;
}

class RateCounter {
 public:
  RateCounter()
      : kWindowSizeUs(1000000),
        packets_per_second_(0),
        bytes_per_second_(0),
        last_accumulated_us_(0),
        window_() {}

  void UpdateRates(int64_t send_time_us, uint32_t payload_size) {
    packets_per_second_++;
    bytes_per_second_ += payload_size;
    last_accumulated_us_ = send_time_us;
    window_.push_back(std::make_pair(send_time_us, payload_size));
    while (!window_.empty()) {
      const TimeSizePair& packet = window_.front();
      if (packet.first > (last_accumulated_us_ - kWindowSizeUs)) {
        break;
      }
      assert(packets_per_second_ >= 1);
      assert(bytes_per_second_ >= packet.second);
      packets_per_second_--;
      bytes_per_second_ -= packet.second;
      window_.pop_front();
    }
  }

  uint32_t bits_per_second() const {
    return bytes_per_second_ * 8;
  }
  uint32_t packets_per_second() const { return packets_per_second_; }

 private:
  typedef std::pair<int64_t, uint32_t> TimeSizePair;

  const int64_t kWindowSizeUs;
  uint32_t packets_per_second_;
  uint32_t bytes_per_second_;
  int64_t last_accumulated_us_;
  std::list<TimeSizePair> window_;
};

Random::Random(uint32_t seed)
    : a_(0x531FDB97 ^ seed),
      b_(0x6420ECA8 + seed) {
}

float Random::Rand() {
  const float kScale = 1.0f / 0xffffffff;
  float result = kScale * b_;
  a_ ^= b_;
  b_ += a_;
  return result;
}

int Random::Gaussian(int mean, int standard_deviation) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform, which is defined on the
  // interval (0, 1], hence the mask+add below.
  const double kPi = 3.14159265358979323846;
  const double kScale = 1.0 / 0x80000000ul;
  double u1 = kScale * ((a_ & 0x7ffffffful) + 1);
  double u2 = kScale * ((b_ & 0x7ffffffful) + 1);
  a_ ^= b_;
  b_ += a_;
  return static_cast<int>(mean + standard_deviation *
      sqrt(-2 * log(u1)) * cos(2 * kPi * u2));
}

Packet::Packet()
    : flow_id_(0),
      creation_time_us_(-1),
      send_time_us_(-1),
      payload_size_(0) {
  memset(&header_, 0, sizeof(header_));
}

Packet::Packet(int flow_id, int64_t send_time_us, uint32_t payload_size,
               const RTPHeader& header)
    : flow_id_(flow_id),
      creation_time_us_(send_time_us),
      send_time_us_(send_time_us),
      payload_size_(payload_size),
      header_(header) {
}

Packet::Packet(int64_t send_time_us, uint32_t sequence_number)
    : flow_id_(0),
      creation_time_us_(send_time_us),
      send_time_us_(send_time_us),
      payload_size_(0) {
  memset(&header_, 0, sizeof(header_));
  header_.sequenceNumber = sequence_number;
}

bool Packet::operator<(const Packet& rhs) const {
  return send_time_us_ < rhs.send_time_us_;
}

void Packet::set_send_time_us(int64_t send_time_us) {
  assert(send_time_us >= 0);
  send_time_us_ = send_time_us;
}

void Packet::SetAbsSendTimeMs(int64_t abs_send_time_ms) {
  header_.extension.hasAbsoluteSendTime = true;
  header_.extension.absoluteSendTime = ((static_cast<int64_t>(abs_send_time_ms *
    (1 << 18)) + 500) / 1000) & 0x00fffffful;
}

bool IsTimeSorted(const Packets& packets) {
  PacketsConstIt last_it = packets.begin();
  for (PacketsConstIt it = last_it; it != packets.end(); ++it) {
    if (it != last_it && *it < *last_it) {
      return false;
    }
    last_it = it;
  }
  return true;
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 bool is_sender)
    : listener_(listener), flow_ids_(1, 0) {
  if (listener_) {
    listener_->AddPacketProcessor(this, is_sender);
  }
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 const FlowIds& flow_ids,
                                 bool is_sender)
    : listener_(listener), flow_ids_(flow_ids) {
  if (listener_) {
    listener_->AddPacketProcessor(this, is_sender);
  }
}

PacketProcessor::~PacketProcessor() {
  if (listener_) {
    listener_->RemovePacketProcessor(this);
  }
}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_("") {}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     const std::string& name)
    : PacketProcessor(listener, false),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     const FlowIds& flow_ids,
                                     const std::string& name)
    : PacketProcessor(listener, flow_ids, false),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {
  std::stringstream ss;
  ss << name_ << "_";
  for (size_t i = 0; i < flow_ids.size(); ++i) {
    ss << flow_ids[i] << ",";
  }
  name_ = ss.str();
}

RateCounterFilter::~RateCounterFilter() {
  LogStats();
}

uint32_t RateCounterFilter::packets_per_second() const {
  return rate_counter_->packets_per_second();
}

uint32_t RateCounterFilter::bits_per_second() const {
  return rate_counter_->bits_per_second();
}

void RateCounterFilter::LogStats() {
  BWE_TEST_LOGGING_CONTEXT("RateCounterFilter");
  packets_per_second_stats_.Log("pps");
  kbps_stats_.Log("kbps");
}

Stats<double> RateCounterFilter::GetBitrateStats() const {
  return kbps_stats_;
}

void RateCounterFilter::Plot(int64_t timestamp_ms) {
  BWE_TEST_LOGGING_CONTEXT(name_.c_str());
  BWE_TEST_LOGGING_PLOT("Throughput_#1", timestamp_ms,
                        rate_counter_->bits_per_second() / 1000.0);
}

void RateCounterFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsConstIt it = in_out->begin(); it != in_out->end(); ++it) {
    rate_counter_->UpdateRates(it->send_time_us(), it->payload_size());
  }
  packets_per_second_stats_.Push(rate_counter_->packets_per_second());
  kbps_stats_.Push(rate_counter_->bits_per_second() / 1000.0);
}

LossFilter::LossFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      random_(0x12345678),
      loss_fraction_(0.0f) {
}

void LossFilter::SetLoss(float loss_percent) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Loss", "%f%%", loss_percent);
  assert(loss_percent >= 0.0f);
  assert(loss_percent <= 100.0f);
  loss_fraction_ = loss_percent * 0.01f;
}

void LossFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ) {
    if (random_.Rand() < loss_fraction_) {
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
}

DelayFilter::DelayFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      delay_us_(0),
      last_send_time_us_(0) {
}

void DelayFilter::SetDelay(int64_t delay_ms) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Delay", "%d ms", static_cast<int>(delay_ms));
  assert(delay_ms >= 0);
  delay_us_ = delay_ms * 1000;
}

void DelayFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ++it) {
    int64_t new_send_time_us = it->send_time_us() + delay_us_;
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    it->set_send_time_us(last_send_time_us_);
  }
}

JitterFilter::JitterFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      random_(0x89674523),
      stddev_jitter_us_(0),
      last_send_time_us_(0) {
}

void JitterFilter::SetJitter(int64_t stddev_jitter_ms) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Jitter", "%d ms",
                        static_cast<int>(stddev_jitter_ms));
  assert(stddev_jitter_ms >= 0);
  stddev_jitter_us_ = stddev_jitter_ms * 1000;
}

void JitterFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ++it) {
    int64_t new_send_time_us = it->send_time_us();
    new_send_time_us += random_.Gaussian(0, stddev_jitter_us_);
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    it->set_send_time_us(last_send_time_us_);
  }
}

ReorderFilter::ReorderFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      random_(0x27452389),
      reorder_fraction_(0.0f) {
}

void ReorderFilter::SetReorder(float reorder_percent) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("Reordering", "%f%%", reorder_percent);
  assert(reorder_percent >= 0.0f);
  assert(reorder_percent <= 100.0f);
  reorder_fraction_ = reorder_percent * 0.01f;
}

void ReorderFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  if (in_out->size() >= 2) {
    PacketsIt last_it = in_out->begin();
    PacketsIt it = last_it;
    while (++it != in_out->end()) {
      if (random_.Rand() < reorder_fraction_) {
        int64_t t1 = last_it->send_time_us();
        int64_t t2 = it->send_time_us();
        std::swap(*last_it, *it);
        last_it->set_send_time_us(t1);
        it->set_send_time_us(t2);
      }
      last_it = it;
    }
  }
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      kbps_(1200),
      last_send_time_us_(0),
      delay_cap_helper_(new DelayCapHelper()) {
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener,
                         const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, false),
      kbps_(1200),
      last_send_time_us_(0),
      delay_cap_helper_(new DelayCapHelper()) {
}

ChokeFilter::~ChokeFilter() {}

void ChokeFilter::SetCapacity(uint32_t kbps) {
  BWE_TEST_LOGGING_ENABLE(false);
  BWE_TEST_LOGGING_LOG1("BitrateChoke", "%d kbps", kbps);
  kbps_ = kbps;
}

void ChokeFilter::RunFor(int64_t /*time_ms*/, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end(); ) {
    int64_t earliest_send_time_us = last_send_time_us_ +
        (it->payload_size() * 8 * 1000 + kbps_ / 2) / kbps_;
    int64_t new_send_time_us = std::max(it->send_time_us(),
                                        earliest_send_time_us);
    if (delay_cap_helper_->ShouldSendPacket(new_send_time_us,
                                            it->send_time_us())) {
      it->set_send_time_us(new_send_time_us);
      last_send_time_us_ = new_send_time_us;
      ++it;
    } else {
      it = in_out->erase(it);
    }
  }
}

void ChokeFilter::SetMaxDelay(int max_delay_ms) {
  delay_cap_helper_->SetMaxDelay(max_delay_ms);
}

Stats<double> ChokeFilter::GetDelayStats() const {
  return delay_cap_helper_->delay_stats();
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener)
    : PacketProcessor(listener, false),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(""),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    const std::string& name)
    : PacketProcessor(listener, false),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(name),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {}

TraceBasedDeliveryFilter::~TraceBasedDeliveryFilter() {
}

bool TraceBasedDeliveryFilter::Init(const std::string& filename) {
  FILE* trace_file = fopen(filename.c_str(), "r");
  if (!trace_file) {
    return false;
  }
  int64_t first_timestamp = -1;
  while(!feof(trace_file)) {
    const size_t kMaxLineLength = 100;
    char line[kMaxLineLength];
    if (fgets(line, kMaxLineLength, trace_file)) {
      std::string line_string(line);
      std::istringstream buffer(line_string);
      int64_t timestamp;
      buffer >> timestamp;
      timestamp /= 1000;  // Convert to microseconds.
      if (first_timestamp == -1)
        first_timestamp = timestamp;
      assert(delivery_times_us_.empty() ||
             timestamp - first_timestamp - delivery_times_us_.back() >= 0);
      delivery_times_us_.push_back(timestamp - first_timestamp);
    }
  }
  assert(!delivery_times_us_.empty());
  next_delivery_it_ = delivery_times_us_.begin();
  fclose(trace_file);
  return true;
}

void TraceBasedDeliveryFilter::Plot(int64_t timestamp_ms) {
  BWE_TEST_LOGGING_CONTEXT(name_.c_str());
  // This plots the max possible throughput of the trace-based delivery filter,
  // which will be reached if a packet sent on every packet slot of the trace.
  BWE_TEST_LOGGING_PLOT("MaxThroughput_#1", timestamp_ms,
                        rate_counter_->bits_per_second() / 1000.0);
}

void TraceBasedDeliveryFilter::RunFor(int64_t time_ms, Packets* in_out) {
  assert(in_out);
  for (PacketsIt it = in_out->begin(); it != in_out->end();) {
    while (local_time_us_ < it->send_time_us()) {
      ProceedToNextSlot();
    }
    // Drop any packets that have been queued for too long.
    while (!delay_cap_helper_->ShouldSendPacket(local_time_us_,
                                                it->send_time_us())) {
      it = in_out->erase(it);
      if (it == in_out->end()) {
        return;
      }
    }
    if (local_time_us_ >= it->send_time_us()) {
      it->set_send_time_us(local_time_us_);
      ProceedToNextSlot();
    }
    ++it;
  }
  packets_per_second_stats_.Push(rate_counter_->packets_per_second());
  kbps_stats_.Push(rate_counter_->bits_per_second() / 1000.0);
}

void TraceBasedDeliveryFilter::SetMaxDelay(int max_delay_ms) {
  delay_cap_helper_->SetMaxDelay(max_delay_ms);
}

Stats<double> TraceBasedDeliveryFilter::GetDelayStats() const {
  return delay_cap_helper_->delay_stats();
}

Stats<double> TraceBasedDeliveryFilter::GetBitrateStats() const {
  return kbps_stats_;
}

void TraceBasedDeliveryFilter::ProceedToNextSlot() {
  if (*next_delivery_it_ <= local_time_us_) {
    ++next_delivery_it_;
    if (next_delivery_it_ == delivery_times_us_.end()) {
      // When the trace wraps we allow two packets to be sent back-to-back.
      for (TimeList::iterator it = delivery_times_us_.begin();
           it != delivery_times_us_.end(); ++it) {
        *it += local_time_us_ - current_offset_us_;
      }
      current_offset_us_ += local_time_us_ - current_offset_us_;
      next_delivery_it_ = delivery_times_us_.begin();
    }
  }
  local_time_us_ = *next_delivery_it_;
  const int kPayloadSize = 1200;
  rate_counter_->UpdateRates(local_time_us_, kPayloadSize);
}

PacketSender::PacketSender(PacketProcessorListener* listener)
    : PacketProcessor(listener, true) {}

PacketSender::PacketSender(PacketProcessorListener* listener,
                           const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, true) {

}

VideoSender::VideoSender(int flow_id,
                         PacketProcessorListener* listener,
                         float fps,
                         uint32_t kbps,
                         uint32_t ssrc,
                         float first_frame_offset)
    : PacketSender(listener, FlowIds(1, flow_id)),
      kMaxPayloadSizeBytes(1200),
      kTimestampBase(0xff80ff00ul),
      frame_period_ms_(1000.0 / fps),
      bytes_per_second_((1000 * kbps) / 8),
      frame_size_bytes_(bytes_per_second_ / fps),
      next_frame_ms_(frame_period_ms_ * first_frame_offset),
      now_ms_(0.0),
      prototype_header_() {
  assert(first_frame_offset >= 0.0f);
  assert(first_frame_offset < 1.0f);
  memset(&prototype_header_, 0, sizeof(prototype_header_));
  prototype_header_.ssrc = ssrc;
  prototype_header_.sequenceNumber = 0xf000u;
}

uint32_t VideoSender::GetCapacityKbps() const {
  return (bytes_per_second_ * 8) / 1000;
}

uint32_t VideoSender::NextFrameSize() {
  return frame_size_bytes_;
}

uint32_t VideoSender::NextPacketSize(uint32_t frame_size,
                                     uint32_t remaining_payload) {
  return std::min(kMaxPayloadSizeBytes, remaining_payload);
}

void VideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  assert(in_out);
  now_ms_ += time_ms;
  Packets new_packets;
  while (now_ms_ >= next_frame_ms_) {
    prototype_header_.timestamp = kTimestampBase +
        static_cast<uint32_t>(next_frame_ms_ * 90.0);
    prototype_header_.extension.transmissionTimeOffset = 0;

    // Generate new packets for this frame, all with the same timestamp,
    // but the payload size is capped, so if the whole frame doesn't fit in
    // one packet, we will see a number of equally sized packets followed by
    // one smaller at the tail.
    int64_t send_time_us = next_frame_ms_ * 1000.0;
    uint32_t frame_size = NextFrameSize();
    uint32_t payload_size = frame_size;

    while (payload_size > 0) {
      ++prototype_header_.sequenceNumber;
      uint32_t size = NextPacketSize(frame_size, payload_size);
      new_packets.push_back(Packet(flow_ids()[0], send_time_us, size,
                                   prototype_header_));
      new_packets.back().SetAbsSendTimeMs(next_frame_ms_);
      payload_size -= size;
    }

    next_frame_ms_ += frame_period_ms_;
  }
  in_out->merge(new_packets);
}

AdaptiveVideoSender::AdaptiveVideoSender(int flow_id,
                                         PacketProcessorListener* listener,
                                         float fps,
                                         uint32_t kbps,
                                         uint32_t ssrc,
                                         float first_frame_offset)
    : VideoSender(flow_id, listener, fps, kbps, ssrc, first_frame_offset) {
}

void AdaptiveVideoSender::GiveFeedback(const PacketSender::Feedback& feedback) {
  bytes_per_second_ = std::min(feedback.estimated_bps / 8, 2500000u / 8);
  frame_size_bytes_ = (bytes_per_second_ * frame_period_ms_ + 500) / 1000;
}

PeriodicKeyFrameSender::PeriodicKeyFrameSender(
    int flow_id,
    PacketProcessorListener* listener,
    float fps,
    uint32_t kbps,
    uint32_t ssrc,
    float first_frame_offset,
    int key_frame_interval)
    : AdaptiveVideoSender(flow_id,
                          listener,
                          fps,
                          kbps,
                          ssrc,
                          first_frame_offset),
      key_frame_interval_(key_frame_interval),
      frame_counter_(0),
      compensation_bytes_(0),
      compensation_per_frame_(0) {
}

uint32_t PeriodicKeyFrameSender::NextFrameSize() {
  uint32_t payload_size = frame_size_bytes_;
  if (frame_counter_ == 0) {
    payload_size = kMaxPayloadSizeBytes * 12;
    compensation_bytes_ = 4 * frame_size_bytes_;
    compensation_per_frame_ = compensation_bytes_ / 30;
  } else if (key_frame_interval_ > 0 &&
             (frame_counter_ % key_frame_interval_ == 0)) {
    payload_size *= 5;
    compensation_bytes_ = payload_size - frame_size_bytes_;
    compensation_per_frame_ = compensation_bytes_ / 30;
  } else if (compensation_bytes_ > 0) {
    if (compensation_per_frame_ > static_cast<int>(payload_size)) {
      // Skip this frame.
      compensation_bytes_ -= payload_size;
      payload_size = 0;
    } else {
      payload_size -= compensation_per_frame_;
      compensation_bytes_ -= compensation_per_frame_;
    }
  }
  if (compensation_bytes_ < 0)
    compensation_bytes_ = 0;
  ++frame_counter_;
  return payload_size;
}

uint32_t PeriodicKeyFrameSender::NextPacketSize(uint32_t frame_size,
                                                uint32_t remaining_payload) {
  uint32_t fragments =
      (frame_size + (kMaxPayloadSizeBytes - 1)) / kMaxPayloadSizeBytes;
  uint32_t avg_size = (frame_size + fragments - 1) / fragments;
  return std::min(avg_size, remaining_payload);
}

PacedVideoSender::PacedVideoSender(PacketProcessorListener* listener,
                                   uint32_t kbps,
                                   AdaptiveVideoSender* source)
    // It is important that the first_frame_offset and the initial time of
    // clock_ are both zero, otherwise we can't have absolute time in this
    // class.
    : PacketSender(listener, source->flow_ids()),
      clock_(0),
      start_of_run_ms_(0),
      pacer_(&clock_, this, kbps, PacedSender::kDefaultPaceMultiplier* kbps, 0),
      source_(source) {
}

void PacedVideoSender::RunFor(int64_t time_ms, Packets* in_out) {
  start_of_run_ms_ = clock_.TimeInMilliseconds();
  Packets generated_packets;
  source_->RunFor(time_ms, &generated_packets);
  // Run process periodically to allow the packets to be paced out.
  int64_t end_time_ms = clock_.TimeInMilliseconds() + time_ms;
  Packets::iterator it = generated_packets.begin();
  while (clock_.TimeInMilliseconds() <= end_time_ms) {
    int time_until_process_ms = pacer_.TimeUntilNextProcess();
    if (time_until_process_ms < 0)
      time_until_process_ms = 0;
    int time_until_packet_ms = time_ms;
    if (it != generated_packets.end())
      time_until_packet_ms =
          (it->send_time_us() + 500) / 1000 - clock_.TimeInMilliseconds();
    assert(time_until_packet_ms >= 0);
    int time_until_next_event_ms = time_until_packet_ms;
    if (time_until_process_ms < time_until_packet_ms &&
        pacer_.QueueSizePackets() > 0)
      time_until_next_event_ms = time_until_process_ms;

    if (clock_.TimeInMilliseconds() + time_until_next_event_ms > end_time_ms) {
      clock_.AdvanceTimeMilliseconds(end_time_ms - clock_.TimeInMilliseconds());
      break;
    }
    clock_.AdvanceTimeMilliseconds(time_until_next_event_ms);
    if (time_until_process_ms < time_until_packet_ms) {
      // Time to process.
      pacer_.Process();
    } else {
      // Time to send next packet to pacer.
      pacer_.SendPacket(PacedSender::kNormalPriority,
                        it->header().ssrc,
                        it->header().sequenceNumber,
                        (it->send_time_us() + 500) / 1000,
                        it->payload_size(),
                        false);
      pacer_queue_.push_back(*it);
      const size_t kMaxPacerQueueSize = 10000;
      if (pacer_queue_.size() > kMaxPacerQueueSize) {
        pacer_queue_.pop_front();
      }
      ++it;
    }
  }
  QueuePackets(in_out, end_time_ms * 1000);
}

void PacedVideoSender::QueuePackets(Packets* batch,
                                    int64_t end_of_batch_time_us) {
  queue_.merge(*batch);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if (it->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  batch->merge(to_transfer);
}

void PacedVideoSender::GiveFeedback(const PacketSender::Feedback& feedback) {
  source_->GiveFeedback(feedback);
  pacer_.UpdateBitrate(
      feedback.estimated_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * feedback.estimated_bps / 1000,
      0);
}

bool PacedVideoSender::TimeToSendPacket(uint32_t ssrc,
                                        uint16_t sequence_number,
                                        int64_t capture_time_ms,
                                        bool retransmission) {
  for (Packets::iterator it = pacer_queue_.begin(); it != pacer_queue_.end();
       ++it) {
    if (it->header().sequenceNumber == sequence_number) {
      int64_t pace_out_time_ms = clock_.TimeInMilliseconds();
      // Make sure a packet is never paced out earlier than when it was put into
      // the pacer.
      assert(pace_out_time_ms >= (it->send_time_us() + 500) / 1000);
      it->SetAbsSendTimeMs(pace_out_time_ms);
      it->set_send_time_us(1000 * pace_out_time_ms);
      queue_.push_back(*it);
      return true;
    }
  }
  return false;
}

int PacedVideoSender::TimeToSendPadding(int bytes) {
  return 0;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
