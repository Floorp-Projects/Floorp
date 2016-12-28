/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"

#include <string.h>

#include <set>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/include/trace.h"

#ifdef _WIN32
// Disable warning C4355: 'this' : used in base member initializer list.
#pragma warning(disable : 4355)
#endif

namespace webrtc {

RtpRtcp::Configuration::Configuration()
    : audio(false),
      receiver_only(false),
      clock(nullptr),
      receive_statistics(NullObjectReceiveStatistics()),
      outgoing_transport(nullptr),
      intra_frame_callback(nullptr),
      bandwidth_callback(nullptr),
      transport_feedback_callback(nullptr),
      rtt_stats(nullptr),
      rtcp_packet_type_counter_observer(nullptr),
      audio_messages(NullObjectRtpAudioFeedback()),
      remote_bitrate_estimator(nullptr),
      paced_sender(nullptr),
      transport_sequence_number_allocator(nullptr),
      send_bitrate_observer(nullptr),
      send_frame_count_observer(nullptr),
      send_side_delay_observer(nullptr) {}

RtpRtcp* RtpRtcp::CreateRtpRtcp(const RtpRtcp::Configuration& configuration) {
  if (configuration.clock) {
    return new ModuleRtpRtcpImpl(configuration);
  } else {
    // No clock implementation provided, use default clock.
    RtpRtcp::Configuration configuration_copy;
    memcpy(&configuration_copy, &configuration,
           sizeof(RtpRtcp::Configuration));
    configuration_copy.clock = Clock::GetRealTimeClock();
    return new ModuleRtpRtcpImpl(configuration_copy);
  }
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(const Configuration& configuration)
    : rtp_sender_(configuration.audio,
                  configuration.clock,
                  configuration.outgoing_transport,
                  configuration.audio_messages,
                  configuration.paced_sender,
                  configuration.transport_sequence_number_allocator,
                  configuration.transport_feedback_callback,
                  configuration.send_bitrate_observer,
                  configuration.send_frame_count_observer,
                  configuration.send_side_delay_observer),
      rtcp_sender_(configuration.audio,
                   configuration.clock,
                   configuration.receive_statistics,
                   configuration.rtcp_packet_type_counter_observer,
                   configuration.outgoing_transport),
      rtcp_receiver_(configuration.clock,
                     configuration.receiver_only,
                     configuration.rtcp_packet_type_counter_observer,
                     configuration.bandwidth_callback,
                     configuration.intra_frame_callback,
                     configuration.transport_feedback_callback,
                     this),
      clock_(configuration.clock),
      audio_(configuration.audio),
      collision_detected_(false),
      last_process_time_(configuration.clock->TimeInMilliseconds()),
      last_bitrate_process_time_(configuration.clock->TimeInMilliseconds()),
      last_rtt_process_time_(configuration.clock->TimeInMilliseconds()),
      packet_overhead_(28),                     // IPV4 UDP.
      padding_index_(static_cast<size_t>(-1)),  // Start padding at first child.
      nack_method_(kNackOff),
      nack_last_time_sent_full_(0),
      nack_last_time_sent_full_prev_(0),
      nack_last_seq_number_sent_(0),
      key_frame_req_method_(kKeyFrameReqPliRtcp),
      remote_bitrate_(configuration.remote_bitrate_estimator),
      rtt_stats_(configuration.rtt_stats),
      critical_section_rtt_(CriticalSectionWrapper::CreateCriticalSection()),
      rtt_ms_(0) {
  send_video_codec_.codecType = kVideoCodecUnknown;

  // Make sure that RTCP objects are aware of our SSRC.
  uint32_t SSRC = rtp_sender_.SSRC();
  rtcp_sender_.SetSSRC(SSRC);
  SetRtcpReceiverSsrcs(SSRC);
}

// Returns the number of milliseconds until the module want a worker thread
// to call Process.
int64_t ModuleRtpRtcpImpl::TimeUntilNextProcess() {
  const int64_t now = clock_->TimeInMilliseconds();
  const int64_t kRtpRtcpMaxIdleTimeProcessMs = 5;
  return kRtpRtcpMaxIdleTimeProcessMs - (now - last_process_time_);
}

// Process any pending tasks such as timeouts (non time critical events).
int32_t ModuleRtpRtcpImpl::Process() {
  const int64_t now = clock_->TimeInMilliseconds();
  last_process_time_ = now;

  const int64_t kRtpRtcpBitrateProcessTimeMs = 10;
  if (now >= last_bitrate_process_time_ + kRtpRtcpBitrateProcessTimeMs) {
    rtp_sender_.ProcessBitrate();
    last_bitrate_process_time_ = now;
  }

  const int64_t kRtpRtcpRttProcessTimeMs = 1000;
  bool process_rtt = now >= last_rtt_process_time_ + kRtpRtcpRttProcessTimeMs;
  if (rtcp_sender_.Sending()) {
    // Process RTT if we have received a receiver report and we haven't
    // processed RTT for at least |kRtpRtcpRttProcessTimeMs| milliseconds.
    if (rtcp_receiver_.LastReceivedReceiverReport() >
        last_rtt_process_time_ && process_rtt) {
      std::vector<RTCPReportBlock> receive_blocks;
      rtcp_receiver_.StatisticsReceived(&receive_blocks);
      int64_t max_rtt = 0;
      for (std::vector<RTCPReportBlock>::iterator it = receive_blocks.begin();
           it != receive_blocks.end(); ++it) {
        int64_t rtt = 0;
        rtcp_receiver_.RTT(it->remoteSSRC, &rtt, NULL, NULL, NULL);
        max_rtt = (rtt > max_rtt) ? rtt : max_rtt;
      }
      // Report the rtt.
      if (rtt_stats_ && max_rtt != 0)
        rtt_stats_->OnRttUpdate(max_rtt);
    }

    // Verify receiver reports are delivered and the reported sequence number
    // is increasing.
    int64_t rtcp_interval = RtcpReportInterval();
    if (rtcp_receiver_.RtcpRrTimeout(rtcp_interval)) {
      LOG_F(LS_WARNING) << "Timeout: No RTCP RR received.";
    } else if (rtcp_receiver_.RtcpRrSequenceNumberTimeout(rtcp_interval)) {
      LOG_F(LS_WARNING) <<
          "Timeout: No increase in RTCP RR extended highest sequence number.";
    }

    if (remote_bitrate_ && rtcp_sender_.TMMBR()) {
      unsigned int target_bitrate = 0;
      std::vector<unsigned int> ssrcs;
      if (remote_bitrate_->LatestEstimate(&ssrcs, &target_bitrate)) {
        if (!ssrcs.empty()) {
          target_bitrate = target_bitrate / ssrcs.size();
        }
        rtcp_sender_.SetTargetBitrate(target_bitrate);
      }
    }
  } else {
    // Report rtt from receiver.
    if (process_rtt) {
       int64_t rtt_ms;
       if (rtt_stats_ && rtcp_receiver_.GetAndResetXrRrRtt(&rtt_ms)) {
         rtt_stats_->OnRttUpdate(rtt_ms);
       }
    }
  }

  // Get processed rtt.
  if (process_rtt) {
    last_rtt_process_time_ = now;
    if (rtt_stats_)
      set_rtt_ms(rtt_stats_->LastProcessedRtt());
  }

  // For sending streams, make sure to not send a SR before media has been sent.
  if (rtcp_sender_.TimeToSendRTCPReport()) {
    RTCPSender::FeedbackState state = GetFeedbackState();
    // Prevent sending streams to send SR before any media has been sent.
    if (!rtcp_sender_.Sending() || state.packets_sent > 0)
      rtcp_sender_.SendRTCP(state, kRtcpReport);
  }

  if (UpdateRTCPReceiveInformationTimers()) {
    // A receiver has timed out
    rtcp_receiver_.UpdateTMMBR();
  }
  return 0;
}

void ModuleRtpRtcpImpl::SetRtxSendStatus(int mode) {
  rtp_sender_.SetRtxStatus(mode);
}

int ModuleRtpRtcpImpl::RtxSendStatus() const {
  return rtp_sender_.RtxStatus();
}

void ModuleRtpRtcpImpl::SetRtxSsrc(uint32_t ssrc) {
  rtp_sender_.SetRtxSsrc(ssrc);
}

void ModuleRtpRtcpImpl::SetRtxSendPayloadType(int payload_type,
                                              int associated_payload_type) {
  rtp_sender_.SetRtxPayloadType(payload_type, associated_payload_type);
}

std::pair<int, int> ModuleRtpRtcpImpl::RtxSendPayloadType() const {
  return rtp_sender_.RtxPayloadType();
}

int32_t ModuleRtpRtcpImpl::IncomingRtcpPacket(
    const uint8_t* rtcp_packet,
    const size_t length) {
  // Allow receive of non-compound RTCP packets.
  RTCPUtility::RTCPParserV2 rtcp_parser(rtcp_packet, length, true);

  const bool valid_rtcpheader = rtcp_parser.IsValid();
  if (!valid_rtcpheader) {
    LOG(LS_WARNING) << "Incoming invalid RTCP packet";
    return -1;
  }
  RTCPHelp::RTCPPacketInformation rtcp_packet_information;
  int32_t ret_val = rtcp_receiver_.IncomingRTCPPacket(
      rtcp_packet_information, &rtcp_parser);
  if (ret_val == 0) {
    rtcp_receiver_.TriggerCallbacksFromRTCPPacket(rtcp_packet_information);
  }
  return ret_val;
}

int32_t ModuleRtpRtcpImpl::RegisterSendPayload(
    const CodecInst& voice_codec) {
  return rtp_sender_.RegisterPayload(
           voice_codec.plname,
           voice_codec.pltype,
           voice_codec.plfreq,
           voice_codec.channels,
           (voice_codec.rate < 0) ? 0 : voice_codec.rate);
}

int32_t ModuleRtpRtcpImpl::RegisterSendPayload(const VideoCodec& video_codec) {
  send_video_codec_ = video_codec;
  return rtp_sender_.RegisterPayload(video_codec.plName,
                                     video_codec.plType,
                                     90000,
                                     0,
                                     video_codec.maxBitrate);
}

int32_t ModuleRtpRtcpImpl::DeRegisterSendPayload(const int8_t payload_type) {
  return rtp_sender_.DeRegisterSendPayload(payload_type);
}

int8_t ModuleRtpRtcpImpl::SendPayloadType() const {
  return rtp_sender_.SendPayloadType();
}

uint32_t ModuleRtpRtcpImpl::StartTimestamp() const {
  return rtp_sender_.StartTimestamp();
}

// Configure start timestamp, default is a random number.
void ModuleRtpRtcpImpl::SetStartTimestamp(const uint32_t timestamp) {
  rtcp_sender_.SetStartTimestamp(timestamp);
  rtp_sender_.SetStartTimestamp(timestamp, true);
}

uint16_t ModuleRtpRtcpImpl::SequenceNumber() const {
  return rtp_sender_.SequenceNumber();
}

// Set SequenceNumber, default is a random number.
void ModuleRtpRtcpImpl::SetSequenceNumber(const uint16_t seq_num) {
  rtp_sender_.SetSequenceNumber(seq_num);
}

bool ModuleRtpRtcpImpl::SetRtpStateForSsrc(uint32_t ssrc,
                                           const RtpState& rtp_state) {
  if (rtp_sender_.SSRC() == ssrc) {
    rtp_sender_.SetRtpState(rtp_state);
    return true;
  }
  if (rtp_sender_.RtxSsrc() == ssrc) {
    rtp_sender_.SetRtxRtpState(rtp_state);
    return true;
  }
  return false;
}

bool ModuleRtpRtcpImpl::GetRtpStateForSsrc(uint32_t ssrc, RtpState* rtp_state) {
  if (rtp_sender_.SSRC() == ssrc) {
    *rtp_state = rtp_sender_.GetRtpState();
    return true;
  }
  if (rtp_sender_.RtxSsrc() == ssrc) {
    *rtp_state = rtp_sender_.GetRtxRtpState();
    return true;
  }
  return false;
}

uint32_t ModuleRtpRtcpImpl::SSRC() const {
  return rtp_sender_.SSRC();
}

// Configure SSRC, default is a random number.
void ModuleRtpRtcpImpl::SetSSRC(const uint32_t ssrc) {
  rtp_sender_.SetSSRC(ssrc);
  rtcp_sender_.SetSSRC(ssrc);
  SetRtcpReceiverSsrcs(ssrc);
}

void ModuleRtpRtcpImpl::SetCsrcs(const std::vector<uint32_t>& csrcs) {
  rtcp_sender_.SetCsrcs(csrcs);
  rtp_sender_.SetCsrcs(csrcs);
}

// TODO(pbos): Handle media and RTX streams separately (separate RTCP
// feedbacks).
RTCPSender::FeedbackState ModuleRtpRtcpImpl::GetFeedbackState() {
  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_.GetDataCounters(&rtp_stats, &rtx_stats);

  RTCPSender::FeedbackState state;
  state.send_payload_type = SendPayloadType();
  state.frequency_hz = CurrentSendFrequencyHz();
  state.packets_sent = rtp_stats.transmitted.packets +
                       rtx_stats.transmitted.packets;
  state.media_bytes_sent = rtp_stats.transmitted.payload_bytes +
                           rtx_stats.transmitted.payload_bytes;
  state.module = this;

  LastReceivedNTP(&state.last_rr_ntp_secs,
                  &state.last_rr_ntp_frac,
                  &state.remote_sr);

  state.has_last_xr_rr = LastReceivedXrReferenceTimeInfo(&state.last_xr_rr);

  uint32_t tmp;
  BitrateSent(&state.send_bitrate, &tmp, &tmp, &tmp);
  return state;
}

int ModuleRtpRtcpImpl::CurrentSendFrequencyHz() const {
  return rtp_sender_.SendPayloadFrequency();
}

int32_t ModuleRtpRtcpImpl::SetSendingStatus(const bool sending) {
  if (rtcp_sender_.Sending() != sending) {
    // Sends RTCP BYE when going from true to false
    if (rtcp_sender_.SetSendingStatus(GetFeedbackState(), sending) != 0) {
      LOG(LS_WARNING) << "Failed to send RTCP BYE";
    }

    collision_detected_ = false;

    // Generate a new time_stamp if true and not configured via API
    // Generate a new SSRC for the next "call" if false
    rtp_sender_.SetSendingStatus(sending);
    if (sending) {
      // Make sure the RTCP sender has the same timestamp offset.
      rtcp_sender_.SetStartTimestamp(rtp_sender_.StartTimestamp());
    }

    // Make sure that RTCP objects are aware of our SSRC (it could have changed
    // Due to collision)
    uint32_t SSRC = rtp_sender_.SSRC();
    rtcp_sender_.SetSSRC(SSRC);
    SetRtcpReceiverSsrcs(SSRC);

    return 0;
  }
  return 0;
}

bool ModuleRtpRtcpImpl::Sending() const {
  return rtcp_sender_.Sending();
}

void ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending) {
  rtp_sender_.SetSendingMediaStatus(sending);
}

bool ModuleRtpRtcpImpl::SendingMedia() const {
  return rtp_sender_.SendingMedia();
}

int32_t ModuleRtpRtcpImpl::SendOutgoingData(
    FrameType frame_type,
    int8_t payload_type,
    uint32_t time_stamp,
    int64_t capture_time_ms,
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* rtp_video_hdr) {
  rtcp_sender_.SetLastRtpTime(time_stamp, capture_time_ms);
  // Make sure an RTCP report isn't queued behind a key frame.
  if (rtcp_sender_.TimeToSendRTCPReport(kVideoFrameKey == frame_type)) {
      rtcp_sender_.SendRTCP(GetFeedbackState(), kRtcpReport);
  }
  return rtp_sender_.SendOutgoingData(
      frame_type, payload_type, time_stamp, capture_time_ms, payload_data,
      payload_size, fragmentation, rtp_video_hdr);
}

bool ModuleRtpRtcpImpl::TimeToSendPacket(uint32_t ssrc,
                                         uint16_t sequence_number,
                                         int64_t capture_time_ms,
                                         bool retransmission) {
  if (SendingMedia() && ssrc == rtp_sender_.SSRC()) {
    return rtp_sender_.TimeToSendPacket(
        sequence_number, capture_time_ms, retransmission);
  }
  // No RTP sender is interested in sending this packet.
  return true;
}

size_t ModuleRtpRtcpImpl::TimeToSendPadding(size_t bytes) {
  return rtp_sender_.TimeToSendPadding(bytes);
}

uint16_t ModuleRtpRtcpImpl::MaxPayloadLength() const {
  return rtp_sender_.MaxPayloadLength();
}

uint16_t ModuleRtpRtcpImpl::MaxDataPayloadLength() const {
  return rtp_sender_.MaxDataPayloadLength();
}

int32_t ModuleRtpRtcpImpl::SetTransportOverhead(
    const bool tcp,
    const bool ipv6,
    const uint8_t authentication_overhead) {
  uint16_t packet_overhead = 0;
  if (ipv6) {
    packet_overhead = 40;
  } else {
    packet_overhead = 20;
  }
  if (tcp) {
    // TCP.
    packet_overhead += 20;
  } else {
    // UDP.
    packet_overhead += 8;
  }
  packet_overhead += authentication_overhead;

  if (packet_overhead == packet_overhead_) {
    // Ok same as before.
    return 0;
  }
  // Calc diff.
  int16_t packet_over_head_diff = packet_overhead - packet_overhead_;

  // Store new.
  packet_overhead_ = packet_overhead;

  uint16_t length =
      rtp_sender_.MaxPayloadLength() - packet_over_head_diff;
  return rtp_sender_.SetMaxPayloadLength(length, packet_overhead_);
}

int32_t ModuleRtpRtcpImpl::SetMaxTransferUnit(const uint16_t mtu) {
  RTC_DCHECK_LE(mtu, IP_PACKET_SIZE) << "Invalid mtu: " << mtu;
  return rtp_sender_.SetMaxPayloadLength(mtu - packet_overhead_,
                                         packet_overhead_);
}

RtcpMode ModuleRtpRtcpImpl::RTCP() const {
  if (rtcp_sender_.Status() != RtcpMode::kOff) {
    return rtcp_receiver_.Status();
  }
  return RtcpMode::kOff;
}

// Configure RTCP status i.e on/off.
void ModuleRtpRtcpImpl::SetRTCPStatus(const RtcpMode method) {
  rtcp_sender_.SetRTCPStatus(method);
  rtcp_receiver_.SetRTCPStatus(method);
}

int32_t ModuleRtpRtcpImpl::SetCNAME(const char* c_name) {
  return rtcp_sender_.SetCNAME(c_name);
}

int32_t ModuleRtpRtcpImpl::AddMixedCNAME(uint32_t ssrc, const char* c_name) {
  return rtcp_sender_.AddMixedCNAME(ssrc, c_name);
}

int32_t ModuleRtpRtcpImpl::RemoveMixedCNAME(const uint32_t ssrc) {
  return rtcp_sender_.RemoveMixedCNAME(ssrc);
}

int32_t ModuleRtpRtcpImpl::RemoteCNAME(
    const uint32_t remote_ssrc,
    char c_name[RTCP_CNAME_SIZE]) const {
  return rtcp_receiver_.CNAME(remote_ssrc, c_name);
}

int32_t ModuleRtpRtcpImpl::RemoteNTP(
    uint32_t* received_ntpsecs,
    uint32_t* received_ntpfrac,
    uint32_t* rtcp_arrival_time_secs,
    uint32_t* rtcp_arrival_time_frac,
    uint32_t* rtcp_timestamp) const {
  return rtcp_receiver_.NTP(received_ntpsecs,
                            received_ntpfrac,
                            rtcp_arrival_time_secs,
                            rtcp_arrival_time_frac,
                            rtcp_timestamp)
             ? 0
             : -1;
}

// Get RoundTripTime.
int32_t ModuleRtpRtcpImpl::RTT(const uint32_t remote_ssrc,
                               int64_t* rtt,
                               int64_t* avg_rtt,
                               int64_t* min_rtt,
                               int64_t* max_rtt) const {
  int32_t ret = rtcp_receiver_.RTT(remote_ssrc, rtt, avg_rtt, min_rtt, max_rtt);
  if (rtt && *rtt == 0) {
    // Try to get RTT from RtcpRttStats class.
    *rtt = rtt_ms();
  }
  return ret;
}

// Force a send of an RTCP packet.
// Normal SR and RR are triggered via the process function.
int32_t ModuleRtpRtcpImpl::SendRTCP(RTCPPacketType packet_type) {
  return rtcp_sender_.SendRTCP(GetFeedbackState(), packet_type);
}

// Force a send of an RTCP packet.
// Normal SR and RR are triggered via the process function.
int32_t ModuleRtpRtcpImpl::SendCompoundRTCP(
    const std::set<RTCPPacketType>& packet_types) {
  return rtcp_sender_.SendCompoundRTCP(GetFeedbackState(), packet_types);
}

int32_t ModuleRtpRtcpImpl::SetRTCPApplicationSpecificData(
    const uint8_t sub_type,
    const uint32_t name,
    const uint8_t* data,
    const uint16_t length) {
  return  rtcp_sender_.SetApplicationSpecificData(sub_type, name, data, length);
}

// (XR) VOIP metric.
int32_t ModuleRtpRtcpImpl::SetRTCPVoIPMetrics(
  const RTCPVoIPMetric* voip_metric) {
  return  rtcp_sender_.SetRTCPVoIPMetrics(voip_metric);
}

void ModuleRtpRtcpImpl::SetRtcpXrRrtrStatus(bool enable) {
  return rtcp_sender_.SendRtcpXrReceiverReferenceTime(enable);
}

bool ModuleRtpRtcpImpl::RtcpXrRrtrStatus() const {
  return rtcp_sender_.RtcpXrReceiverReferenceTime();
}

// TODO(asapersson): Replace this method with the one below.
int32_t ModuleRtpRtcpImpl::DataCountersRTP(
    size_t* bytes_sent,
    uint32_t* packets_sent) const {
  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_.GetDataCounters(&rtp_stats, &rtx_stats);

  if (bytes_sent) {
    *bytes_sent = rtp_stats.transmitted.payload_bytes +
                  rtp_stats.transmitted.padding_bytes +
                  rtp_stats.transmitted.header_bytes +
                  rtx_stats.transmitted.payload_bytes +
                  rtx_stats.transmitted.padding_bytes +
                  rtx_stats.transmitted.header_bytes;
  }
  if (packets_sent) {
    *packets_sent = rtp_stats.transmitted.packets +
                    rtx_stats.transmitted.packets;
  }
  return 0;
}

void ModuleRtpRtcpImpl::GetSendStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  rtp_sender_.GetDataCounters(rtp_counters, rtx_counters);
}

void ModuleRtpRtcpImpl::GetRtpPacketLossStats(
    bool outgoing,
    uint32_t ssrc,
    struct RtpPacketLossStats* loss_stats) const {
  if (!loss_stats) return;
  const PacketLossStats* stats_source = NULL;
  if (outgoing) {
    if (SSRC() == ssrc) {
      stats_source = &send_loss_stats_;
    }
  } else {
    if (rtcp_receiver_.RemoteSSRC() == ssrc) {
      stats_source = &receive_loss_stats_;
    }
  }
  if (stats_source) {
    loss_stats->single_packet_loss_count =
        stats_source->GetSingleLossCount();
    loss_stats->multiple_packet_loss_event_count =
        stats_source->GetMultipleLossEventCount();
    loss_stats->multiple_packet_loss_packet_count =
        stats_source->GetMultipleLossPacketCount();
  }
}

int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(RTCPSenderInfo* sender_info) {
  return rtcp_receiver_.SenderInfoReceived(sender_info);
}

// Received RTCP report.
int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(
    std::vector<RTCPReportBlock>* receive_blocks) const {
  return rtcp_receiver_.StatisticsReceived(receive_blocks);
}

// (REMB) Receiver Estimated Max Bitrate.
bool ModuleRtpRtcpImpl::REMB() const {
  return rtcp_sender_.REMB();
}

void ModuleRtpRtcpImpl::SetREMBStatus(const bool enable) {
  rtcp_sender_.SetREMBStatus(enable);
}

void ModuleRtpRtcpImpl::SetREMBData(const uint32_t bitrate,
                                    const std::vector<uint32_t>& ssrcs) {
  rtcp_sender_.SetREMBData(bitrate, ssrcs);
}

int32_t ModuleRtpRtcpImpl::RegisterSendRtpHeaderExtension(
    const RTPExtensionType type,
    const uint8_t id) {
  return rtp_sender_.RegisterRtpHeaderExtension(type, id);
}

int32_t ModuleRtpRtcpImpl::DeregisterSendRtpHeaderExtension(
    const RTPExtensionType type) {
  return rtp_sender_.DeregisterRtpHeaderExtension(type);
}

// (TMMBR) Temporary Max Media Bit Rate.
bool ModuleRtpRtcpImpl::TMMBR() const {
  return rtcp_sender_.TMMBR();
}

void ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable) {
  rtcp_sender_.SetTMMBRStatus(enable);
}

int32_t ModuleRtpRtcpImpl::SetTMMBN(const TMMBRSet* bounding_set) {
  uint32_t max_bitrate_kbit =
      rtp_sender_.MaxConfiguredBitrateVideo() / 1000;
  return rtcp_sender_.SetTMMBN(bounding_set, max_bitrate_kbit);
}

// Returns the currently configured retransmission mode.
int ModuleRtpRtcpImpl::SelectiveRetransmissions() const {
  return rtp_sender_.SelectiveRetransmissions();
}

// Enable or disable a retransmission mode, which decides which packets will
// be retransmitted if NACKed.
int ModuleRtpRtcpImpl::SetSelectiveRetransmissions(uint8_t settings) {
  return rtp_sender_.SetSelectiveRetransmissions(settings);
}

// Send a Negative acknowledgment packet.
int32_t ModuleRtpRtcpImpl::SendNACK(const uint16_t* nack_list,
                                    const uint16_t size) {
  for (int i = 0; i < size; ++i) {
    receive_loss_stats_.AddLostPacket(nack_list[i]);
  }
  uint16_t nack_length = size;
  uint16_t start_id = 0;
  int64_t now = clock_->TimeInMilliseconds();
  if (TimeToSendFullNackList(now)) {
    nack_last_time_sent_full_ = now;
    nack_last_time_sent_full_prev_ = now;
  } else {
    // Only send extended list.
    if (nack_last_seq_number_sent_ == nack_list[size - 1]) {
      // Last sequence number is the same, do not send list.
      return 0;
    }
    // Send new sequence numbers.
    for (int i = 0; i < size; ++i) {
      if (nack_last_seq_number_sent_ == nack_list[i]) {
        start_id = i + 1;
        break;
      }
    }
    nack_length = size - start_id;
  }

  // Our RTCP NACK implementation is limited to kRtcpMaxNackFields sequence
  // numbers per RTCP packet.
  if (nack_length > kRtcpMaxNackFields) {
    nack_length = kRtcpMaxNackFields;
  }
  nack_last_seq_number_sent_ = nack_list[start_id + nack_length - 1];

  return rtcp_sender_.SendRTCP(
      GetFeedbackState(), kRtcpNack, nack_length, &nack_list[start_id]);
}

bool ModuleRtpRtcpImpl::TimeToSendFullNackList(int64_t now) const {
  // Use RTT from RtcpRttStats class if provided.
  int64_t rtt = rtt_ms();
  if (rtt == 0) {
    rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &rtt, NULL, NULL);
  }

  const int64_t kStartUpRttMs = 100;
  int64_t wait_time = 5 + ((rtt * 3) >> 1);  // 5 + RTT * 1.5.
  if (rtt == 0) {
    wait_time = kStartUpRttMs;
  }

  // Send a full NACK list once within every |wait_time|.
  if (rtt_stats_) {
    return now - nack_last_time_sent_full_ > wait_time;
  }
  return now - nack_last_time_sent_full_prev_ > wait_time;
}

// Store the sent packets, needed to answer to Negative acknowledgment requests.
void ModuleRtpRtcpImpl::SetStorePacketsStatus(const bool enable,
                                              const uint16_t number_to_store) {
  rtp_sender_.SetStorePacketsStatus(enable, number_to_store);
}

bool ModuleRtpRtcpImpl::StorePackets() const {
  return rtp_sender_.StorePackets();
}

void ModuleRtpRtcpImpl::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtcp_receiver_.RegisterRtcpStatisticsCallback(callback);
}

RtcpStatisticsCallback* ModuleRtpRtcpImpl::GetRtcpStatisticsCallback() {
  return rtcp_receiver_.GetRtcpStatisticsCallback();
}

bool ModuleRtpRtcpImpl::SendFeedbackPacket(
    const rtcp::TransportFeedback& packet) {
  return rtcp_sender_.SendFeedbackPacket(packet);
}

// Send a TelephoneEvent tone using RFC 2833 (4733).
int32_t ModuleRtpRtcpImpl::SendTelephoneEventOutband(
    const uint8_t key,
    const uint16_t time_ms,
    const uint8_t level) {
  return rtp_sender_.SendTelephoneEvent(key, time_ms, level);
}

// Set audio packet size, used to determine when it's time to send a DTMF
// packet in silence (CNG).
int32_t ModuleRtpRtcpImpl::SetAudioPacketSize(
    const uint16_t packet_size_samples) {
  return rtp_sender_.SetAudioPacketSize(packet_size_samples);
}

int32_t ModuleRtpRtcpImpl::SetAudioLevel(
    const uint8_t level_d_bov) {
  return rtp_sender_.SetAudioLevel(level_d_bov);
}

// Set payload type for Redundant Audio Data RFC 2198.
int32_t ModuleRtpRtcpImpl::SetSendREDPayloadType(
    const int8_t payload_type) {
  return rtp_sender_.SetRED(payload_type);
}

// Get payload type for Redundant Audio Data RFC 2198.
int32_t ModuleRtpRtcpImpl::SendREDPayloadType(int8_t* payload_type) const {
  return rtp_sender_.RED(payload_type);
}

void ModuleRtpRtcpImpl::SetTargetSendBitrate(uint32_t bitrate_bps) {
  rtp_sender_.SetTargetBitrate(bitrate_bps);
}

int32_t ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  key_frame_req_method_ = method;
  return 0;
}

int32_t ModuleRtpRtcpImpl::RequestKeyFrame() {
  switch (key_frame_req_method_) {
    case kKeyFrameReqPliRtcp:
      return SendRTCP(kRtcpPli);
    case kKeyFrameReqFirRtcp:
      return SendRTCP(kRtcpFir);
  }
  return -1;
}

int32_t ModuleRtpRtcpImpl::SendRTCPSliceLossIndication(
    const uint8_t picture_id) {
  return rtcp_sender_.SendRTCP(
      GetFeedbackState(), kRtcpSli, 0, 0, false, picture_id);
}

void ModuleRtpRtcpImpl::SetGenericFECStatus(
    const bool enable,
    const uint8_t payload_type_red,
    const uint8_t payload_type_fec) {
  rtp_sender_.SetGenericFECStatus(enable, payload_type_red, payload_type_fec);
}

void ModuleRtpRtcpImpl::GenericFECStatus(bool* enable,
                                         uint8_t* payload_type_red,
                                         uint8_t* payload_type_fec) {
  rtp_sender_.GenericFECStatus(enable, payload_type_red, payload_type_fec);
}

int32_t ModuleRtpRtcpImpl::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  return rtp_sender_.SetFecParameters(delta_params, key_params);
}

void ModuleRtpRtcpImpl::SetRemoteSSRC(const uint32_t ssrc) {
  // Inform about the incoming SSRC.
  rtcp_sender_.SetRemoteSSRC(ssrc);
  rtcp_receiver_.SetRemoteSSRC(ssrc);

  // Check for a SSRC collision.
  if (rtp_sender_.SSRC() == ssrc && !collision_detected_) {
    // If we detect a collision change the SSRC but only once.
    collision_detected_ = true;
    uint32_t new_ssrc = rtp_sender_.GenerateNewSSRC();
    if (new_ssrc == 0) {
      // Configured via API ignore.
      return;
    }
    if (RtcpMode::kOff != rtcp_sender_.Status()) {
      // Send RTCP bye on the current SSRC.
      SendRTCP(kRtcpBye);
    }
    // Change local SSRC and inform all objects about the new SSRC.
    rtcp_sender_.SetSSRC(new_ssrc);
    SetRtcpReceiverSsrcs(new_ssrc);
  }
}

void ModuleRtpRtcpImpl::BitrateSent(uint32_t* total_rate,
                                    uint32_t* video_rate,
                                    uint32_t* fec_rate,
                                    uint32_t* nack_rate) const {
  *total_rate = rtp_sender_.BitrateSent();
  *video_rate = rtp_sender_.VideoBitrateSent();
  *fec_rate = rtp_sender_.FecOverheadRate();
  *nack_rate = rtp_sender_.NackOverheadRate();
}

void ModuleRtpRtcpImpl::OnRequestIntraFrame() {
  RequestKeyFrame();
}

void ModuleRtpRtcpImpl::OnRequestSendReport() {
  SendRTCP(kRtcpSr);
}

int32_t ModuleRtpRtcpImpl::SendRTCPReferencePictureSelection(
    const uint64_t picture_id) {
  return rtcp_sender_.SendRTCP(
      GetFeedbackState(), kRtcpRpsi, 0, 0, false, picture_id);
}

int64_t ModuleRtpRtcpImpl::SendTimeOfSendReport(
    const uint32_t send_report) {
  return rtcp_sender_.SendTimeOfSendReport(send_report);
}

bool ModuleRtpRtcpImpl::SendTimeOfXrRrReport(
    uint32_t mid_ntp, int64_t* time_ms) const {
  return rtcp_sender_.SendTimeOfXrRrReport(mid_ntp, time_ms);
}

void ModuleRtpRtcpImpl::OnReceivedNACK(
    const std::list<uint16_t>& nack_sequence_numbers) {
  for (uint16_t nack_sequence_number : nack_sequence_numbers) {
    send_loss_stats_.AddLostPacket(nack_sequence_number);
  }
  if (!rtp_sender_.StorePackets() ||
      nack_sequence_numbers.size() == 0) {
    return;
  }
  // Use RTT from RtcpRttStats class if provided.
  int64_t rtt = rtt_ms();
  if (rtt == 0) {
    rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &rtt, NULL, NULL);
  }
  rtp_sender_.OnReceivedNACK(nack_sequence_numbers, rtt);
}

bool ModuleRtpRtcpImpl::LastReceivedNTP(
    uint32_t* rtcp_arrival_time_secs,  // When we got the last report.
    uint32_t* rtcp_arrival_time_frac,
    uint32_t* remote_sr) const {
  // Remote SR: NTP inside the last received (mid 16 bits from sec and frac).
  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;

  if (!rtcp_receiver_.NTP(&ntp_secs,
                          &ntp_frac,
                          rtcp_arrival_time_secs,
                          rtcp_arrival_time_frac,
                          NULL)) {
    return false;
  }
  *remote_sr =
      ((ntp_secs & 0x0000ffff) << 16) + ((ntp_frac & 0xffff0000) >> 16);
  return true;
}

bool ModuleRtpRtcpImpl::LastReceivedXrReferenceTimeInfo(
    RtcpReceiveTimeInfo* info) const {
  return rtcp_receiver_.LastReceivedXrReferenceTimeInfo(info);
}

bool ModuleRtpRtcpImpl::UpdateRTCPReceiveInformationTimers() {
  // If this returns true this channel has timed out.
  // Periodically check if this is true and if so call UpdateTMMBR.
  return rtcp_receiver_.UpdateRTCPReceiveInformationTimers();
}

// Called from RTCPsender.
int32_t ModuleRtpRtcpImpl::BoundingSet(bool* tmmbr_owner,
                                       TMMBRSet* bounding_set) {
  return rtcp_receiver_.BoundingSet(tmmbr_owner, bounding_set);
}

int64_t ModuleRtpRtcpImpl::RtcpReportInterval() {
  if (audio_)
    return RTCP_INTERVAL_AUDIO_MS;
  else
    return RTCP_INTERVAL_VIDEO_MS;
}

void ModuleRtpRtcpImpl::SetRtcpReceiverSsrcs(uint32_t main_ssrc) {
  std::set<uint32_t> ssrcs;
  ssrcs.insert(main_ssrc);
  if (rtp_sender_.RtxStatus() != kRtxOff)
    ssrcs.insert(rtp_sender_.RtxSsrc());
  rtcp_receiver_.SetSsrcs(main_ssrc, ssrcs);
}

void ModuleRtpRtcpImpl::set_rtt_ms(int64_t rtt_ms) {
  CriticalSectionScoped cs(critical_section_rtt_.get());
  rtt_ms_ = rtt_ms;
}

int64_t ModuleRtpRtcpImpl::rtt_ms() const {
  CriticalSectionScoped cs(critical_section_rtt_.get());
  return rtt_ms_;
}

void ModuleRtpRtcpImpl::RegisterSendChannelRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  rtp_sender_.RegisterRtpStatisticsCallback(callback);
}

StreamDataCountersCallback*
    ModuleRtpRtcpImpl::GetSendChannelRtpStatisticsCallback() const {
  return rtp_sender_.GetRtpStatisticsCallback();
}
}  // namespace webrtc
