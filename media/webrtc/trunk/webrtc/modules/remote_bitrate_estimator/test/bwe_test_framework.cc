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
    : flow_id_(0), creation_time_us_(-1), send_time_us_(-1), payload_size_(0) {
}

Packet::Packet(int flow_id, int64_t send_time_us, size_t payload_size)
    : flow_id_(flow_id),
      creation_time_us_(send_time_us),
      send_time_us_(send_time_us),
      payload_size_(payload_size) {
}

Packet::~Packet() {
}

bool Packet::operator<(const Packet& rhs) const {
  return send_time_us_ < rhs.send_time_us_;
}

void Packet::set_send_time_us(int64_t send_time_us) {
  assert(send_time_us >= 0);
  send_time_us_ = send_time_us;
}

MediaPacket::MediaPacket() {
  memset(&header_, 0, sizeof(header_));
}

MediaPacket::MediaPacket(int flow_id,
                         int64_t send_time_us,
                         size_t payload_size,
                         const RTPHeader& header)
    : Packet(flow_id, send_time_us, payload_size), header_(header) {
}

MediaPacket::MediaPacket(int64_t send_time_us, uint32_t sequence_number)
    : Packet(0, send_time_us, 0) {
  memset(&header_, 0, sizeof(header_));
  header_.sequenceNumber = sequence_number;
}

void MediaPacket::SetAbsSendTimeMs(int64_t abs_send_time_ms) {
  header_.extension.hasAbsoluteSendTime = true;
  header_.extension.absoluteSendTime = ((static_cast<int64_t>(abs_send_time_ms *
    (1 << 18)) + 500) / 1000) & 0x00fffffful;
}

RembFeedback::RembFeedback(int flow_id,
                           int64_t send_time_us,
                           uint32_t estimated_bps,
                           RTCPReportBlock report_block)
    : FeedbackPacket(flow_id, send_time_us),
      estimated_bps_(estimated_bps),
      report_block_(report_block) {
}

SendSideBweFeedback::SendSideBweFeedback(
    int flow_id,
    int64_t send_time_us,
    const std::vector<PacketInfo>& packet_feedback_vector)
    : FeedbackPacket(flow_id, send_time_us),
      packet_feedback_vector_(packet_feedback_vector) {
}

bool IsTimeSorted(const Packets& packets) {
  PacketsConstIt last_it = packets.begin();
  for (PacketsConstIt it = last_it; it != packets.end(); ++it) {
    if (it != last_it && **it < **last_it) {
      return false;
    }
    last_it = it;
  }
  return true;
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 int flow_id,
                                 ProcessorType type)
    : listener_(listener) {
  flow_ids_.insert(flow_id);
  if (listener_) {
    listener_->AddPacketProcessor(this, type);
  }
}

PacketProcessor::PacketProcessor(PacketProcessorListener* listener,
                                 const FlowIds& flow_ids,
                                 ProcessorType type)
    : listener_(listener), flow_ids_(flow_ids) {
  if (listener_) {
    listener_->AddPacketProcessor(this, type);
  }
}

PacketProcessor::~PacketProcessor() {
  if (listener_) {
    listener_->RemovePacketProcessor(this);
  }
}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     int flow_id,
                                     const char* name)
    : PacketProcessor(listener, flow_id, kRegular),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {
}

RateCounterFilter::RateCounterFilter(PacketProcessorListener* listener,
                                     const FlowIds& flow_ids,
                                     const char* name)
    : PacketProcessor(listener, flow_ids, kRegular),
      rate_counter_(new RateCounter()),
      packets_per_second_stats_(),
      kbps_stats_(),
      name_(name) {
  std::stringstream ss;
  ss << name_ << "_";
  for (int flow_id : flow_ids) {
    ss << flow_id << ",";
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
  for (const Packet* packet : *in_out) {
    rate_counter_->UpdateRates(packet->send_time_us(), packet->payload_size());
  }
  packets_per_second_stats_.Push(rate_counter_->packets_per_second());
  kbps_stats_.Push(rate_counter_->bits_per_second() / 1000.0);
}

LossFilter::LossFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x12345678),
      loss_fraction_(0.0f) {
}

LossFilter::LossFilter(PacketProcessorListener* listener,
                       const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
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
      delete *it;
      it = in_out->erase(it);
    } else {
      ++it;
    }
  }
}

DelayFilter::DelayFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      delay_us_(0),
      last_send_time_us_(0) {
}

DelayFilter::DelayFilter(PacketProcessorListener* listener,
                         const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
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
  for (Packet* packet : *in_out) {
    int64_t new_send_time_us = packet->send_time_us() + delay_us_;
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    packet->set_send_time_us(last_send_time_us_);
  }
}

JitterFilter::JitterFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x89674523),
      stddev_jitter_us_(0),
      last_send_time_us_(0) {
}

JitterFilter::JitterFilter(PacketProcessorListener* listener,
                           const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
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
  for (Packet* packet : *in_out) {
    int64_t new_send_time_us = packet->send_time_us();
    new_send_time_us += random_.Gaussian(0, stddev_jitter_us_);
    last_send_time_us_ = std::max(last_send_time_us_, new_send_time_us);
    packet->set_send_time_us(last_send_time_us_);
  }
}

ReorderFilter::ReorderFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      random_(0x27452389),
      reorder_fraction_(0.0f) {
}

ReorderFilter::ReorderFilter(PacketProcessorListener* listener,
                             const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
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
        int64_t t1 = (*last_it)->send_time_us();
        int64_t t2 = (*it)->send_time_us();
        std::swap(*last_it, *it);
        (*last_it)->set_send_time_us(t1);
        (*it)->set_send_time_us(t2);
      }
      last_it = it;
    }
  }
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener, int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      kbps_(1200),
      last_send_time_us_(0),
      delay_cap_helper_(new DelayCapHelper()) {
}

ChokeFilter::ChokeFilter(PacketProcessorListener* listener,
                         const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
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
    int64_t earliest_send_time_us =
        last_send_time_us_ +
        ((*it)->payload_size() * 8 * 1000 + kbps_ / 2) / kbps_;
    int64_t new_send_time_us =
        std::max((*it)->send_time_us(), earliest_send_time_us);
    if (delay_cap_helper_->ShouldSendPacket(new_send_time_us,
                                            (*it)->send_time_us())) {
      (*it)->set_send_time_us(new_send_time_us);
      last_send_time_us_ = new_send_time_us;
      ++it;
    } else {
      delete *it;
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
    PacketProcessorListener* listener,
    int flow_id)
    : PacketProcessor(listener, flow_id, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(""),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    const FlowIds& flow_ids)
    : PacketProcessor(listener, flow_ids, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(""),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

TraceBasedDeliveryFilter::TraceBasedDeliveryFilter(
    PacketProcessorListener* listener,
    int flow_id,
    const char* name)
    : PacketProcessor(listener, flow_id, kRegular),
      current_offset_us_(0),
      delivery_times_us_(),
      next_delivery_it_(),
      local_time_us_(-1),
      rate_counter_(new RateCounter),
      name_(name),
      delay_cap_helper_(new DelayCapHelper()),
      packets_per_second_stats_(),
      kbps_stats_() {
}

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
    while (local_time_us_ < (*it)->send_time_us()) {
      ProceedToNextSlot();
    }
    // Drop any packets that have been queued for too long.
    while (!delay_cap_helper_->ShouldSendPacket(local_time_us_,
                                                (*it)->send_time_us())) {
      delete *it;
      it = in_out->erase(it);
      if (it == in_out->end()) {
        return;
      }
    }
    if (local_time_us_ >= (*it)->send_time_us()) {
      (*it)->set_send_time_us(local_time_us_);
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
      for (int64_t& delivery_time_us : delivery_times_us_) {
        delivery_time_us += local_time_us_ - current_offset_us_;
      }
      current_offset_us_ += local_time_us_ - current_offset_us_;
      next_delivery_it_ = delivery_times_us_.begin();
    }
  }
  local_time_us_ = *next_delivery_it_;
  const int kPayloadSize = 1200;
  rate_counter_->UpdateRates(local_time_us_, kPayloadSize);
}

VideoSource::VideoSource(int flow_id,
                         float fps,
                         uint32_t kbps,
                         uint32_t ssrc,
                         int64_t first_frame_offset_ms)
    : kMaxPayloadSizeBytes(1200),
      kTimestampBase(0xff80ff00ul),
      frame_period_ms_(1000.0 / fps),
      bits_per_second_(1000 * kbps),
      frame_size_bytes_(bits_per_second_ / 8 / fps),
      flow_id_(flow_id),
      next_frame_ms_(first_frame_offset_ms),
      now_ms_(0),
      prototype_header_() {
  memset(&prototype_header_, 0, sizeof(prototype_header_));
  prototype_header_.ssrc = ssrc;
  prototype_header_.sequenceNumber = 0xf000u;
}

uint32_t VideoSource::NextFrameSize() {
  return frame_size_bytes_;
}

uint32_t VideoSource::NextPacketSize(uint32_t frame_size,
                                     uint32_t remaining_payload) {
  return std::min(kMaxPayloadSizeBytes, remaining_payload);
}

void VideoSource::RunFor(int64_t time_ms, Packets* in_out) {
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
      MediaPacket* new_packet =
          new MediaPacket(flow_id_, send_time_us, size, prototype_header_);
      new_packets.push_back(new_packet);
      new_packet->SetAbsSendTimeMs(next_frame_ms_);
      payload_size -= size;
    }

    next_frame_ms_ += frame_period_ms_;
  }
  in_out->merge(new_packets, DereferencingComparator<Packet>);
}

AdaptiveVideoSource::AdaptiveVideoSource(int flow_id,
                                         float fps,
                                         uint32_t kbps,
                                         uint32_t ssrc,
                                         int64_t first_frame_offset_ms)
    : VideoSource(flow_id, fps, kbps, ssrc, first_frame_offset_ms) {
}

void AdaptiveVideoSource::SetBitrateBps(int bitrate_bps) {
  bits_per_second_ = std::min(bitrate_bps, 2500000);
  frame_size_bytes_ = (bits_per_second_ / 8 * frame_period_ms_ + 500) / 1000;
}

PeriodicKeyFrameSource::PeriodicKeyFrameSource(int flow_id,
                                               float fps,
                                               uint32_t kbps,
                                               uint32_t ssrc,
                                               int64_t first_frame_offset_ms,
                                               int key_frame_interval)
    : AdaptiveVideoSource(flow_id, fps, kbps, ssrc, first_frame_offset_ms),
      key_frame_interval_(key_frame_interval),
      frame_counter_(0),
      compensation_bytes_(0),
      compensation_per_frame_(0) {
}

uint32_t PeriodicKeyFrameSource::NextFrameSize() {
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

uint32_t PeriodicKeyFrameSource::NextPacketSize(uint32_t frame_size,
                                                uint32_t remaining_payload) {
  uint32_t fragments =
      (frame_size + (kMaxPayloadSizeBytes - 1)) / kMaxPayloadSizeBytes;
  uint32_t avg_size = (frame_size + fragments - 1) / fragments;
  return std::min(avg_size, remaining_payload);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
