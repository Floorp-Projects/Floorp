/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_

#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

#include <algorithm>

#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;

class StreamStatisticianImpl : public StreamStatistician {
 public:
  StreamStatisticianImpl(Clock* clock,
                         RtcpStatisticsCallback* rtcp_callback,
                         StreamDataCountersCallback* rtp_callback);
  virtual ~StreamStatisticianImpl() {}

  virtual bool GetStatistics(RtcpStatistics* statistics, bool reset) OVERRIDE;
  virtual void GetDataCounters(uint32_t* bytes_received,
                               uint32_t* packets_received) const OVERRIDE;
  virtual uint32_t BitrateReceived() const OVERRIDE;
  virtual void ResetStatistics() OVERRIDE;
  virtual bool IsRetransmitOfOldPacket(const RTPHeader& header,
                                       int min_rtt) const OVERRIDE;
  virtual bool IsPacketInOrder(uint16_t sequence_number) const OVERRIDE;

  void IncomingPacket(const RTPHeader& rtp_header,
                      size_t bytes,
                      bool retransmitted);
  void FecPacketReceived();
  void SetMaxReorderingThreshold(int max_reordering_threshold);
  void ProcessBitrate();
  virtual void LastReceiveTimeNtp(uint32_t* secs, uint32_t* frac) const;

 private:
  bool InOrderPacketInternal(uint16_t sequence_number) const;
  RtcpStatistics CalculateRtcpStatistics();
  void UpdateJitter(const RTPHeader& header,
                    uint32_t receive_time_secs,
                    uint32_t receive_time_frac);
  void UpdateCounters(const RTPHeader& rtp_header,
                      size_t bytes,
                      bool retransmitted);
  void NotifyRtpCallback() LOCKS_EXCLUDED(stream_lock_.get());
  void NotifyRtcpCallback() LOCKS_EXCLUDED(stream_lock_.get());

  Clock* clock_;
  scoped_ptr<CriticalSectionWrapper> stream_lock_;
  Bitrate incoming_bitrate_;
  uint32_t ssrc_;
  int max_reordering_threshold_;  // In number of packets or sequence numbers.

  // Stats on received RTP packets.
  uint32_t jitter_q4_;
  uint32_t cumulative_loss_;
  uint32_t jitter_q4_transmission_time_offset_;

  int64_t last_receive_time_ms_;
  uint32_t last_receive_time_secs_;
  uint32_t last_receive_time_frac_;
  uint32_t last_received_timestamp_;
  int32_t last_received_transmission_time_offset_;
  uint16_t received_seq_first_;
  uint16_t received_seq_max_;
  uint16_t received_seq_wraps_;

  // Current counter values.
  uint16_t received_packet_overhead_;
  StreamDataCounters receive_counters_;

  // Counter values when we sent the last report.
  uint32_t last_report_inorder_packets_;
  uint32_t last_report_old_packets_;
  uint16_t last_report_seq_max_;
  RtcpStatistics last_reported_statistics_;

  RtcpStatisticsCallback* const rtcp_callback_;
  StreamDataCountersCallback* const rtp_callback_;
};

class ReceiveStatisticsImpl : public ReceiveStatistics,
                              public RtcpStatisticsCallback,
                              public StreamDataCountersCallback {
 public:
  explicit ReceiveStatisticsImpl(Clock* clock);

  ~ReceiveStatisticsImpl();

  // Implement ReceiveStatistics.
  virtual void IncomingPacket(const RTPHeader& header,
                              size_t bytes,
                              bool retransmitted) OVERRIDE;
  virtual void FecPacketReceived(uint32_t ssrc) OVERRIDE;
  virtual StatisticianMap GetActiveStatisticians() const OVERRIDE;
  virtual StreamStatistician* GetStatistician(uint32_t ssrc) const OVERRIDE;
  virtual void SetMaxReorderingThreshold(int max_reordering_threshold) OVERRIDE;

  // Implement Module.
  virtual int32_t Process() OVERRIDE;
  virtual int32_t TimeUntilNextProcess() OVERRIDE;

  void ChangeSsrc(uint32_t from_ssrc, uint32_t to_ssrc);

  virtual void RegisterRtcpStatisticsCallback(RtcpStatisticsCallback* callback)
      OVERRIDE;

  virtual void RegisterRtpStatisticsCallback(
      StreamDataCountersCallback* callback) OVERRIDE;

 private:
  virtual void StatisticsUpdated(const RtcpStatistics& statistics,
                                 uint32_t ssrc) OVERRIDE;
  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) OVERRIDE;

  typedef std::map<uint32_t, StreamStatisticianImpl*> StatisticianImplMap;

  Clock* clock_;
  scoped_ptr<CriticalSectionWrapper> receive_statistics_lock_;
  int64_t last_rate_update_ms_;
  StatisticianImplMap statisticians_;

  RtcpStatisticsCallback* rtcp_stats_callback_;
  StreamDataCountersCallback* rtp_stats_callback_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVE_STATISTICS_IMPL_H_
