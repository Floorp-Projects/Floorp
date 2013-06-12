/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver.h"

#include <cassert>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

using ModuleRTPUtility::AudioPayload;
using ModuleRTPUtility::GetCurrentRTP;
using ModuleRTPUtility::Payload;
using ModuleRTPUtility::RTPPayloadParser;
using ModuleRTPUtility::StringCompare;
using ModuleRTPUtility::VideoPayload;

RTPReceiver::RTPReceiver(const int32_t id,
                         Clock* clock,
                         ModuleRtpRtcpImpl* owner,
                         RtpAudioFeedback* incoming_audio_messages_callback,
                         RtpData* incoming_payload_callback,
                         RtpFeedback* incoming_messages_callback,
                         RTPReceiverStrategy* rtp_media_receiver,
                         RTPPayloadRegistry* rtp_payload_registry)
    : Bitrate(clock),
      rtp_payload_registry_(rtp_payload_registry),
      rtp_media_receiver_(rtp_media_receiver),
      id_(id),
      rtp_rtcp_(*owner),
      cb_rtp_feedback_(incoming_messages_callback),

      critical_section_rtp_receiver_(
        CriticalSectionWrapper::CreateCriticalSection()),
      last_receive_time_(0),
      last_received_payload_length_(0),

      packet_timeout_ms_(0),

      rtp_header_extension_map_(),
      ssrc_(0),
      num_csrcs_(0),
      current_remote_csrc_(),
      num_energy_(0),
      current_remote_energy_(),
      use_ssrc_filter_(false),
      ssrc_filter_(0),

      jitter_q4_(0),
      jitter_max_q4_(0),
      cumulative_loss_(0),
      jitter_q4_transmission_time_offset_(0),
      local_time_last_received_timestamp_(0),
      last_received_frame_time_ms_(0),
      last_received_timestamp_(0),
      last_received_sequence_number_(0),
      last_received_transmission_time_offset_(0),

      received_seq_first_(0),
      received_seq_max_(0),
      received_seq_wraps_(0),

      received_packet_oh_(12),  // RTP header.
      received_byte_count_(0),
      received_old_packet_count_(0),
      received_inorder_packet_count_(0),

      last_report_inorder_packets_(0),
      last_report_old_packets_(0),
      last_report_seq_max_(0),
      last_report_fraction_lost_(0),
      last_report_cumulative_lost_(0),
      last_report_extended_high_seq_num_(0),
      last_report_jitter_(0),
      last_report_jitter_transmission_time_offset_(0),

      nack_method_(kNackOff),
      max_reordering_threshold_(kDefaultMaxReorderingThreshold),
      rtx_(false),
      ssrc_rtx_(0),
      payload_type_rtx_(-1) {
  assert(incoming_audio_messages_callback &&
         incoming_messages_callback &&
         incoming_payload_callback);

  memset(current_remote_csrc_, 0, sizeof(current_remote_csrc_));
  memset(current_remote_energy_, 0, sizeof(current_remote_energy_));

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPReceiver::~RTPReceiver() {
  for (int i = 0; i < num_csrcs_; ++i) {
    cb_rtp_feedback_->OnIncomingCSRCChanged(id_, current_remote_csrc_[i],
                                            false);
  }
  delete critical_section_rtp_receiver_;
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s deleted", __FUNCTION__);
}

RtpVideoCodecTypes RTPReceiver::VideoCodecType() const {
  ModuleRTPUtility::PayloadUnion media_specific;
  rtp_media_receiver_->GetLastMediaSpecificPayload(&media_specific);
  return media_specific.Video.videoCodecType;
}

uint32_t RTPReceiver::MaxConfiguredBitrate() const {
  ModuleRTPUtility::PayloadUnion media_specific;
  rtp_media_receiver_->GetLastMediaSpecificPayload(&media_specific);
  return media_specific.Video.maxRate;
}

bool RTPReceiver::REDPayloadType(const int8_t payload_type) const {
  return rtp_payload_registry_->red_payload_type() == payload_type;
}

int8_t RTPReceiver::REDPayloadType() const {
  return rtp_payload_registry_->red_payload_type();
}

int32_t RTPReceiver::SetPacketTimeout(const uint32_t timeout_ms) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  packet_timeout_ms_ = timeout_ms;
  return 0;
}

bool RTPReceiver::HaveNotReceivedPackets() const {
  return last_receive_time_ == 0;
}

void RTPReceiver::PacketTimeout() {
  bool packet_time_out = false;
  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);
    if (packet_timeout_ms_ == 0) {
      // Not configured.
      return;
    }

    if (HaveNotReceivedPackets()) {
      // Not active.
      return;
    }

    int64_t now = clock_->TimeInMilliseconds();

    if (now - last_receive_time_ > packet_timeout_ms_) {
      packet_time_out = true;
      last_receive_time_ = 0;            // Only one callback.
      rtp_payload_registry_->ResetLastReceivedPayloadTypes();
    }
  }
  if (packet_time_out) {
    cb_rtp_feedback_->OnPacketTimeout(id_);
  }
}

void RTPReceiver::ProcessDeadOrAlive(const bool rtcp_alive,
                                     const int64_t now) {
  RTPAliveType alive = kRtpDead;

  if (last_receive_time_ + 1000 > now) {
    // Always alive if we have received a RTP packet the last second.
    alive = kRtpAlive;

  } else {
    if (rtcp_alive) {
      alive = rtp_media_receiver_->ProcessDeadOrAlive(
          last_received_payload_length_);
    } else {
      // No RTP packet for 1 sec and no RTCP: dead.
    }
  }

  cb_rtp_feedback_->OnPeriodicDeadOrAlive(id_, alive);
}

uint16_t RTPReceiver::PacketOHReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_packet_oh_;
}

uint32_t RTPReceiver::PacketCountReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_inorder_packet_count_;
}

uint32_t RTPReceiver::ByteCountReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_byte_count_;
}

int32_t RTPReceiver::RegisterReceivePayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_type,
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  // TODO(phoglund): Try to streamline handling of the RED codec and some other
  // cases which makes it necessary to keep track of whether we created a
  // payload or not.
  bool created_new_payload = false;
  int32_t result = rtp_payload_registry_->RegisterReceivePayload(
      payload_name, payload_type, frequency, channels, rate,
      &created_new_payload);
  if (created_new_payload) {
    if (rtp_media_receiver_->OnNewPayloadTypeCreated(payload_name, payload_type,
                                                     frequency) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                   "%s failed to register payload",
                   __FUNCTION__);
      return -1;
    }
  }
  return result;
}

int32_t RTPReceiver::DeRegisterReceivePayload(
    const int8_t payload_type) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return rtp_payload_registry_->DeRegisterReceivePayload(payload_type);
}

int32_t RTPReceiver::ReceivePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate,
    int8_t* payload_type) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return rtp_payload_registry_->ReceivePayloadType(
      payload_name, frequency, channels, rate, payload_type);
}

int32_t RTPReceiver::RegisterRtpHeaderExtension(
    const RTPExtensionType type,
    const uint8_t id) {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);
  return rtp_header_extension_map_.Register(type, id);
}

int32_t RTPReceiver::DeregisterRtpHeaderExtension(
    const RTPExtensionType type) {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);
  return rtp_header_extension_map_.Deregister(type);
}

void RTPReceiver::GetHeaderExtensionMapCopy(RtpHeaderExtensionMap* map) const {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);
  rtp_header_extension_map_.GetCopy(map);
}

NACKMethod RTPReceiver::NACK() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return nack_method_;
}

// Turn negative acknowledgment requests on/off.
int32_t RTPReceiver::SetNACKStatus(const NACKMethod method,
                                   int max_reordering_threshold) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  if (max_reordering_threshold < 0) {
    return -1;
  } else if (method == kNackRtcp) {
    max_reordering_threshold_ = max_reordering_threshold;
  } else {
    max_reordering_threshold_ = kDefaultMaxReorderingThreshold;
  }
  nack_method_ = method;
  return 0;
}

void RTPReceiver::SetRTXStatus(bool enable, uint32_t ssrc) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  rtx_ = enable;
  ssrc_rtx_ = ssrc;
}

void RTPReceiver::RTXStatus(bool* enable, uint32_t* ssrc,
                            int* payload_type) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  *enable = rtx_;
  *ssrc = ssrc_rtx_;
  *payload_type = payload_type_rtx_;
}

void RTPReceiver::SetRtxPayloadType(int payload_type) {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);
  payload_type_rtx_ = payload_type;
}

uint32_t RTPReceiver::SSRC() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return ssrc_;
}

// Get remote CSRC.
int32_t RTPReceiver::CSRCs(
    uint32_t array_of_csrcs[kRtpCsrcSize]) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  assert(num_csrcs_ <= kRtpCsrcSize);

  if (num_csrcs_ > 0) {
    memcpy(array_of_csrcs, current_remote_csrc_,
           sizeof(uint32_t)*num_csrcs_);
  }
  return num_csrcs_;
}

int32_t RTPReceiver::Energy(
    uint8_t array_of_energy[kRtpCsrcSize]) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  assert(num_energy_ <= kRtpCsrcSize);

  if (num_energy_ > 0) {
    memcpy(array_of_energy, current_remote_energy_,
           sizeof(uint8_t)*num_csrcs_);
  }
  return num_energy_;
}

int32_t RTPReceiver::IncomingRTPPacket(
  WebRtcRTPHeader* rtp_header,
  const uint8_t* packet,
  const uint16_t packet_length) {
  TRACE_EVENT0("webrtc_rtp", "RTPRecv::Packet");
  // The rtp_header argument contains the parsed RTP header.
  int length = packet_length - rtp_header->header.paddingLength;

  // Sanity check.
  if ((length - rtp_header->header.headerLength) < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument",
                 __FUNCTION__);
    return -1;
  }
  if (rtx_) {
    if (ssrc_rtx_ == rtp_header->header.ssrc) {
      // Sanity check, RTX packets has 2 extra header bytes.
      if (rtp_header->header.headerLength + kRtxHeaderSize > packet_length) {
        return -1;
      }
      // If a specific RTX payload type is negotiated, set back to the media
      // payload type and treat it like a media packet from here.
      if (payload_type_rtx_ != -1) {
        if (payload_type_rtx_ == rtp_header->header.payloadType &&
            rtp_payload_registry_->last_received_media_payload_type() != -1) {
          rtp_header->header.payloadType =
              rtp_payload_registry_->last_received_media_payload_type();
        } else {
          WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                       "Incorrect RTX configuration, dropping packet.");
          return -1;
        }
      }
      rtp_header->header.ssrc = ssrc_;
      rtp_header->header.sequenceNumber =
        (packet[rtp_header->header.headerLength] << 8) +
        packet[1 + rtp_header->header.headerLength];
      // Count the RTX header as part of the RTP header.
      rtp_header->header.headerLength += 2;
    }
  }
  if (use_ssrc_filter_) {
    if (rtp_header->header.ssrc != ssrc_filter_) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "%s drop packet due to SSRC filter",
                   __FUNCTION__);
      return -1;
    }
  }
  if (last_receive_time_ == 0) {
    // Trigger only once.
    if (length - rtp_header->header.headerLength == 0) {
      // Keep-alive packet.
      cb_rtp_feedback_->OnReceivedPacket(id_, kPacketKeepAlive);
    } else {
      cb_rtp_feedback_->OnReceivedPacket(id_, kPacketRtp);
    }
  }
  int8_t first_payload_byte = 0;
  if (length > 0) {
    first_payload_byte = packet[rtp_header->header.headerLength];
  }
  // Trigger our callbacks.
  CheckSSRCChanged(rtp_header);

  bool is_red = false;
  ModuleRTPUtility::PayloadUnion specific_payload = {};

  if (CheckPayloadChanged(rtp_header,
                          first_payload_byte,
                          is_red,
                          &specific_payload) == -1) {
    if (length - rtp_header->header.headerLength == 0) {
      // OK, keep-alive packet.
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                   "%s received keepalive",
                   __FUNCTION__);
      return 0;
    }
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "%s received invalid payloadtype",
                 __FUNCTION__);
    return -1;
  }
  CheckCSRC(rtp_header);

  uint16_t payload_data_length =
    ModuleRTPUtility::GetPayloadDataLength(rtp_header, packet_length);

  bool is_first_packet_in_frame =
      SequenceNumber() + 1 == rtp_header->header.sequenceNumber &&
      TimeStamp() != rtp_header->header.timestamp;
  bool is_first_packet = is_first_packet_in_frame || HaveNotReceivedPackets();

  int32_t ret_val = rtp_media_receiver_->ParseRtpPacket(
      rtp_header, specific_payload, is_red, packet, packet_length,
      clock_->TimeInMilliseconds(), is_first_packet);

  if (ret_val < 0) {
    return ret_val;
  }

  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  // This compares to received_seq_max_. We store the last received after we
  // have done the callback.
  bool old_packet = RetransmitOfOldPacket(rtp_header->header.sequenceNumber,
                                          rtp_header->header.timestamp);

  // This updates received_seq_max_ and other members.
  UpdateStatistics(rtp_header, payload_data_length, old_packet);

  // Need to be updated after RetransmitOfOldPacket and
  // RetransmitOfOldPacketUpdateStatistics.
  last_receive_time_ = clock_->TimeInMilliseconds();
  last_received_payload_length_ = payload_data_length;

  if (!old_packet) {
    if (last_received_timestamp_ != rtp_header->header.timestamp) {
      last_received_timestamp_ = rtp_header->header.timestamp;
      last_received_frame_time_ms_ = clock_->TimeInMilliseconds();
    }
    last_received_sequence_number_ = rtp_header->header.sequenceNumber;
    last_received_transmission_time_offset_ =
      rtp_header->extension.transmissionTimeOffset;
  }
  return ret_val;
}

// Implementation note: we expect to have the critical_section_rtp_receiver_
// critsect when we call this.
void RTPReceiver::UpdateStatistics(const WebRtcRTPHeader* rtp_header,
                                   const uint16_t bytes,
                                   const bool old_packet) {
  uint32_t frequency_hz = rtp_media_receiver_->GetFrequencyHz();

  Bitrate::Update(bytes);

  received_byte_count_ += bytes;

  if (received_seq_max_ == 0 && received_seq_wraps_ == 0) {
    // This is the first received report.
    received_seq_first_ = rtp_header->header.sequenceNumber;
    received_seq_max_ = rtp_header->header.sequenceNumber;
    received_inorder_packet_count_ = 1;
    local_time_last_received_timestamp_ =
      GetCurrentRTP(clock_, frequency_hz);  // Time in samples.
    return;
  }

  // Count only the new packets received.
  if (InOrderPacket(rtp_header->header.sequenceNumber)) {
    const uint32_t RTPtime =
      GetCurrentRTP(clock_, frequency_hz);  // Time in samples.
    received_inorder_packet_count_++;

    // Wrong if we use RetransmitOfOldPacket.
    int32_t seq_diff =
        rtp_header->header.sequenceNumber - received_seq_max_;
    if (seq_diff < 0) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // new max
    received_seq_max_ = rtp_header->header.sequenceNumber;

    if (rtp_header->header.timestamp != last_received_timestamp_ &&
        received_inorder_packet_count_ > 1) {
      int32_t time_diff_samples =
          (RTPtime - local_time_last_received_timestamp_) -
          (rtp_header->header.timestamp - last_received_timestamp_);

      time_diff_samples = abs(time_diff_samples);

      // lib_jingle sometimes deliver crazy jumps in TS for the same stream.
      // If this happens, don't update jitter value. Use 5 secs video frequency
      // as the treshold.
      if (time_diff_samples < 450000) {
        // Note we calculate in Q4 to avoid using float.
        int32_t jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
        jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
      }

      // Extended jitter report, RFC 5450.
      // Actual network jitter, excluding the source-introduced jitter.
      int32_t time_diff_samples_ext =
        (RTPtime - local_time_last_received_timestamp_) -
        ((rtp_header->header.timestamp +
          rtp_header->extension.transmissionTimeOffset) -
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
    local_time_last_received_timestamp_ = RTPtime;
  } else {
    if (old_packet) {
      received_old_packet_count_++;
    } else {
      received_inorder_packet_count_++;
    }
  }

  uint16_t packet_oh =
      rtp_header->header.headerLength + rtp_header->header.paddingLength;

  // Our measured overhead. Filter from RFC 5104 4.2.1.2:
  // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
  received_packet_oh_ = (15 * received_packet_oh_ + packet_oh) >> 4;
}

// Implementation note: we expect to have the critical_section_rtp_receiver_
// critsect when we call this.
bool RTPReceiver::RetransmitOfOldPacket(
    const uint16_t sequence_number,
    const uint32_t rtp_time_stamp) const {
  if (InOrderPacket(sequence_number)) {
    return false;
  }

  uint32_t frequency_khz = rtp_media_receiver_->GetFrequencyHz() / 1000;
  int64_t time_diff_ms = clock_->TimeInMilliseconds() -
      last_receive_time_;

  // Diff in time stamp since last received in order.
  int32_t rtp_time_stamp_diff_ms =
      static_cast<int32_t>(rtp_time_stamp - last_received_timestamp_) /
      frequency_khz;

  uint16_t min_rtt = 0;
  int32_t max_delay_ms = 0;
  rtp_rtcp_.RTT(ssrc_, NULL, NULL, &min_rtt, NULL);
  if (min_rtt == 0) {
    // Jitter variance in samples.
    float jitter = jitter_q4_ >> 4;

    // Jitter standard deviation in samples.
    float jitter_std = sqrt(jitter);

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
  if (time_diff_ms > rtp_time_stamp_diff_ms + max_delay_ms) {
    return true;
  }
  return false;
}

bool RTPReceiver::InOrderPacket(const uint16_t sequence_number) const {
  if (IsNewerSequenceNumber(sequence_number, received_seq_max_)) {
    return true;
  } else {
    // If we have a restart of the remote side this packet is still in order.
    return !IsNewerSequenceNumber(sequence_number, received_seq_max_ -
                                  max_reordering_threshold_);
  }
}

uint16_t RTPReceiver::SequenceNumber() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_sequence_number_;
}

uint32_t RTPReceiver::TimeStamp() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_timestamp_;
}

int32_t RTPReceiver::LastReceivedTimeMs() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_frame_time_ms_;
}

// Compute time stamp of the last incoming packet that is the first packet of
// its frame.
int32_t RTPReceiver::EstimatedRemoteTimeStamp(
    uint32_t& timestamp) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  uint32_t frequency_hz = rtp_media_receiver_->GetFrequencyHz();

  if (local_time_last_received_timestamp_ == 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "%s invalid state", __FUNCTION__);
    return -1;
  }
  // Time in samples.
  uint32_t diff = GetCurrentRTP(clock_, frequency_hz) -
                        local_time_last_received_timestamp_;

  timestamp = last_received_timestamp_ + diff;
  return 0;
}

// Get the currently configured SSRC filter.
int32_t RTPReceiver::SSRCFilter(uint32_t& allowed_ssrc) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  if (use_ssrc_filter_) {
    allowed_ssrc = ssrc_filter_;
    return 0;
  }
  WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
               "%s invalid state", __FUNCTION__);
  return -1;
}

// Set a SSRC to be used as a filter for incoming RTP streams.
int32_t RTPReceiver::SetSSRCFilter(
    const bool enable, const uint32_t allowed_ssrc) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  use_ssrc_filter_ = enable;
  if (enable) {
    ssrc_filter_ = allowed_ssrc;
  } else {
    ssrc_filter_ = 0;
  }
  return 0;
}

// Implementation note: must not hold critsect when called.
void RTPReceiver::CheckSSRCChanged(const WebRtcRTPHeader* rtp_header) {
  bool new_ssrc = false;
  bool re_initialize_decoder = false;
  char payload_name[RTP_PAYLOAD_NAME_SIZE];
  uint32_t frequency = kDefaultVideoFrequency;
  uint8_t channels = 1;
  uint32_t rate = 0;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    int8_t last_received_payload_type =
        rtp_payload_registry_->last_received_payload_type();
    if (ssrc_ != rtp_header->header.ssrc ||
        (last_received_payload_type == -1 && ssrc_ == 0)) {
      // We need the payload_type_ to make the call if the remote SSRC is 0.
      new_ssrc = true;

      ResetStatistics();

      last_received_timestamp_      = 0;
      last_received_sequence_number_ = 0;
      last_received_transmission_time_offset_ = 0;
      last_received_frame_time_ms_ = 0;

      // Do we have a SSRC? Then the stream is restarted.
      if (ssrc_) {
        // Do we have the same codec? Then re-initialize coder.
        if (rtp_header->header.payloadType == last_received_payload_type) {
          re_initialize_decoder = true;

          Payload* payload;
          if (rtp_payload_registry_->PayloadTypeToPayload(
              rtp_header->header.payloadType, payload) != 0) {
            return;
          }
          assert(payload);
          payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
          strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
          if (payload->audio) {
            frequency = payload->typeSpecific.Audio.frequency;
            channels = payload->typeSpecific.Audio.channels;
            rate = payload->typeSpecific.Audio.rate;
          } else {
            frequency = kDefaultVideoFrequency;
          }
        }
      }
      ssrc_ = rtp_header->header.ssrc;
    }
  }
  if (new_ssrc) {
    // We need to get this to our RTCP sender and receiver.
    // We need to do this outside critical section.
    rtp_rtcp_.SetRemoteSSRC(rtp_header->header.ssrc);
    cb_rtp_feedback_->OnIncomingSSRCChanged(id_, rtp_header->header.ssrc);
  }
  if (re_initialize_decoder) {
    if (-1 == cb_rtp_feedback_->OnInitializeDecoder(
        id_, rtp_header->header.payloadType, payload_name, frequency,
        channels, rate)) {
      // New stream, same codec.
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                   "Failed to create decoder for payload type:%d",
                   rtp_header->header.payloadType);
    }
  }
}

// Implementation note: must not hold critsect when called.
// TODO(phoglund): Move as much as possible of this code path into the media
// specific receivers. Basically this method goes through a lot of trouble to
// compute something which is only used by the media specific parts later. If
// this code path moves we can get rid of some of the rtp_receiver ->
// media_specific interface (such as CheckPayloadChange, possibly get/set
// last known payload).
int32_t RTPReceiver::CheckPayloadChanged(
  const WebRtcRTPHeader* rtp_header,
  const int8_t first_payload_byte,
  bool& is_red,
  ModuleRTPUtility::PayloadUnion* specific_payload) {
  bool re_initialize_decoder = false;

  char payload_name[RTP_PAYLOAD_NAME_SIZE];
  int8_t payload_type = rtp_header->header.payloadType;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    int8_t last_received_payload_type =
        rtp_payload_registry_->last_received_payload_type();
    if (payload_type != last_received_payload_type) {
      if (REDPayloadType(payload_type)) {
        // Get the real codec payload type.
        payload_type = first_payload_byte & 0x7f;
        is_red = true;

        if (REDPayloadType(payload_type)) {
          // Invalid payload type, traced by caller. If we proceeded here,
          // this would be set as |_last_received_payload_type|, and we would no
          // longer catch corrupt packets at this level.
          return -1;
        }

        // When we receive RED we need to check the real payload type.
        if (payload_type == last_received_payload_type) {
          rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);
          return 0;
        }
      }
      bool should_reset_statistics = false;
      bool should_discard_changes = false;

      rtp_media_receiver_->CheckPayloadChanged(
        payload_type, specific_payload, &should_reset_statistics,
        &should_discard_changes);

      if (should_reset_statistics) {
        ResetStatistics();
      }
      if (should_discard_changes) {
        is_red = false;
        return 0;
      }

      Payload* payload;
      if (rtp_payload_registry_->PayloadTypeToPayload(payload_type,
                                                      payload) != 0) {
        // Not a registered payload type.
        return -1;
      }
      assert(payload);
      payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
      strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);

      rtp_payload_registry_->set_last_received_payload_type(payload_type);

      re_initialize_decoder = true;

      rtp_media_receiver_->SetLastMediaSpecificPayload(payload->typeSpecific);
      rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);

      if (!payload->audio) {
        if (VideoCodecType() == kRtpFecVideo) {
          // Only reset the decoder on media packets.
          re_initialize_decoder = false;
        } else {
          bool media_type_unchanged =
              rtp_payload_registry_->ReportMediaPayloadType(payload_type);
          if (media_type_unchanged) {
            // Only reset the decoder if the media codec type has changed.
            re_initialize_decoder = false;
          }
        }
      }
      if (re_initialize_decoder) {
        ResetStatistics();
      }
    } else {
      rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);
      is_red = false;
    }
  }   // End critsect.

  if (re_initialize_decoder) {
    if (-1 == rtp_media_receiver_->InvokeOnInitializeDecoder(
        cb_rtp_feedback_, id_, payload_type, payload_name,
        *specific_payload)) {
      return -1;  // Wrong payload type.
    }
  }
  return 0;
}

// Implementation note: must not hold critsect when called.
void RTPReceiver::CheckCSRC(const WebRtcRTPHeader* rtp_header) {
  int32_t num_csrcs_diff = 0;
  uint32_t old_remote_csrc[kRtpCsrcSize];
  uint8_t old_num_csrcs = 0;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    if (!rtp_media_receiver_->ShouldReportCsrcChanges(
        rtp_header->header.payloadType)) {
      return;
    }
    num_energy_ = rtp_header->type.Audio.numEnergy;
    if (rtp_header->type.Audio.numEnergy > 0 &&
        rtp_header->type.Audio.numEnergy <= kRtpCsrcSize) {
      memcpy(current_remote_energy_,
             rtp_header->type.Audio.arrOfEnergy,
             rtp_header->type.Audio.numEnergy);
    }
    old_num_csrcs  = num_csrcs_;
    if (old_num_csrcs > 0) {
      // Make a copy of old.
      memcpy(old_remote_csrc, current_remote_csrc_,
             num_csrcs_ * sizeof(uint32_t));
    }
    const uint8_t num_csrcs = rtp_header->header.numCSRCs;
    if ((num_csrcs > 0) && (num_csrcs <= kRtpCsrcSize)) {
      // Copy new.
      memcpy(current_remote_csrc_,
             rtp_header->header.arrOfCSRCs,
             num_csrcs * sizeof(uint32_t));
    }
    if (num_csrcs > 0 || old_num_csrcs > 0) {
      num_csrcs_diff = num_csrcs - old_num_csrcs;
      num_csrcs_ = num_csrcs;  // Update stored CSRCs.
    } else {
      // No change.
      return;
    }
  }  // End critsect.

  bool have_called_callback = false;
  // Search for new CSRC in old array.
  for (uint8_t i = 0; i < rtp_header->header.numCSRCs; ++i) {
    const uint32_t csrc = rtp_header->header.arrOfCSRCs[i];

    bool found_match = false;
    for (uint8_t j = 0; j < old_num_csrcs; ++j) {
      if (csrc == old_remote_csrc[j]) {  // old list
        found_match = true;
        break;
      }
    }
    if (!found_match && csrc) {
      // Didn't find it, report it as new.
      have_called_callback = true;
      cb_rtp_feedback_->OnIncomingCSRCChanged(id_, csrc, true);
    }
  }
  // Search for old CSRC in new array.
  for (uint8_t i = 0; i < old_num_csrcs; ++i) {
    const uint32_t csrc = old_remote_csrc[i];

    bool found_match = false;
    for (uint8_t j = 0; j < rtp_header->header.numCSRCs; ++j) {
      if (csrc == rtp_header->header.arrOfCSRCs[j]) {
        found_match = true;
        break;
      }
    }
    if (!found_match && csrc) {
      // Did not find it, report as removed.
      have_called_callback = true;
      cb_rtp_feedback_->OnIncomingCSRCChanged(id_, csrc, false);
    }
  }
  if (!have_called_callback) {
    // If the CSRC list contain non-unique entries we will end up here.
    // Using CSRC 0 to signal this event, not interop safe, other
    // implementations might have CSRC 0 as a valid value.
    if (num_csrcs_diff > 0) {
      cb_rtp_feedback_->OnIncomingCSRCChanged(id_, 0, true);
    } else if (num_csrcs_diff < 0) {
      cb_rtp_feedback_->OnIncomingCSRCChanged(id_, 0, false);
    }
  }
}

int32_t RTPReceiver::ResetStatistics() {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  last_report_inorder_packets_ = 0;
  last_report_old_packets_ = 0;
  last_report_seq_max_ = 0;
  last_report_fraction_lost_ = 0;
  last_report_cumulative_lost_ = 0;
  last_report_extended_high_seq_num_ = 0;
  last_report_jitter_ = 0;
  last_report_jitter_transmission_time_offset_ = 0;
  jitter_q4_ = 0;
  jitter_max_q4_ = 0;
  cumulative_loss_ = 0;
  jitter_q4_transmission_time_offset_ = 0;
  received_seq_wraps_ = 0;
  received_seq_max_ = 0;
  received_seq_first_ = 0;
  received_byte_count_ = 0;
  received_old_packet_count_ = 0;
  received_inorder_packet_count_ = 0;
  return 0;
}

int32_t RTPReceiver::ResetDataCounters() {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  received_byte_count_ = 0;
  received_old_packet_count_ = 0;
  received_inorder_packet_count_ = 0;
  last_report_inorder_packets_ = 0;

  return 0;
}

int32_t RTPReceiver::Statistics(
    uint8_t*  fraction_lost,
    uint32_t* cum_lost,
    uint32_t* ext_max,
    uint32_t* jitter,
    uint32_t* max_jitter,
    uint32_t* jitter_transmission_time_offset,
    bool reset) const {
  int32_t missing;
  return Statistics(fraction_lost,
                    cum_lost,
                    ext_max,
                    jitter,
                    max_jitter,
                    jitter_transmission_time_offset,
                    &missing,
                    reset);
}

int32_t RTPReceiver::Statistics(
    uint8_t*  fraction_lost,
    uint32_t* cum_lost,
    uint32_t* ext_max,
    uint32_t* jitter,
    uint32_t* max_jitter,
    uint32_t* jitter_transmission_time_offset,
    int32_t*  missing,
    bool reset) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  if (missing == NULL) {
    return -1;
  }
  if (received_seq_first_ == 0 && received_byte_count_ == 0) {
    // We have not received anything. -1 required by RTCP sender.
    return -1;
  }
  if (!reset) {
    if (last_report_inorder_packets_ == 0) {
      // No report.
      return -1;
    }
    // Just get last report.
    if (fraction_lost) {
      *fraction_lost = last_report_fraction_lost_;
    }
    if (cum_lost) {
      *cum_lost = last_report_cumulative_lost_;  // 24 bits valid.
    }
    if (ext_max) {
      *ext_max = last_report_extended_high_seq_num_;
    }
    if (jitter) {
      *jitter = last_report_jitter_;
    }
    if (max_jitter) {
      // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
      *max_jitter = (jitter_max_q4_ >> 4);
    }
    if (jitter_transmission_time_offset) {
      *jitter_transmission_time_offset =
        last_report_jitter_transmission_time_offset_;
    }
    return 0;
  }

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
      received_inorder_packet_count_ - last_report_inorder_packets_;

  if (nack_method_ == kNackOff) {
    // This is needed for re-ordered packets.
    uint32_t old_packets =
        received_old_packet_count_ - last_report_old_packets_;
    rec_since_last += old_packets;
  } else {
    // With NACK we don't know the expected retransmitions during the last
    // second. We know how many "old" packets we have received. We just count
    // the number of old received to estimate the loss, but it still does not
    // guarantee an exact number since we run this based on time triggered by
    // sending of a RTP packet. This should have a minimum effect.

    // With NACK we don't count old packets as received since they are
    // re-transmitted. We use RTT to decide if a packet is re-ordered or
    // re-transmitted.
  }

  *missing = 0;
  if (exp_since_last > rec_since_last) {
    *missing = (exp_since_last - rec_since_last);
  }
  uint8_t local_fraction_lost = 0;
  if (exp_since_last) {
    // Scale 0 to 255, where 255 is 100% loss.
    local_fraction_lost = (uint8_t)((255 * (*missing)) / exp_since_last);
  }
  if (fraction_lost) {
    *fraction_lost = local_fraction_lost;
  }

  // We need a counter for cumulative loss too.
  cumulative_loss_ += *missing;

  if (jitter_q4_ > jitter_max_q4_) {
    jitter_max_q4_ = jitter_q4_;
  }
  if (cum_lost) {
    *cum_lost =  cumulative_loss_;
  }
  if (ext_max) {
    *ext_max = (received_seq_wraps_ << 16) + received_seq_max_;
  }
  // Note: internal jitter value is in Q4 and needs to be scaled by 1/16.
  if (jitter) {
    *jitter = (jitter_q4_ >> 4);
  }
  if (max_jitter) {
    *max_jitter = (jitter_max_q4_ >> 4);
  }
  if (jitter_transmission_time_offset) {
    *jitter_transmission_time_offset =
      (jitter_q4_transmission_time_offset_ >> 4);
  }
  if (reset) {
    // Store this report.
    last_report_fraction_lost_ = local_fraction_lost;
    last_report_cumulative_lost_ = cumulative_loss_;  // 24 bits valid.
    last_report_extended_high_seq_num_ =
        (received_seq_wraps_ << 16) + received_seq_max_;
    last_report_jitter_  = (jitter_q4_ >> 4);
    last_report_jitter_transmission_time_offset_ =
      (jitter_q4_transmission_time_offset_ >> 4);

    // Only for report blocks in RTCP SR and RR.
    last_report_inorder_packets_ = received_inorder_packet_count_;
    last_report_old_packets_ = received_old_packet_count_;
    last_report_seq_max_ = received_seq_max_;
  }
  return 0;
}

int32_t RTPReceiver::DataCounters(
    uint32_t* bytes_received,
    uint32_t* packets_received) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  if (bytes_received) {
    *bytes_received = received_byte_count_;
  }
  if (packets_received) {
    *packets_received =
        received_old_packet_count_ + received_inorder_packet_count_;
  }
  return 0;
}

void RTPReceiver::ProcessBitrate() {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);

  Bitrate::Process();
  TRACE_COUNTER_ID1("webrtc_rtp",
                    "RTPReceiverBitrate", ssrc_, BitrateLast());
  TRACE_COUNTER_ID1("webrtc_rtp",
                    "RTPReceiverPacketRate", ssrc_, PacketRate());
}

} // namespace webrtc
