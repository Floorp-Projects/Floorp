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

#include <assert.h>
#include <string.h>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace.h"

#ifdef _WIN32
// Disable warning C4355: 'this' : used in base member initializer list.
#pragma warning(disable : 4355)
#endif

namespace webrtc {

RtpRtcp::Configuration::Configuration()
    : id(-1),
      audio(false),
      clock(NULL),
      default_module(NULL),
      receive_statistics(NullObjectReceiveStatistics()),
      outgoing_transport(NULL),
      rtcp_feedback(NULL),
      intra_frame_callback(NULL),
      bandwidth_callback(NULL),
      rtt_stats(NULL),
      audio_messages(NullObjectRtpAudioFeedback()),
      remote_bitrate_estimator(NULL),
      paced_sender(NULL),
      send_bitrate_observer(NULL),
      send_frame_count_observer(NULL),
      send_side_delay_observer(NULL) {
}

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
    : rtp_sender_(configuration.id,
                  configuration.audio,
                  configuration.clock,
                  configuration.outgoing_transport,
                  configuration.audio_messages,
                  configuration.paced_sender,
                  configuration.send_bitrate_observer,
                  configuration.send_frame_count_observer,
                  configuration.send_side_delay_observer),
      rtcp_sender_(configuration.id,
                   configuration.audio,
                   configuration.clock,
                   configuration.receive_statistics),
      rtcp_receiver_(configuration.id, configuration.clock, this),
      clock_(configuration.clock),
      id_(configuration.id),
      audio_(configuration.audio),
      collision_detected_(false),
      last_process_time_(configuration.clock->TimeInMilliseconds()),
      last_bitrate_process_time_(configuration.clock->TimeInMilliseconds()),
      last_rtt_process_time_(configuration.clock->TimeInMilliseconds()),
      packet_overhead_(28),  // IPV4 UDP.
      critical_section_module_ptrs_(
          CriticalSectionWrapper::CreateCriticalSection()),
      critical_section_module_ptrs_feedback_(
          CriticalSectionWrapper::CreateCriticalSection()),
      default_module_(
          static_cast<ModuleRtpRtcpImpl*>(configuration.default_module)),
      padding_index_(static_cast<size_t>(-1)),  // Start padding at first child.
      nack_method_(kNackOff),
      nack_last_time_sent_full_(0),
      nack_last_seq_number_sent_(0),
      simulcast_(false),
      key_frame_req_method_(kKeyFrameReqFirRtp),
      remote_bitrate_(configuration.remote_bitrate_estimator),
      rtt_stats_(configuration.rtt_stats),
      critical_section_rtt_(CriticalSectionWrapper::CreateCriticalSection()),
      rtt_ms_(0) {
  send_video_codec_.codecType = kVideoCodecUnknown;

  if (default_module_) {
    default_module_->RegisterChildModule(this);
  }
  // TODO(pwestin) move to constructors of each rtp/rtcp sender/receiver object.
  rtcp_receiver_.RegisterRtcpObservers(configuration.intra_frame_callback,
                                       configuration.bandwidth_callback,
                                       configuration.rtcp_feedback);
  rtcp_sender_.RegisterSendTransport(configuration.outgoing_transport);

  // Make sure that RTCP objects are aware of our SSRC.
  uint32_t SSRC = rtp_sender_.SSRC();
  rtcp_sender_.SetSSRC(SSRC);
  SetRtcpReceiverSsrcs(SSRC);
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl() {
  // All child modules MUST be deleted before deleting the default.
  assert(child_modules_.empty());

  // Deregister for the child modules.
  // Will go in to the default and remove it self.
  if (default_module_) {
    default_module_->DeRegisterChildModule(this);
  }
}

void ModuleRtpRtcpImpl::RegisterChildModule(RtpRtcp* module) {
  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  // We use two locks for protecting child_modules_, one
  // (critical_section_module_ptrs_feedback_) for incoming
  // messages (BitrateSent) and critical_section_module_ptrs_
  // for all outgoing messages sending packets etc.
  child_modules_.push_back(static_cast<ModuleRtpRtcpImpl*>(module));
}

void ModuleRtpRtcpImpl::DeRegisterChildModule(RtpRtcp* remove_module) {
  CriticalSectionScoped lock(
      critical_section_module_ptrs_.get());
  CriticalSectionScoped double_lock(
      critical_section_module_ptrs_feedback_.get());

  std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RtpRtcp* module = *it;
    if (module == remove_module) {
      child_modules_.erase(it);
      return;
    }
    it++;
  }
}

// Returns the number of milliseconds until the module want a worker thread
// to call Process.
int32_t ModuleRtpRtcpImpl::TimeUntilNextProcess() {
    const int64_t now = clock_->TimeInMilliseconds();
  return kRtpRtcpMaxIdleTimeProcess - (now - last_process_time_);
}

// Process any pending tasks such as timeouts (non time critical events).
int32_t ModuleRtpRtcpImpl::Process() {
  const int64_t now = clock_->TimeInMilliseconds();
  last_process_time_ = now;

  if (now >= last_bitrate_process_time_ + kRtpRtcpBitrateProcessTimeMs) {
    rtp_sender_.ProcessBitrate();
    last_bitrate_process_time_ = now;
  }

  if (!IsDefaultModule()) {
    bool process_rtt = now >= last_rtt_process_time_ + kRtpRtcpRttProcessTimeMs;
    if (rtcp_sender_.Sending()) {
      // Process RTT if we have received a receiver report and we haven't
      // processed RTT for at least |kRtpRtcpRttProcessTimeMs| milliseconds.
      if (rtcp_receiver_.LastReceivedReceiverReport() >
          last_rtt_process_time_ && process_rtt) {
        std::vector<RTCPReportBlock> receive_blocks;
        rtcp_receiver_.StatisticsReceived(&receive_blocks);
        uint16_t max_rtt = 0;
        for (std::vector<RTCPReportBlock>::iterator it = receive_blocks.begin();
             it != receive_blocks.end(); ++it) {
          uint16_t rtt = 0;
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
         uint16_t rtt_ms;
         if (rtt_stats_ && rtcp_receiver_.GetAndResetXrRrRtt(&rtt_ms)) {
           rtt_stats_->OnRttUpdate(rtt_ms);
         }
      }
    }

    // Get processed rtt.
    if (process_rtt) {
      last_rtt_process_time_ = now;
      if (rtt_stats_) {
        set_rtt_ms(rtt_stats_->LastProcessedRtt());
      }
    }

    if (rtcp_sender_.TimeToSendRTCPReport()) {
      rtcp_sender_.SendRTCP(GetFeedbackState(), kRtcpReport);
    }
  }

  if (UpdateRTCPReceiveInformationTimers()) {
    // A receiver has timed out
    rtcp_receiver_.UpdateTMMBR();
  }
  return 0;
}

void ModuleRtpRtcpImpl::SetRTXSendStatus(int mode) {
  rtp_sender_.SetRTXStatus(mode);
}

void ModuleRtpRtcpImpl::RTXSendStatus(int* mode,
                                      uint32_t* ssrc,
                                      int* payload_type) const {
  rtp_sender_.RTXStatus(mode, ssrc, payload_type);
}

void ModuleRtpRtcpImpl::SetRtxSsrc(uint32_t ssrc) {
  rtp_sender_.SetRtxSsrc(ssrc);
}

void ModuleRtpRtcpImpl::SetRtxSendPayloadType(int payload_type) {
  rtp_sender_.SetRtxPayloadType(payload_type);
}

int32_t ModuleRtpRtcpImpl::IncomingRtcpPacket(
    const uint8_t* rtcp_packet,
    const uint16_t length) {
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

int32_t ModuleRtpRtcpImpl::RegisterSendPayload(
    const VideoCodec& video_codec) {
  send_video_codec_ = video_codec;
  {
    // simulcast_ is accessed when accessing child_modules_, so this write needs
    // to be protected by the same lock.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    simulcast_ = video_codec.numberOfSimulcastStreams > 1;
  }
  return rtp_sender_.RegisterPayload(video_codec.plName,
                                     video_codec.plType,
                                     90000,
                                     0,
                                     video_codec.maxBitrate);
}

int32_t ModuleRtpRtcpImpl::DeRegisterSendPayload(
    const int8_t payload_type) {
  return rtp_sender_.DeRegisterSendPayload(payload_type);
}

int8_t ModuleRtpRtcpImpl::SendPayloadType() const {
  return rtp_sender_.SendPayloadType();
}

uint32_t ModuleRtpRtcpImpl::StartTimestamp() const {
  return rtp_sender_.StartTimestamp();
}

// Configure start timestamp, default is a random number.
int32_t ModuleRtpRtcpImpl::SetStartTimestamp(
    const uint32_t timestamp) {
  rtcp_sender_.SetStartTimestamp(timestamp);
  rtp_sender_.SetStartTimestamp(timestamp, true);
  return 0;  // TODO(pwestin): change to void.
}

uint16_t ModuleRtpRtcpImpl::SequenceNumber() const {
  return rtp_sender_.SequenceNumber();
}

// Set SequenceNumber, default is a random number.
int32_t ModuleRtpRtcpImpl::SetSequenceNumber(
    const uint16_t seq_num) {
  rtp_sender_.SetSequenceNumber(seq_num);
  return 0;  // TODO(pwestin): change to void.
}

void ModuleRtpRtcpImpl::SetRtpStateForSsrc(uint32_t ssrc,
                                           const RtpState& rtp_state) {
  if (rtp_sender_.SSRC() == ssrc) {
    rtp_sender_.SetRtpState(rtp_state);
    return;
  }
  if (rtp_sender_.RtxSsrc() == ssrc) {
    rtp_sender_.SetRtxRtpState(rtp_state);
    return;
  }

  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  for (size_t i = 0; i < child_modules_.size(); ++i) {
    child_modules_[i]->SetRtpStateForSsrc(ssrc, rtp_state);
  }
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

  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  for (size_t i = 0; i < child_modules_.size(); ++i) {
    if (child_modules_[i]->GetRtpStateForSsrc(ssrc, rtp_state))
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

int32_t ModuleRtpRtcpImpl::SetCSRCStatus(const bool include) {
  rtcp_sender_.SetCSRCStatus(include);
  rtp_sender_.SetCSRCStatus(include);
  return 0;  // TODO(pwestin): change to void.
}

int32_t ModuleRtpRtcpImpl::CSRCs(
  uint32_t arr_of_csrc[kRtpCsrcSize]) const {
  return rtp_sender_.CSRCs(arr_of_csrc);
}

int32_t ModuleRtpRtcpImpl::SetCSRCs(
    const uint32_t arr_of_csrc[kRtpCsrcSize],
    const uint8_t arr_length) {
  if (IsDefaultModule()) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCSRCs(arr_of_csrc, arr_length);
      }
      it++;
    }
  } else {
    rtcp_sender_.SetCSRCs(arr_of_csrc, arr_length);
    rtp_sender_.SetCSRCs(arr_of_csrc, arr_length);
  }
  return 0;  // TODO(pwestin): change to void.
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
  state.packets_sent = rtp_stats.packets + rtx_stats.packets;
  state.media_bytes_sent = rtp_stats.bytes + rtx_stats.bytes;
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

int32_t ModuleRtpRtcpImpl::SetSendingMediaStatus(const bool sending) {
  rtp_sender_.SetSendingMediaStatus(sending);
  return 0;
}

bool ModuleRtpRtcpImpl::SendingMedia() const {
  if (!IsDefaultModule()) {
    return rtp_sender_.SendingMedia();
  }

  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  std::vector<ModuleRtpRtcpImpl*>::const_iterator it = child_modules_.begin();
  while (it != child_modules_.end()) {
    RTPSender& rtp_sender = (*it)->rtp_sender_;
    if (rtp_sender.SendingMedia()) {
      return true;
    }
    it++;
  }
  return false;
}

int32_t ModuleRtpRtcpImpl::SendOutgoingData(
    FrameType frame_type,
    int8_t payload_type,
    uint32_t time_stamp,
    int64_t capture_time_ms,
    const uint8_t* payload_data,
    uint32_t payload_size,
    const RTPFragmentationHeader* fragmentation,
    const RTPVideoHeader* rtp_video_hdr) {
  rtcp_sender_.SetLastRtpTime(time_stamp, capture_time_ms);

  if (!IsDefaultModule()) {
    // Don't send RTCP from default module.
    if (rtcp_sender_.TimeToSendRTCPReport(kVideoFrameKey == frame_type)) {
      rtcp_sender_.SendRTCP(GetFeedbackState(), kRtcpReport);
    }
    return rtp_sender_.SendOutgoingData(frame_type,
                                        payload_type,
                                        time_stamp,
                                        capture_time_ms,
                                        payload_data,
                                        payload_size,
                                        fragmentation,
                                        NULL,
                                        &(rtp_video_hdr->codecHeader));
  }
  int32_t ret_val = -1;
  CriticalSectionScoped lock(critical_section_module_ptrs_.get());
  if (simulcast_) {
    if (rtp_video_hdr == NULL) {
      return -1;
    }
    int idx = 0;
    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    for (; idx < rtp_video_hdr->simulcastIdx; ++it) {
      if (it == child_modules_.end()) {
        return -1;
      }
      if ((*it)->SendingMedia()) {
        ++idx;
      }
    }
    for (; it != child_modules_.end(); ++it) {
      if ((*it)->SendingMedia()) {
        break;
      }
      ++idx;
    }
    if (it == child_modules_.end()) {
      return -1;
    }
    return (*it)->SendOutgoingData(frame_type,
                                   payload_type,
                                   time_stamp,
                                   capture_time_ms,
                                   payload_data,
                                   payload_size,
                                   fragmentation,
                                   rtp_video_hdr);
  } else {
    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    // Send to all "child" modules
    while (it != child_modules_.end()) {
      if ((*it)->SendingMedia()) {
        ret_val = (*it)->SendOutgoingData(frame_type,
                                          payload_type,
                                          time_stamp,
                                          capture_time_ms,
                                          payload_data,
                                          payload_size,
                                          fragmentation,
                                          rtp_video_hdr);
      }
      it++;
    }
  }
  return ret_val;
}

bool ModuleRtpRtcpImpl::TimeToSendPacket(uint32_t ssrc,
                                         uint16_t sequence_number,
                                         int64_t capture_time_ms,
                                         bool retransmission) {
  if (!IsDefaultModule()) {
    // Don't send from default module.
    if (SendingMedia() && ssrc == rtp_sender_.SSRC()) {
      return rtp_sender_.TimeToSendPacket(sequence_number, capture_time_ms,
                                          retransmission);
    }
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      if ((*it)->SendingMedia() && ssrc == (*it)->rtp_sender_.SSRC()) {
        return (*it)->rtp_sender_.TimeToSendPacket(sequence_number,
                                                   capture_time_ms,
                                                   retransmission);
      }
      ++it;
    }
  }
  // No RTP sender is interested in sending this packet.
  return true;
}

int ModuleRtpRtcpImpl::TimeToSendPadding(int bytes) {
  if (!IsDefaultModule()) {
    // Don't send from default module.
    return rtp_sender_.TimeToSendPadding(bytes);
  } else {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    for (size_t i = 0; i < child_modules_.size(); ++i) {
      // Send padding on one of the modules sending media.
      if (child_modules_[i]->SendingMedia()) {
        return child_modules_[i]->rtp_sender_.TimeToSendPadding(bytes);
      }
    }
  }
  return 0;
}

bool ModuleRtpRtcpImpl::GetSendSideDelay(int* avg_send_delay_ms,
                                         int* max_send_delay_ms) const {
  assert(avg_send_delay_ms);
  assert(max_send_delay_ms);

  if (IsDefaultModule()) {
    // This API is only supported for child modules.
    return false;
  }
  return rtp_sender_.GetSendSideDelay(avg_send_delay_ms, max_send_delay_ms);
}

uint16_t ModuleRtpRtcpImpl::MaxPayloadLength() const {
  return rtp_sender_.MaxPayloadLength();
}

uint16_t ModuleRtpRtcpImpl::MaxDataPayloadLength() const {
  // Assuming IP/UDP.
  uint16_t min_data_payload_length = IP_PACKET_SIZE - 28;

  if (IsDefaultModule()) {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::vector<ModuleRtpRtcpImpl*>::const_iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        uint16_t data_payload_length =
          module->MaxDataPayloadLength();
        if (data_payload_length < min_data_payload_length) {
          min_data_payload_length = data_payload_length;
        }
      }
      it++;
    }
  }

  uint16_t data_payload_length = rtp_sender_.MaxDataPayloadLength();
  if (data_payload_length < min_data_payload_length) {
    min_data_payload_length = data_payload_length;
  }
  return min_data_payload_length;
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
  if (mtu > IP_PACKET_SIZE) {
    LOG(LS_ERROR) << "Invalid mtu: " << mtu;
    return -1;
  }
  return rtp_sender_.SetMaxPayloadLength(mtu - packet_overhead_,
                                         packet_overhead_);
}

RTCPMethod ModuleRtpRtcpImpl::RTCP() const {
  if (rtcp_sender_.Status() != kRtcpOff) {
    return rtcp_receiver_.Status();
  }
  return kRtcpOff;
}

// Configure RTCP status i.e on/off.
int32_t ModuleRtpRtcpImpl::SetRTCPStatus(const RTCPMethod method) {
  if (rtcp_sender_.SetRTCPStatus(method) == 0) {
    return rtcp_receiver_.SetRTCPStatus(method);
  }
  return -1;
}

// Only for internal test.
uint32_t ModuleRtpRtcpImpl::LastSendReport(
    uint32_t& last_rtcptime) {
  return rtcp_sender_.LastSendReport(last_rtcptime);
}

int32_t ModuleRtpRtcpImpl::SetCNAME(const char c_name[RTCP_CNAME_SIZE]) {
  return rtcp_sender_.SetCNAME(c_name);
}

int32_t ModuleRtpRtcpImpl::AddMixedCNAME(
  const uint32_t ssrc,
  const char c_name[RTCP_CNAME_SIZE]) {
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
                               uint16_t* rtt,
                               uint16_t* avg_rtt,
                               uint16_t* min_rtt,
                               uint16_t* max_rtt) const {
  int32_t ret = rtcp_receiver_.RTT(remote_ssrc, rtt, avg_rtt, min_rtt, max_rtt);
  if (rtt && *rtt == 0) {
    // Try to get RTT from RtcpRttStats class.
    *rtt = static_cast<uint16_t>(rtt_ms());
  }
  return ret;
}

// Reset RoundTripTime statistics.
int32_t ModuleRtpRtcpImpl::ResetRTT(const uint32_t remote_ssrc) {
  return rtcp_receiver_.ResetRTT(remote_ssrc);
}

int32_t
ModuleRtpRtcpImpl::GetReportBlockInfo(const uint32_t remote_ssrc,
                                      uint32_t* ntp_high,
                                      uint32_t* ntp_low,
                                      uint32_t* packets_received,
                                      uint64_t* octets_received) const {
  WEBRTC_TRACE(kTraceModuleCall, kTraceRtpRtcp, id_, "RemotePacketsReceived()");

  return rtcp_receiver_.GetReportBlockInfo(remote_ssrc,
                                           ntp_high, ntp_low,
                                           packets_received, octets_received);
}

// Reset RTP data counters for the sending side.
int32_t ModuleRtpRtcpImpl::ResetSendDataCountersRTP() {
  rtp_sender_.ResetDataCounters();
  return 0;  // TODO(pwestin): change to void.
}

// Force a send of an RTCP packet.
// Normal SR and RR are triggered via the process function.
int32_t ModuleRtpRtcpImpl::SendRTCP(uint32_t rtcp_packet_type) {
  return rtcp_sender_.SendRTCP(GetFeedbackState(), rtcp_packet_type);
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

int32_t ModuleRtpRtcpImpl::DataCountersRTP(
    uint32_t* bytes_sent,
    uint32_t* packets_sent) const {
  StreamDataCounters rtp_stats;
  StreamDataCounters rtx_stats;
  rtp_sender_.GetDataCounters(&rtp_stats, &rtx_stats);

  if (bytes_sent) {
    *bytes_sent = rtp_stats.bytes + rtp_stats.padding_bytes +
                  rtp_stats.header_bytes + rtx_stats.bytes +
                  rtx_stats.padding_bytes + rtx_stats.header_bytes;
  }
  if (packets_sent) {
    *packets_sent = rtp_stats.packets + rtx_stats.packets;
  }
  return 0;
}

int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(RTCPSenderInfo* sender_info) {
  return rtcp_receiver_.SenderInfoReceived(sender_info);
}

// Received RTCP report.
int32_t ModuleRtpRtcpImpl::RemoteRTCPStat(
    std::vector<RTCPReportBlock>* receive_blocks) const {
  return rtcp_receiver_.StatisticsReceived(receive_blocks);
}

int32_t ModuleRtpRtcpImpl::AddRTCPReportBlock(
    const uint32_t ssrc,
    const RTCPReportBlock* report_block) {
  return rtcp_sender_.AddExternalReportBlock(ssrc, report_block);
}

int32_t ModuleRtpRtcpImpl::RemoveRTCPReportBlock(
  const uint32_t ssrc) {
  return rtcp_sender_.RemoveExternalReportBlock(ssrc);
}

void ModuleRtpRtcpImpl::GetRtcpPacketTypeCounters(
    RtcpPacketTypeCounter* packets_sent,
    RtcpPacketTypeCounter* packets_received) const {
  rtcp_sender_.GetPacketTypeCounter(packets_sent);
  rtcp_receiver_.GetPacketTypeCounter(packets_received);
}

// (REMB) Receiver Estimated Max Bitrate.
bool ModuleRtpRtcpImpl::REMB() const {
  return rtcp_sender_.REMB();
}

int32_t ModuleRtpRtcpImpl::SetREMBStatus(const bool enable) {
  return rtcp_sender_.SetREMBStatus(enable);
}

int32_t ModuleRtpRtcpImpl::SetREMBData(const uint32_t bitrate,
                                       const uint8_t number_of_ssrc,
                                       const uint32_t* ssrc) {
  return rtcp_sender_.SetREMBData(bitrate, number_of_ssrc, ssrc);
}

// (IJ) Extended jitter report.
bool ModuleRtpRtcpImpl::IJ() const {
  return rtcp_sender_.IJ();
}

int32_t ModuleRtpRtcpImpl::SetIJStatus(const bool enable) {
  return rtcp_sender_.SetIJStatus(enable);
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

int32_t ModuleRtpRtcpImpl::SetTMMBRStatus(const bool enable) {
  return rtcp_sender_.SetTMMBRStatus(enable);
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
  // Use RTT from RtcpRttStats class if provided.
  uint16_t rtt = rtt_ms();
  if (rtt == 0) {
    rtcp_receiver_.RTT(rtcp_receiver_.RemoteSSRC(), NULL, &rtt, NULL, NULL);
  }

  int64_t wait_time = 5 + ((rtt * 3) >> 1);  // 5 + RTT * 1.5.
  if (wait_time == 5) {
    wait_time = 100;  // During startup we don't have an RTT.
  }
  const int64_t now = clock_->TimeInMilliseconds();
  const int64_t time_limit = now - wait_time;
  uint16_t nackLength = size;
  uint16_t start_id = 0;

  if (nack_last_time_sent_full_ < time_limit) {
    // Send list. Set the timer to make sure we only send a full NACK list once
    // within every time_limit.
    nack_last_time_sent_full_ = now;
  } else {
    // Only send if extended list.
    if (nack_last_seq_number_sent_ == nack_list[size - 1]) {
      // Last seq num is the same don't send list.
      return 0;
    } else {
      // Send NACKs only for new sequence numbers to avoid re-sending
      // NACKs for sequences we have already sent.
      for (int i = 0; i < size; ++i)  {
        if (nack_last_seq_number_sent_ == nack_list[i]) {
          start_id = i + 1;
          break;
        }
      }
      nackLength = size - start_id;
    }
  }
  // Our RTCP NACK implementation is limited to kRtcpMaxNackFields sequence
  // numbers per RTCP packet.
  if (nackLength > kRtcpMaxNackFields) {
    nackLength = kRtcpMaxNackFields;
  }
  nack_last_seq_number_sent_ = nack_list[start_id + nackLength - 1];

  return rtcp_sender_.SendRTCP(
      GetFeedbackState(), kRtcpNack, nackLength, &nack_list[start_id]);
}

// Store the sent packets, needed to answer to a Negative acknowledgment
// requests.
int32_t ModuleRtpRtcpImpl::SetStorePacketsStatus(
    const bool enable,
    const uint16_t number_to_store) {
  rtp_sender_.SetStorePacketsStatus(enable, number_to_store);
  return 0;  // TODO(pwestin): change to void.
}

bool ModuleRtpRtcpImpl::StorePackets() const {
  return rtp_sender_.StorePackets();
}

void ModuleRtpRtcpImpl::RegisterSendChannelRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  rtcp_receiver_.RegisterRtcpStatisticsCallback(callback);
}

RtcpStatisticsCallback* ModuleRtpRtcpImpl::
        GetSendChannelRtcpStatisticsCallback() {
  return rtcp_receiver_.GetRtcpStatisticsCallback();
}

// Send a TelephoneEvent tone using RFC 2833 (4733).
int32_t ModuleRtpRtcpImpl::SendTelephoneEventOutband(
    const uint8_t key,
    const uint16_t time_ms,
    const uint8_t level) {
  return rtp_sender_.SendTelephoneEvent(key, time_ms, level);
}

bool ModuleRtpRtcpImpl::SendTelephoneEventActive(
    int8_t& telephone_event) const {
  return rtp_sender_.SendTelephoneEventActive(&telephone_event);
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
int32_t ModuleRtpRtcpImpl::SendREDPayloadType(
    int8_t& payload_type) const {
  return rtp_sender_.RED(&payload_type);
}

void ModuleRtpRtcpImpl::SetTargetSendBitrate(
    const std::vector<uint32_t>& stream_bitrates) {
  if (IsDefaultModule()) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    if (simulcast_) {
      std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (size_t i = 0;
           it != child_modules_.end() && i < stream_bitrates.size(); ++it) {
        if ((*it)->SendingMedia()) {
          RTPSender& rtp_sender = (*it)->rtp_sender_;
          rtp_sender.SetTargetBitrate(stream_bitrates[i]);
          ++i;
        }
      }
    } else {
      if (stream_bitrates.size() > 1)
        return;
      std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
      for (; it != child_modules_.end(); ++it) {
        RTPSender& rtp_sender = (*it)->rtp_sender_;
        rtp_sender.SetTargetBitrate(stream_bitrates[0]);
      }
    }
  } else {
    if (stream_bitrates.size() > 1)
      return;
    rtp_sender_.SetTargetBitrate(stream_bitrates[0]);
  }
}

int32_t ModuleRtpRtcpImpl::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  key_frame_req_method_ = method;
  return 0;
}

int32_t ModuleRtpRtcpImpl::RequestKeyFrame() {
  switch (key_frame_req_method_) {
    case kKeyFrameReqFirRtp:
      return rtp_sender_.SendRTPIntraRequest();
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

int32_t ModuleRtpRtcpImpl::SetCameraDelay(const int32_t delay_ms) {
  if (IsDefaultModule()) {
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetCameraDelay(delay_ms);
      }
      it++;
    }
    return 0;
  }
  return rtcp_sender_.SetCameraDelay(delay_ms);
}

int32_t ModuleRtpRtcpImpl::SetGenericFECStatus(
    const bool enable,
    const uint8_t payload_type_red,
    const uint8_t payload_type_fec) {
  return rtp_sender_.SetGenericFECStatus(enable,
                                         payload_type_red,
                                         payload_type_fec);
}

int32_t ModuleRtpRtcpImpl::GenericFECStatus(
    bool& enable,
    uint8_t& payload_type_red,
    uint8_t& payload_type_fec) {
  bool child_enabled = false;
  if (IsDefaultModule()) {
    // For default we need to check all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());
    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module)  {
        bool enabled = false;
        uint8_t dummy_ptype_red = 0;
        uint8_t dummy_ptype_fec = 0;
        if (module->GenericFECStatus(enabled,
                                     dummy_ptype_red,
                                     dummy_ptype_fec) == 0 && enabled) {
          child_enabled = true;
          break;
        }
      }
      it++;
    }
  }
  int32_t ret_val = rtp_sender_.GenericFECStatus(&enable,
                                                 &payload_type_red,
                                                 &payload_type_fec);
  if (child_enabled) {
    // Returns true if enabled for any child module.
    enable = child_enabled;
  }
  return ret_val;
}

int32_t ModuleRtpRtcpImpl::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  if (IsDefaultModule())  {
    // For default we need to update all child modules too.
    CriticalSectionScoped lock(critical_section_module_ptrs_.get());

    std::vector<ModuleRtpRtcpImpl*>::iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        module->SetFecParameters(delta_params, key_params);
      }
      it++;
    }
    return 0;
  }
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
    if (kRtcpOff != rtcp_sender_.Status()) {
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
  if (IsDefaultModule()) {
    // For default we need to update the send bitrate.
    CriticalSectionScoped lock(critical_section_module_ptrs_feedback_.get());

    if (total_rate != NULL)
      *total_rate = 0;
    if (video_rate != NULL)
      *video_rate = 0;
    if (fec_rate != NULL)
      *fec_rate = 0;
    if (nack_rate != NULL)
      *nack_rate = 0;

    std::vector<ModuleRtpRtcpImpl*>::const_iterator it = child_modules_.begin();
    while (it != child_modules_.end()) {
      RtpRtcp* module = *it;
      if (module) {
        uint32_t child_total_rate = 0;
        uint32_t child_video_rate = 0;
        uint32_t child_fec_rate = 0;
        uint32_t child_nack_rate = 0;
        module->BitrateSent(&child_total_rate,
                            &child_video_rate,
                            &child_fec_rate,
                            &child_nack_rate);
        if (total_rate != NULL && child_total_rate > *total_rate)
          *total_rate = child_total_rate;
        if (video_rate != NULL && child_video_rate > *video_rate)
          *video_rate = child_video_rate;
        if (fec_rate != NULL && child_fec_rate > *fec_rate)
          *fec_rate = child_fec_rate;
        if (nack_rate != NULL && child_nack_rate > *nack_rate)
          *nack_rate = child_nack_rate;
      }
      it++;
    }
    return;
  }
  if (total_rate != NULL)
    *total_rate = rtp_sender_.BitrateSent();
  if (video_rate != NULL)
    *video_rate = rtp_sender_.VideoBitrateSent();
  if (fec_rate != NULL)
    *fec_rate = rtp_sender_.FecOverheadRate();
  if (nack_rate != NULL)
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

bool ModuleRtpRtcpImpl::GetSendReportMetadata(const uint32_t send_report,
                                              uint32_t *time_of_send,
                                              uint32_t *packet_count,
                                              uint64_t *octet_count) {
  return rtcp_sender_.GetSendReportMetadata(send_report,
                                            time_of_send,
                                            packet_count,
                                            octet_count);
}

bool ModuleRtpRtcpImpl::SendTimeOfXrRrReport(
    uint32_t mid_ntp, int64_t* time_ms) const {
  return rtcp_sender_.SendTimeOfXrRrReport(mid_ntp, time_ms);
}

void ModuleRtpRtcpImpl::OnReceivedNACK(
    const std::list<uint16_t>& nack_sequence_numbers) {
  if (!rtp_sender_.StorePackets() ||
      nack_sequence_numbers.size() == 0) {
    return;
  }
  // Use RTT from RtcpRttStats class if provided.
  uint16_t rtt = rtt_ms();
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
int32_t ModuleRtpRtcpImpl::BoundingSet(bool& tmmbr_owner,
                                       TMMBRSet*& bounding_set) {
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
  int rtx_mode = kRtxOff;
  uint32_t rtx_ssrc = 0;
  int rtx_payload_type = 0;
  rtp_sender_.RTXStatus(&rtx_mode, &rtx_ssrc, &rtx_payload_type);
  if (rtx_mode != kRtxOff)
    ssrcs.insert(rtx_ssrc);
  rtcp_receiver_.SetSsrcs(main_ssrc, ssrcs);
}

void ModuleRtpRtcpImpl::set_rtt_ms(uint32_t rtt_ms) {
  CriticalSectionScoped cs(critical_section_rtt_.get());
  rtt_ms_ = rtt_ms;
}

uint32_t ModuleRtpRtcpImpl::rtt_ms() const {
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

bool ModuleRtpRtcpImpl::IsDefaultModule() const {
  CriticalSectionScoped cs(critical_section_module_ptrs_.get());
  return !child_modules_.empty();
}

}  // Namespace webrtc
