/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_nack_stats.h"
#include "modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/ntp_time.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {
class VideoBitrateAllocationObserver;
namespace rtcp {
class CommonHeader;
class ReportBlock;
class Rrtr;
class TargetBitrate;
class TmmbItem;
}  // namespace rtcp

class RTCPReceiver {
 public:
  class ModuleRtpRtcp {
   public:
    virtual void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) = 0;
    virtual void OnRequestSendReport() = 0;
    virtual void OnReceivedNack(
        const std::vector<uint16_t>& nack_sequence_numbers) = 0;
    virtual void OnReceivedRtcpReportBlocks(
        const ReportBlockList& report_blocks) = 0;

    virtual bool GetSendReportMetadata(const uint32_t send_report,
                                       uint64_t *time_of_send,
                                       uint32_t *packet_count,
                                       uint64_t *octet_count) = 0;

   protected:
    virtual ~ModuleRtpRtcp() = default;
  };

  RTCPReceiver(Clock* clock,
               bool receiver_only,
               RtcpPacketTypeCounterObserver* packet_type_counter_observer,
               RtcpBandwidthObserver* rtcp_bandwidth_observer,
               RtcpEventObserver* rtcp_event_observer,
               RtcpIntraFrameObserver* rtcp_intra_frame_observer,
               TransportFeedbackObserver* transport_feedback_observer,
               VideoBitrateAllocationObserver* bitrate_allocation_observer,
               ModuleRtpRtcp* owner);
  virtual ~RTCPReceiver();

  void IncomingPacket(const uint8_t* packet, size_t packet_size);

  int64_t LastReceivedReportBlockMs() const;

  void SetSsrcs(uint32_t main_ssrc, const std::set<uint32_t>& registered_ssrcs);
  void SetRemoteSSRC(uint32_t ssrc);
  uint32_t RemoteSSRC() const;

  // Get received cname.
  int32_t CNAME(uint32_t remote_ssrc, char cname[RTCP_CNAME_SIZE]) const;

  // Get received NTP.
  bool NTP(uint32_t* received_ntp_secs,
           uint32_t* received_ntp_frac,
           uint32_t* rtcp_arrival_time_secs,
           uint32_t* rtcp_arrival_time_frac,
           uint32_t* rtcp_timestamp) const;

  // Get received sender packet and octet counts
  void RemoteRTCPSenderInfo(uint32_t* packet_count,
                            uint32_t* octet_count,
                            NtpTime* ntp_timestamp) const;

  bool LastReceivedXrReferenceTimeInfo(rtcp::ReceiveTimeInfo* info) const;

  // Get rtt.
  int32_t RTT(uint32_t remote_ssrc,
              int64_t* last_rtt_ms,
              int64_t* avg_rtt_ms,
              int64_t* min_rtt_ms,
              int64_t* max_rtt_ms) const;

  void SetRtcpXrRrtrStatus(bool enable);
  bool GetAndResetXrRrRtt(int64_t* rtt_ms);

  // Get statistics.
  int32_t StatisticsReceived(std::vector<RTCPReportBlock>* receiveBlocks) const;

  // Returns true if we haven't received an RTCP RR for several RTCP
  // intervals, but only triggers true once.
  bool RtcpRrTimeout(int64_t rtcp_interval_ms);

  // Returns true if we haven't received an RTCP RR telling the receive side
  // has not received RTP packets for too long, i.e. extended highest sequence
  // number hasn't increased for several RTCP intervals. The function only
  // returns true once until a new RR is received.
  bool RtcpRrSequenceNumberTimeout(int64_t rtcp_interval_ms);

  std::vector<rtcp::TmmbItem> TmmbrReceived();
  // Return true if new bandwidth should be set.
  bool UpdateTmmbrTimers();
  std::vector<rtcp::TmmbItem> BoundingSet(bool* tmmbr_owner);
  // Set new bandwidth and notify remote clients about it.
  void NotifyTmmbrUpdated();

  void RegisterRtcpStatisticsCallback(RtcpStatisticsCallback* callback);
  RtcpStatisticsCallback* GetRtcpStatisticsCallback();

 private:
  struct PacketInformation;
  struct TmmbrInformation;
  struct ReportBlockWithRtt;
  struct LastFirStatus;
  // RTCP report blocks mapped by remote SSRC.
  using ReportBlockInfoMap = std::map<uint32_t, ReportBlockWithRtt>;
  // RTCP report blocks map mapped by source SSRC.
  using ReportBlockMap = std::map<uint32_t, ReportBlockInfoMap>;

  bool ParseCompoundPacket(const uint8_t* packet_begin,
                           const uint8_t* packet_end,
                           PacketInformation* packet_information);

  void TriggerCallbacksFromRtcpPacket(
      const PacketInformation& packet_information);

  TmmbrInformation* FindOrCreateTmmbrInfo(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);
  // Update TmmbrInformation (if present) is alive.
  void UpdateTmmbrRemoteIsAlive(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);
  TmmbrInformation* GetTmmbrInformation(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleSenderReport(const rtcp::CommonHeader& rtcp_block,
                          PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleReceiverReport(const rtcp::CommonHeader& rtcp_block,
                            PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleReportBlock(const rtcp::ReportBlock& report_block,
                         PacketInformation* packet_information,
                         uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleSdes(const rtcp::CommonHeader& rtcp_block,
                  PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXr(const rtcp::CommonHeader& rtcp_block,
                PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrReceiveReferenceTime(uint32_t sender_ssrc,
                                    const rtcp::Rrtr& rrtr)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrDlrrReportBlock(const rtcp::ReceiveTimeInfo& rti)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrTargetBitrate(uint32_t ssrc,
                             const rtcp::TargetBitrate& target_bitrate,
                             PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleNack(const rtcp::CommonHeader& rtcp_block,
                  PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleBye(const rtcp::CommonHeader& rtcp_block)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandlePli(const rtcp::CommonHeader& rtcp_block,
                 PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandlePsfbApp(const rtcp::CommonHeader& rtcp_block,
                     PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleTmmbr(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleTmmbn(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleSrReq(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleFir(const rtcp::CommonHeader& rtcp_block,
                 PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleTransportFeedback(const rtcp::CommonHeader& rtcp_block,
                               PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  Clock* const clock_;
  const bool receiver_only_;
  ModuleRtpRtcp* const rtp_rtcp_;

  rtc::CriticalSection feedbacks_lock_;
  RtcpBandwidthObserver* const rtcp_bandwidth_observer_;
  RtcpEventObserver* const rtcp_event_observer_;
  RtcpIntraFrameObserver* const rtcp_intra_frame_observer_;
  TransportFeedbackObserver* const transport_feedback_observer_;
  VideoBitrateAllocationObserver* const bitrate_allocation_observer_;

  rtc::CriticalSection rtcp_receiver_lock_;
  uint32_t main_ssrc_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  uint32_t remote_ssrc_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  std::set<uint32_t> registered_ssrcs_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  // Received sender report.
  NtpTime remote_sender_ntp_time_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  uint32_t remote_sender_rtp_time_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  uint32_t remote_sender_packet_count_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  uint32_t remote_sender_octet_count_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  // When did we receive the last send report.
  NtpTime last_received_sr_ntp_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  // Received XR receive time report.
  rtcp::ReceiveTimeInfo remote_time_info_;
  // Time when the report was received.
  NtpTime last_received_xr_ntp_;
  // Estimated rtt, zero when there is no valid estimate.
  bool xr_rrtr_status_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  int64_t xr_rr_rtt_ms_;

  int64_t oldest_tmmbr_info_ms_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  // Mapped by remote ssrc.
  std::map<uint32_t, TmmbrInformation> tmmbr_infos_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  ReportBlockMap received_report_blocks_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  std::map<uint32_t, LastFirStatus> last_fir_
      RTC_GUARDED_BY(rtcp_receiver_lock_);
  std::map<uint32_t, std::string> received_cnames_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  // The last time we received an RTCP Report block for this module.
  int64_t last_received_rb_ms_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  // The time we last received an RTCP RR telling we have successfully
  // delivered RTP packet to the remote side.
  int64_t last_increased_sequence_number_ms_;

  RtcpStatisticsCallback* stats_callback_ RTC_GUARDED_BY(feedbacks_lock_);

  RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
  RtcpPacketTypeCounter packet_type_counter_;

  RtcpNackStats nack_stats_;

  size_t num_skipped_packets_;
  int64_t last_skipped_packets_warning_ms_;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_
