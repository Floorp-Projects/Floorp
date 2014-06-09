/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/receive_statistics_impl.h"

#include <math.h>

#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

const int64_t kStatisticsTimeoutMs = 8000;
const int kStatisticsProcessIntervalMs = 1000;

StreamStatistician::~StreamStatistician() {}

StreamStatisticianImpl::StreamStatisticianImpl(
    Clock* clock,
    RtcpStatisticsCallback* rtcp_callback,
    StreamDataCountersCallback* rtp_callback)
    : clock_(clock),
      stream_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      incoming_bitrate_(clock, NULL),
      ssrc_(0),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold),
      jitter_q4_(0),
      cumulative_loss_(0),
      jitter_q4_transmission_time_offset_(0),
      last_receive_time_ms_(0),
      last_receive_time_secs_(0),
      last_receive_time_frac_(0),
      last_received_timestamp_(0),
      last_received_transmission_time_offset_(0),
      received_seq_first_(0),
      received_seq_max_(0),
      received_seq_wraps_(0),
      received_packet_overhead_(12),
      last_report_inorder_packets_(0),
      last_report_old_packets_(0),
      last_report_seq_max_(0),
      rtcp_callback_(rtcp_callback),
      rtp_callback_(rtp_callback) {}

void StreamStatisticianImpl::ResetStatistics() {
  CriticalSectionScoped cs(stream_lock_.get());
  last_report_inorder_packets_ = 0;
  last_report_old_packets_ = 0;
  last_report_seq_max_ = 0;
  last_reported_statistics_ = RtcpStatistics();
  jitter_q4_ = 0;
  cumulative_loss_ = 0;
  jitter_q4_transmission_time_offset_ = 0;
  received_seq_wraps_ = 0;
  received_seq_max_ = 0;
  received_seq_first_ = 0;
  receive_counters_ = StreamDataCounters();
}

void StreamStatisticianImpl::IncomingPacket(const RTPHeader& header,
                                            size_t bytes,
                                            bool retransmitted) {
  UpdateCounters(header, bytes, retransmitted);
  NotifyRtpCallback();
}

void StreamStatisticianImpl::UpdateCounters(const RTPHeader& header,
                                            size_t bytes,
                                            bool retransmitted) {
  CriticalSectionScoped cs(stream_lock_.get());
  bool in_order = InOrderPacketInternal(header.sequenceNumber);
  ssrc_ = header.ssrc;
  incoming_bitrate_.Update(bytes);
  receive_counters_.bytes +=
      bytes - (header.paddingLength + header.headerLength);
  receive_counters_.header_bytes += header.headerLength;
  receive_counters_.padding_bytes += header.paddingLength;
  ++receive_counters_.packets;
  if (!in_order && retransmitted) {
    ++receive_counters_.retransmitted_packets;
  }

  if (receive_counters_.packets == 1) {
    received_seq_first_ = header.sequenceNumber;
  }

  // Count only the new packets received. That is, if packets 1, 2, 3, 5, 4, 6
  // are received, 4 will be ignored.
  if (in_order) {
    // Current time in samples.
    uint32_t receive_time_secs;
    uint32_t receive_time_frac;
    clock_->CurrentNtp(receive_time_secs, receive_time_frac);

    // Wrong if we use RetransmitOfOldPacket.
    if (receive_counters_.packets > 1 &&
        received_seq_max_ > header.sequenceNumber) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // New max.
    received_seq_max_ = header.sequenceNumber;

    // If new time stamp and more than one in-order packet received, calculate
    // new jitter statistics.
    if (header.timestamp != last_received_timestamp_ &&
        (receive_counters_.packets - receive_counters_.retransmitted_packets) >
            1) {
      UpdateJitter(header, receive_time_secs, receive_time_frac);
    }
    last_received_timestamp_ = header.timestamp;
    last_receive_time_secs_ = receive_time_secs;
    last_receive_time_frac_ = receive_time_frac;
    last_receive_time_ms_ = clock_->TimeInMilliseconds();
  }

  uint16_t packet_oh = header.headerLength + header.paddingLength;

  // Our measured overhead. Filter from RFC 5104 4.2.1.2:
  // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
  received_packet_overhead_ = (15 * received_packet_overhead_ + packet_oh) >> 4;
}

void StreamStatisticianImpl::UpdateJitter(const RTPHeader& header,
                                          uint32_t receive_time_secs,
                                          uint32_t receive_time_frac) {
  uint32_t receive_time_rtp = ModuleRTPUtility::ConvertNTPTimeToRTP(
          receive_time_secs, receive_time_frac, header.payload_type_frequency);
  uint32_t last_receive_time_rtp = ModuleRTPUtility::ConvertNTPTimeToRTP(
      last_receive_time_secs_, last_receive_time_frac_,
      header.payload_type_frequency);
  int32_t time_diff_samples = (receive_time_rtp - last_receive_time_rtp) -
      (header.timestamp - last_received_timestamp_);

  time_diff_samples = abs(time_diff_samples);

  // lib_jingle sometimes deliver crazy jumps in TS for the same stream.
  // If this happens, don't update jitter value. Use 5 secs video frequency
  // as the threshold.
  if (time_diff_samples < 450000) {
    // Note we calculate in Q4 to avoid using float.
    int32_t jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
    jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
  }

  // Extended jitter report, RFC 5450.
  // Actual network jitter, excluding the source-introduced jitter.
  int32_t time_diff_samples_ext =
    (receive_time_rtp - last_receive_time_rtp) -
    ((header.timestamp +
      header.extension.transmissionTimeOffset) -
     (last_received_timestamp_ +
      last_received_transmission_time_offset_));

  time_diff_samples_ext = abs(time_diff_samples_ext);

  if (time_diff_samples_ext < 450000) {
    int32_t jitter_diffQ4TransmissionTimeOffset =
      (time_diff_samples_ext << 4) - jitter_q4_transmission_time_offset_;
    jitter_q4_transmission_time_offset_ +=
      ((jitter_diffQ4TransmissionTimeOffset + 8) >> 4);
  }
}

void StreamStatisticianImpl::NotifyRtpCallback() {
  StreamDataCounters data;
  uint32_t ssrc;
  {
    CriticalSectionScoped cs(stream_lock_.get());
    data = receive_counters_;
    ssrc = ssrc_;
  }
  rtp_callback_->DataCountersUpdated(data, ssrc);
}

void StreamStatisticianImpl::NotifyRtcpCallback() {
  RtcpStatistics data;
  uint32_t ssrc;
  {
    CriticalSectionScoped cs(stream_lock_.get());
    data = last_reported_statistics_;
    ssrc = ssrc_;
  }
  rtcp_callback_->StatisticsUpdated(data, ssrc);
}

void StreamStatisticianImpl::FecPacketReceived() {
  {
    CriticalSectionScoped cs(stream_lock_.get());
    ++receive_counters_.fec_packets;
  }
  NotifyRtpCallback();
}

void StreamStatisticianImpl::SetMaxReorderingThreshold(
    int max_reordering_threshold) {
  CriticalSectionScoped cs(stream_lock_.get());
  max_reordering_threshold_ = max_reordering_threshold;
}

bool StreamStatisticianImpl::GetStatistics(RtcpStatistics* statistics,
                                           bool reset) {
  {
    CriticalSectionScoped cs(stream_lock_.get());
    if (received_seq_first_ == 0 && receive_counters_.bytes == 0) {
      // We have not received anything.
      return false;
    }

    if (!reset) {
      if (last_report_inorder_packets_ == 0) {
        // No report.
        return false;
      }
      // Just get last report.
      *statistics = last_reported_statistics_;
      return true;
    }

    *statistics = CalculateRtcpStatistics();
  }

  NotifyRtcpCallback();

  return true;
}

RtcpStatistics StreamStatisticianImpl::CalculateRtcpStatistics() {
  RtcpStatistics stats;

  if (last_report_inorder_packets_ == 0) {
    // First time we send a report.
    last_report_seq_max_ = received_seq_first_ - 1;
  }

  // Calculate fraction lost.
  uint16_t exp_since_last = (received_seq_max_ - last_report_seq_max_);

  if (last_report_seq_max_ > received_seq_max_) {
    // Can we assume that the seq_num can't go decrease over a full RTCP period?
    exp_since_last = 0;
  }

  // Number of received RTP packets since last report, counts all packets but
  // not re-transmissions.
  uint32_t rec_since_last =
      (receive_counters_.packets - receive_counters_.retransmitted_packets) -
      last_report_inorder_packets_;

  // With NACK we don't know the expected retransmissions during the last
  // second. We know how many "old" packets we have received. We just count
  // the number of old received to estimate the loss, but it still does not
  // guarantee an exact number since we run this based on time triggered by
  // sending of an RTP packet. This should have a minimum effect.

  // With NACK we don't count old packets as received since they are
  // re-transmitted. We use RTT to decide if a packet is re-ordered or
  // re-transmitted.
  uint32_t retransmitted_packets =
      receive_counters_.retransmitted_packets - last_report_old_packets_;
  rec_since_last += retransmitted_packets;

  int32_t missing = 0;
  if (exp_since_last > rec_since_last) {
    missing = (exp_since_last - rec_since_last);
  }
  uint8_t local_fraction_lost = 0;
  if (exp_since_last) {
    // Scale 0 to 255, where 255 is 100% loss.
    local_fraction_lost =
        static_cast<uint8_t>(255 * missing / exp_since_last);
  }
  stats.fraction_lost = local_fraction_lost;

  // We need a counter for cumulative loss too.
  cumulative_loss_ += missing;
  stats.cumulative_lost = cumulative_loss_;
  stats.extended_max_sequence_number =
      (received_seq_wraps_ << 16) + received_seq_max_;
  // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
  stats.jitter = jitter_q4_ >> 4;

  // Store this report.
  last_reported_statistics_ = stats;

  // Only for report blocks in RTCP SR and RR.
  last_report_inorder_packets_ =
      receive_counters_.packets - receive_counters_.retransmitted_packets;
  last_report_old_packets_ = receive_counters_.retransmitted_packets;
  last_report_seq_max_ = received_seq_max_;

  return stats;
}

void StreamStatisticianImpl::GetDataCounters(
    uint32_t* bytes_received, uint32_t* packets_received) const {
  CriticalSectionScoped cs(stream_lock_.get());
  if (bytes_received) {
    *bytes_received = receive_counters_.bytes + receive_counters_.header_bytes +
                      receive_counters_.padding_bytes;
  }
  if (packets_received) {
    *packets_received = receive_counters_.packets;
  }
}

uint32_t StreamStatisticianImpl::BitrateReceived() const {
  CriticalSectionScoped cs(stream_lock_.get());
  return incoming_bitrate_.BitrateNow();
}

void StreamStatisticianImpl::ProcessBitrate() {
  CriticalSectionScoped cs(stream_lock_.get());
  incoming_bitrate_.Process();
}

void StreamStatisticianImpl::LastReceiveTimeNtp(uint32_t* secs,
                                                uint32_t* frac) const {
  CriticalSectionScoped cs(stream_lock_.get());
  *secs = last_receive_time_secs_;
  *frac = last_receive_time_frac_;
}

bool StreamStatisticianImpl::IsRetransmitOfOldPacket(
    const RTPHeader& header, int min_rtt) const {
  CriticalSectionScoped cs(stream_lock_.get());
  if (InOrderPacketInternal(header.sequenceNumber)) {
    return false;
  }
  uint32_t frequency_khz = header.payload_type_frequency / 1000;
  assert(frequency_khz > 0);

  int64_t time_diff_ms = clock_->TimeInMilliseconds() -
      last_receive_time_ms_;

  // Diff in time stamp since last received in order.
  uint32_t timestamp_diff = header.timestamp - last_received_timestamp_;
  int32_t rtp_time_stamp_diff_ms = static_cast<int32_t>(timestamp_diff) /
      frequency_khz;

  int32_t max_delay_ms = 0;
  if (min_rtt == 0) {
    // Jitter standard deviation in samples.
    float jitter_std = sqrt(static_cast<float>(jitter_q4_ >> 4));

    // 2 times the standard deviation => 95% confidence.
    // And transform to milliseconds by dividing by the frequency in kHz.
    max_delay_ms = static_cast<int32_t>((2 * jitter_std) / frequency_khz);

    // Min max_delay_ms is 1.
    if (max_delay_ms == 0) {
      max_delay_ms = 1;
    }
  } else {
    max_delay_ms = (min_rtt / 3) + 1;
  }
  return time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms;
}

bool StreamStatisticianImpl::IsPacketInOrder(uint16_t sequence_number) const {
  CriticalSectionScoped cs(stream_lock_.get());
  return InOrderPacketInternal(sequence_number);
}

bool StreamStatisticianImpl::InOrderPacketInternal(
    uint16_t sequence_number) const {
  // First packet is always in order.
  if (last_receive_time_ms_ == 0)
    return true;

  if (IsNewerSequenceNumber(sequence_number, received_seq_max_)) {
    return true;
  } else {
    // If we have a restart of the remote side this packet is still in order.
    return !IsNewerSequenceNumber(sequence_number, received_seq_max_ -
                                  max_reordering_threshold_);
  }
}

ReceiveStatistics* ReceiveStatistics::Create(Clock* clock) {
  return new ReceiveStatisticsImpl(clock);
}

ReceiveStatisticsImpl::ReceiveStatisticsImpl(Clock* clock)
    : clock_(clock),
      receive_statistics_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      last_rate_update_ms_(0),
      rtcp_stats_callback_(NULL),
      rtp_stats_callback_(NULL) {}

ReceiveStatisticsImpl::~ReceiveStatisticsImpl() {
  while (!statisticians_.empty()) {
    delete statisticians_.begin()->second;
    statisticians_.erase(statisticians_.begin());
  }
}

void ReceiveStatisticsImpl::IncomingPacket(const RTPHeader& header,
                                           size_t bytes,
                                           bool retransmitted) {
  StatisticianImplMap::iterator it;
  {
    CriticalSectionScoped cs(receive_statistics_lock_.get());
    it = statisticians_.find(header.ssrc);
    if (it == statisticians_.end()) {
      std::pair<StatisticianImplMap::iterator, uint32_t> insert_result =
          statisticians_.insert(std::make_pair(
              header.ssrc, new StreamStatisticianImpl(clock_, this, this)));
      it = insert_result.first;
    }
  }
  it->second->IncomingPacket(header, bytes, retransmitted);
}

void ReceiveStatisticsImpl::FecPacketReceived(uint32_t ssrc) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  StatisticianImplMap::iterator it = statisticians_.find(ssrc);
  assert(it != statisticians_.end());
  it->second->FecPacketReceived();
}

void ReceiveStatisticsImpl::ChangeSsrc(uint32_t from_ssrc, uint32_t to_ssrc) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  StatisticianImplMap::iterator from_it = statisticians_.find(from_ssrc);
  if (from_it == statisticians_.end())
    return;
  if (statisticians_.find(to_ssrc) != statisticians_.end())
    return;
  statisticians_[to_ssrc] = from_it->second;
  statisticians_.erase(from_it);
}

StatisticianMap ReceiveStatisticsImpl::GetActiveStatisticians() const {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  StatisticianMap active_statisticians;
  for (StatisticianImplMap::const_iterator it = statisticians_.begin();
       it != statisticians_.end(); ++it) {
    uint32_t secs;
    uint32_t frac;
    it->second->LastReceiveTimeNtp(&secs, &frac);
    if (clock_->CurrentNtpInMilliseconds() -
        Clock::NtpToMs(secs, frac) < kStatisticsTimeoutMs) {
      active_statisticians[it->first] = it->second;
    }
  }
  return active_statisticians;
}

StreamStatistician* ReceiveStatisticsImpl::GetStatistician(
    uint32_t ssrc) const {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  StatisticianImplMap::const_iterator it = statisticians_.find(ssrc);
  if (it == statisticians_.end())
    return NULL;
  return it->second;
}

void ReceiveStatisticsImpl::SetMaxReorderingThreshold(
    int max_reordering_threshold) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  for (StatisticianImplMap::iterator it = statisticians_.begin();
       it != statisticians_.end(); ++it) {
    it->second->SetMaxReorderingThreshold(max_reordering_threshold);
  }
}

int32_t ReceiveStatisticsImpl::Process() {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  for (StatisticianImplMap::iterator it = statisticians_.begin();
       it != statisticians_.end(); ++it) {
    it->second->ProcessBitrate();
  }
  last_rate_update_ms_ = clock_->TimeInMilliseconds();
  return 0;
}

int32_t ReceiveStatisticsImpl::TimeUntilNextProcess() {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  int time_since_last_update = clock_->TimeInMilliseconds() -
      last_rate_update_ms_;
  return std::max(kStatisticsProcessIntervalMs - time_since_last_update, 0);
}

void ReceiveStatisticsImpl::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  if (callback != NULL)
    assert(rtcp_stats_callback_ == NULL);
  rtcp_stats_callback_ = callback;
}

void ReceiveStatisticsImpl::StatisticsUpdated(const RtcpStatistics& statistics,
                                              uint32_t ssrc) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  if (rtcp_stats_callback_) {
    rtcp_stats_callback_->StatisticsUpdated(statistics, ssrc);
  }
}

void ReceiveStatisticsImpl::RegisterRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  if (callback != NULL)
    assert(rtp_stats_callback_ == NULL);
  rtp_stats_callback_ = callback;
}

void ReceiveStatisticsImpl::DataCountersUpdated(const StreamDataCounters& stats,
                                                uint32_t ssrc) {
  CriticalSectionScoped cs(receive_statistics_lock_.get());
  if (rtp_stats_callback_) {
    rtp_stats_callback_->DataCountersUpdated(stats, ssrc);
  }
}

void NullReceiveStatistics::IncomingPacket(const RTPHeader& rtp_header,
                                           size_t bytes,
                                           bool retransmitted) {}

void NullReceiveStatistics::FecPacketReceived(uint32_t ssrc) {}

StatisticianMap NullReceiveStatistics::GetActiveStatisticians() const {
  return StatisticianMap();
}

StreamStatistician* NullReceiveStatistics::GetStatistician(
    uint32_t ssrc) const {
  return NULL;
}

void NullReceiveStatistics::SetMaxReorderingThreshold(
    int max_reordering_threshold) {}

int32_t NullReceiveStatistics::TimeUntilNextProcess() { return 0; }

int32_t NullReceiveStatistics::Process() { return 0; }

void NullReceiveStatistics::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {}

void NullReceiveStatistics::RegisterRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {}

}  // namespace webrtc
