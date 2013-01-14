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

namespace webrtc {

using ModuleRTPUtility::AudioPayload;
using ModuleRTPUtility::GetCurrentRTP;
using ModuleRTPUtility::Payload;
using ModuleRTPUtility::RTPPayloadParser;
using ModuleRTPUtility::StringCompare;
using ModuleRTPUtility::VideoPayload;

RTPReceiver::RTPReceiver(const WebRtc_Word32 id,
                         const bool audio,
                         RtpRtcpClock* clock,
                         ModuleRtpRtcpImpl* owner,
                         RtpAudioFeedback* incoming_messages_callback)
    : Bitrate(clock),
      id_(id),
      rtp_rtcp_(*owner),
      critical_section_cbs_(CriticalSectionWrapper::CreateCriticalSection()),
      cb_rtp_feedback_(NULL),
      cb_rtp_data_(NULL),

      critical_section_rtp_receiver_(
        CriticalSectionWrapper::CreateCriticalSection()),
      last_receive_time_(0),
      last_received_payload_length_(0),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1),

      packet_timeout_ms_(0),

      red_payload_type_(-1),
      payload_type_map_(),
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
      rtx_(false),
      ssrc_rtx_(0) {
  // TODO(phoglund): Remove hacks requiring direct access to the audio receiver
  // and only instantiate one of these directly into the rtp_media_receiver_
  // field. Right now an audio receiver carries around a video handler and
  // vice versa, which doesn't make sense.
  rtp_receiver_audio_ = new RTPReceiverAudio(id, this,
                                             incoming_messages_callback);
  rtp_receiver_video_ = new RTPReceiverVideo(id, this, owner);

  if (audio) {
    rtp_media_receiver_ = rtp_receiver_audio_;
  } else {
    rtp_media_receiver_ = rtp_receiver_video_;
  }

  memset(current_remote_csrc_, 0, sizeof(current_remote_csrc_));
  memset(current_remote_energy_, 0, sizeof(current_remote_energy_));

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPReceiver::~RTPReceiver() {
  if (cb_rtp_feedback_) {
    for (int i = 0; i < num_csrcs_; ++i) {
      cb_rtp_feedback_->OnIncomingCSRCChanged(id_, current_remote_csrc_[i],
                                              false);
    }
  }
  delete critical_section_cbs_;
  delete critical_section_rtp_receiver_;

  while (!payload_type_map_.empty()) {
    std::map<WebRtc_Word8, Payload*>::iterator it = payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
  delete rtp_receiver_video_;
  delete rtp_receiver_audio_;
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s deleted", __FUNCTION__);
}

RtpVideoCodecTypes RTPReceiver::VideoCodecType() const {
  ModuleRTPUtility::PayloadUnion media_specific;
  rtp_media_receiver_->GetLastMediaSpecificPayload(&media_specific);
  return media_specific.Video.videoCodecType;
}

WebRtc_UWord32 RTPReceiver::MaxConfiguredBitrate() const {
  ModuleRTPUtility::PayloadUnion media_specific;
  rtp_media_receiver_->GetLastMediaSpecificPayload(&media_specific);
  return media_specific.Video.maxRate;
}

bool RTPReceiver::REDPayloadType(const WebRtc_Word8 payload_type) const {
  return (red_payload_type_ == payload_type) ? true : false;
}

WebRtc_Word8 RTPReceiver::REDPayloadType() const {
  return red_payload_type_;
}

WebRtc_Word32 RTPReceiver::SetPacketTimeout(const WebRtc_UWord32 timeout_ms) {
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

    WebRtc_Word64 now = clock_.GetTimeInMS();

    if (now - last_receive_time_ > packet_timeout_ms_) {
      packet_time_out = true;
      last_receive_time_ = 0;            // Only one callback.
      last_received_payload_type_ = -1;  // Makes RemotePayload return -1.
      last_received_media_payload_type_ = -1;
    }
  }
  CriticalSectionScoped lock(critical_section_cbs_);
  if (packet_time_out && cb_rtp_feedback_) {
    cb_rtp_feedback_->OnPacketTimeout(id_);
  }
}

void RTPReceiver::ProcessDeadOrAlive(const bool rtcp_alive,
                                     const WebRtc_Word64 now) {
  if (cb_rtp_feedback_ == NULL) {
    // No callback.
    return;
  }
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

  CriticalSectionScoped lock(critical_section_cbs_);
  if (cb_rtp_feedback_) {
    cb_rtp_feedback_->OnPeriodicDeadOrAlive(id_, alive);
  }
}

WebRtc_UWord16 RTPReceiver::PacketOHReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_packet_oh_;
}

WebRtc_UWord32 RTPReceiver::PacketCountReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_inorder_packet_count_;
}

WebRtc_UWord32 RTPReceiver::ByteCountReceived() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return received_byte_count_;
}

WebRtc_Word32 RTPReceiver::RegisterIncomingRTPCallback(
    RtpFeedback* incoming_messages_callback) {
  CriticalSectionScoped lock(critical_section_cbs_);
  cb_rtp_feedback_ = incoming_messages_callback;
  return 0;
}

WebRtc_Word32 RTPReceiver::RegisterIncomingDataCallback(
    RtpData* incoming_data_callback) {
  CriticalSectionScoped lock(critical_section_cbs_);
  cb_rtp_data_ = incoming_data_callback;
  return 0;
}

WebRtc_Word32 RTPReceiver::RegisterReceivePayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payload_type,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  assert(payload_name);
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  // Sanity check.
  switch (payload_type) {
    // Reserved payload types to avoid RTCP conflicts when marker bit is set.
    case 64:        //  192 Full INTRA-frame request.
    case 72:        //  200 Sender report.
    case 73:        //  201 Receiver report.
    case 74:        //  202 Source description.
    case 75:        //  203 Goodbye.
    case 76:        //  204 Application-defined.
    case 77:        //  205 Transport layer FB message.
    case 78:        //  206 Payload-specific FB message.
    case 79:        //  207 Extended report.
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                   "%s invalid payloadtype:%d",
                   __FUNCTION__, payload_type);
      return -1;
    default:
      break;
  }
  size_t payload_name_length = strlen(payload_name);

  std::map<WebRtc_Word8, Payload*>::iterator it =
    payload_type_map_.find(payload_type);

  if (it != payload_type_map_.end()) {
    // We already use this payload type.
    Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);

    // Check if it's the same as we already have.
    // If same, ignore sending an error.
    if (payload_name_length == name_length &&
        StringCompare(payload->name, payload_name, payload_name_length)) {
      if (rtp_media_receiver_->PayloadIsCompatible(*payload, frequency,
                                                   channels, rate)) {
        rtp_media_receiver_->UpdatePayloadRate(payload, rate);
        return 0;
      }
    }
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument payload_type:%d already registered",
                 __FUNCTION__, payload_type);
    return -1;
  }

  rtp_media_receiver_->PossiblyRemoveExistingPayloadType(
    &payload_type_map_, payload_name, payload_name_length, frequency, channels,
    rate);

  Payload* payload = NULL;

  // Save the RED payload type. Used in both audio and video.
  if (StringCompare(payload_name, "red", 3)) {
    red_payload_type_ = payload_type;
    payload = new Payload;
    payload->audio = false;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1);
  } else {
    payload = rtp_media_receiver_->CreatePayloadType(
        payload_name, payload_type, frequency, channels, rate);
  }
  if (payload == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s failed to register payload",
                 __FUNCTION__);
    return -1;
  }
  payload_type_map_[payload_type] = payload;

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

WebRtc_Word32 RTPReceiver::DeRegisterReceivePayload(
    const WebRtc_Word8 payload_type) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  std::map<WebRtc_Word8, Payload*>::iterator it =
    payload_type_map_.find(payload_type);

  if (it == payload_type_map_.end()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s failed to find payload_type:%d",
                 __FUNCTION__, payload_type);
    return -1;
  }
  delete it->second;
  payload_type_map_.erase(it);
  return 0;
}

WebRtc_Word32 RTPReceiver::ReceivePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate,
    WebRtc_Word8* payload_type) const {
  if (payload_type == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  size_t payload_name_length = strlen(payload_name);

  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
    payload_type_map_.begin();

  while (it != payload_type_map_.end()) {
    Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);
    if (payload_name_length == name_length &&
        StringCompare(payload->name, payload_name, payload_name_length)) {
      // Name matches.
      if (payload->audio) {
        if (rate == 0) {
          // [default] audio, check freq and channels.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels) {
            *payload_type = it->first;
            return 0;
          }
        } else {
          // Non-default audio, check freq, channels and rate.
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels &&
              payload->typeSpecific.Audio.rate == rate) {
            // extra rate condition added
            *payload_type = it->first;
            return 0;
          }
        }
      } else {
        // Video.
        *payload_type = it->first;
        return 0;
      }
    }
    it++;
  }
  return -1;
}

WebRtc_Word32 RTPReceiver::ReceivePayload(
    const WebRtc_Word8 payload_type,
    char payload_name[RTP_PAYLOAD_NAME_SIZE],
    WebRtc_UWord32* frequency,
    WebRtc_UWord8* channels,
    WebRtc_UWord32* rate) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
    payload_type_map_.find(payload_type);

  if (it == payload_type_map_.end()) {
    return -1;
  }
  Payload* payload = it->second;
  assert(payload);

  if (frequency) {
    if (payload->audio) {
      *frequency = payload->typeSpecific.Audio.frequency;
    } else {
      *frequency = kDefaultVideoFrequency;
    }
  }
  if (channels) {
    if (payload->audio) {
      *channels = payload->typeSpecific.Audio.channels;
    } else {
      *channels = 1;
    }
  }
  if (rate) {
    if (payload->audio) {
      *rate = payload->typeSpecific.Audio.rate;
    } else {
      assert(false);
      *rate = 0;
    }
  }
  if (payload_name) {
    payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
  }
  return 0;
}

WebRtc_Word32 RTPReceiver::RemotePayload(
  char payload_name[RTP_PAYLOAD_NAME_SIZE],
  WebRtc_Word8* payload_type,
  WebRtc_UWord32* frequency,
  WebRtc_UWord8* channels) const {
  if (last_received_payload_type_ == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "%s invalid state", __FUNCTION__);
    return -1;
  }
  std::map<WebRtc_Word8, Payload*>::const_iterator it =
    payload_type_map_.find(last_received_payload_type_);

  if (it == payload_type_map_.end()) {
    return -1;
  }
  Payload* payload = it->second;
  assert(payload);
  payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);

  if (payload_type) {
    *payload_type = last_received_payload_type_;
  }
  if (frequency) {
    if (payload->audio) {
      *frequency = payload->typeSpecific.Audio.frequency;
    } else {
      *frequency = kDefaultVideoFrequency;
    }
  }
  if (channels) {
    if (payload->audio) {
      *channels = payload->typeSpecific.Audio.channels;
    } else {
      *channels = 1;
    }
  }
  return 0;
}

WebRtc_Word32 RTPReceiver::RegisterRtpHeaderExtension(
    const RTPExtensionType type,
    const WebRtc_UWord8 id) {
  CriticalSectionScoped cs(critical_section_rtp_receiver_);
  return rtp_header_extension_map_.Register(type, id);
}

WebRtc_Word32 RTPReceiver::DeregisterRtpHeaderExtension(
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
WebRtc_Word32 RTPReceiver::SetNACKStatus(const NACKMethod method) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  nack_method_ = method;
  return 0;
}

void RTPReceiver::SetRTXStatus(const bool enable,
                               const WebRtc_UWord32 ssrc) {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  rtx_ = enable;
  ssrc_rtx_ = ssrc;
}

void RTPReceiver::RTXStatus(bool* enable, WebRtc_UWord32* ssrc) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  *enable = rtx_;
  *ssrc = ssrc_rtx_;
}

WebRtc_UWord32 RTPReceiver::SSRC() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return ssrc_;
}

// Get remote CSRC.
WebRtc_Word32 RTPReceiver::CSRCs(
    WebRtc_UWord32 array_of_csrcs[kRtpCsrcSize]) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  assert(num_csrcs_ <= kRtpCsrcSize);

  if (num_csrcs_ > 0) {
    memcpy(array_of_csrcs, current_remote_csrc_,
           sizeof(WebRtc_UWord32)*num_csrcs_);
  }
  return num_csrcs_;
}

WebRtc_Word32 RTPReceiver::Energy(
    WebRtc_UWord8 array_of_energy[kRtpCsrcSize]) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  assert(num_energy_ <= kRtpCsrcSize);

  if (num_energy_ > 0) {
    memcpy(array_of_energy, current_remote_energy_,
           sizeof(WebRtc_UWord8)*num_csrcs_);
  }
  return num_energy_;
}

WebRtc_Word32 RTPReceiver::IncomingRTPPacket(
  WebRtcRTPHeader* rtp_header,
  const WebRtc_UWord8* packet,
  const WebRtc_UWord16 packet_length) {
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
      // Sanity check.
      if (rtp_header->header.headerLength + 2 > packet_length) {
        return -1;
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
    CriticalSectionScoped lock(critical_section_cbs_);
    if (cb_rtp_feedback_) {
      if (length - rtp_header->header.headerLength == 0) {
        // Keep-alive packet.
        cb_rtp_feedback_->OnReceivedPacket(id_, kPacketKeepAlive);
      } else {
        cb_rtp_feedback_->OnReceivedPacket(id_, kPacketRtp);
      }
    }
  }
  WebRtc_Word8 first_payload_byte = 0;
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

  WebRtc_UWord16 payload_data_length =
    ModuleRTPUtility::GetPayloadDataLength(rtp_header, packet_length);

  WebRtc_Word32 ret_val = rtp_media_receiver_->ParseRtpPacket(
                            rtp_header, specific_payload, is_red, packet,
                            packet_length, clock_.GetTimeInMS());

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
  last_receive_time_ = clock_.GetTimeInMS();
  last_received_payload_length_ = payload_data_length;

  if (!old_packet) {
    if (last_received_timestamp_ != rtp_header->header.timestamp) {
      last_received_timestamp_ = rtp_header->header.timestamp;
      last_received_frame_time_ms_ = clock_.GetTimeInMS();
    }
    last_received_sequence_number_ = rtp_header->header.sequenceNumber;
    last_received_transmission_time_offset_ =
      rtp_header->extension.transmissionTimeOffset;
  }
  return ret_val;
}

// Implementation note: must not hold critsect when called!
WebRtc_Word32 RTPReceiver::CallbackOfReceivedPayloadData(
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_size,
    const WebRtcRTPHeader* rtp_header) {
  CriticalSectionScoped lock(critical_section_cbs_);
  if (cb_rtp_data_) {
    return cb_rtp_data_->OnReceivedPayloadData(payload_data, payload_size,
                                               rtp_header);
  }
  return -1;
}

// Implementation note: we expect to have the critical_section_rtp_receiver_
// critsect when we call this.
void RTPReceiver::UpdateStatistics(const WebRtcRTPHeader* rtp_header,
                                   const WebRtc_UWord16 bytes,
                                   const bool old_packet) {
  WebRtc_UWord32 frequency_hz = rtp_media_receiver_->GetFrequencyHz();

  Bitrate::Update(bytes);

  received_byte_count_ += bytes;

  if (received_seq_max_ == 0 && received_seq_wraps_ == 0) {
    // This is the first received report.
    received_seq_first_ = rtp_header->header.sequenceNumber;
    received_seq_max_ = rtp_header->header.sequenceNumber;
    received_inorder_packet_count_ = 1;
    local_time_last_received_timestamp_ =
      GetCurrentRTP(&clock_, frequency_hz);  // Time in samples.
    return;
  }

  // Count only the new packets received.
  if (InOrderPacket(rtp_header->header.sequenceNumber)) {
    const WebRtc_UWord32 RTPtime =
      GetCurrentRTP(&clock_, frequency_hz);  // Time in samples.
    received_inorder_packet_count_++;

    // Wrong if we use RetransmitOfOldPacket.
    WebRtc_Word32 seq_diff =
        rtp_header->header.sequenceNumber - received_seq_max_;
    if (seq_diff < 0) {
      // Wrap around detected.
      received_seq_wraps_++;
    }
    // new max
    received_seq_max_ = rtp_header->header.sequenceNumber;

    if (rtp_header->header.timestamp != last_received_timestamp_ &&
        received_inorder_packet_count_ > 1) {
      WebRtc_Word32 time_diff_samples =
          (RTPtime - local_time_last_received_timestamp_) -
          (rtp_header->header.timestamp - last_received_timestamp_);

      time_diff_samples = abs(time_diff_samples);

      // lib_jingle sometimes deliver crazy jumps in TS for the same stream.
      // If this happens, don't update jitter value. Use 5 secs video frequency
      // as the treshold.
      if (time_diff_samples < 450000) {
        // Note we calculate in Q4 to avoid using float.
        WebRtc_Word32 jitter_diff_q4 = (time_diff_samples << 4) - jitter_q4_;
        jitter_q4_ += ((jitter_diff_q4 + 8) >> 4);
      }

      // Extended jitter report, RFC 5450.
      // Actual network jitter, excluding the source-introduced jitter.
      WebRtc_Word32 time_diff_samples_ext =
        (RTPtime - local_time_last_received_timestamp_) -
        ((rtp_header->header.timestamp +
          rtp_header->extension.transmissionTimeOffset) -
         (last_received_timestamp_ +
          last_received_transmission_time_offset_));

      time_diff_samples_ext = abs(time_diff_samples_ext);

      if (time_diff_samples_ext < 450000) {
        WebRtc_Word32 jitter_diffQ4TransmissionTimeOffset =
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

  WebRtc_UWord16 packet_oh =
      rtp_header->header.headerLength + rtp_header->header.paddingLength;

  // Our measured overhead. Filter from RFC 5104 4.2.1.2:
  // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
  received_packet_oh_ = (15 * received_packet_oh_ + packet_oh) >> 4;
}

// Implementation note: we expect to have the critical_section_rtp_receiver_
// critsect when we call this.
bool RTPReceiver::RetransmitOfOldPacket(
    const WebRtc_UWord16 sequence_number,
    const WebRtc_UWord32 rtp_time_stamp) const {
  if (InOrderPacket(sequence_number)) {
    return false;
  }

  WebRtc_UWord32 frequency_khz = rtp_media_receiver_->GetFrequencyHz() / 1000;
  WebRtc_Word64 time_diff_ms = clock_.GetTimeInMS() - last_receive_time_;

  // Diff in time stamp since last received in order.
  WebRtc_Word32 rtp_time_stamp_diff_ms =
      static_cast<WebRtc_Word32>(rtp_time_stamp - last_received_timestamp_) /
      frequency_khz;

  WebRtc_UWord16 min_rtt = 0;
  WebRtc_Word32 max_delay_ms = 0;
  rtp_rtcp_.RTT(ssrc_, NULL, NULL, &min_rtt, NULL);
  if (min_rtt == 0) {
    // Jitter variance in samples.
    float jitter = jitter_q4_ >> 4;

    // Jitter standard deviation in samples.
    float jitter_std = sqrt(jitter);

    // 2 times the standard deviation => 95% confidence.
    // And transform to milliseconds by dividing by the frequency in kHz.
    max_delay_ms = static_cast<WebRtc_Word32>((2 * jitter_std) / frequency_khz);

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

bool RTPReceiver::InOrderPacket(const WebRtc_UWord16 sequence_number) const {
  if (received_seq_max_ >= sequence_number) {
    // Detect wrap-around.
    if (!(received_seq_max_ > 0xff00 && sequence_number < 0x0ff)) {
      if (received_seq_max_ - NACK_PACKETS_MAX_SIZE > sequence_number) {
        // We have a restart of the remote side.
      } else {
        // we received a retransmit of a packet we already have.
        return false;
      }
    }
  } else {
    // Detect wrap-around.
    if (sequence_number > 0xff00 && received_seq_max_ < 0x0ff) {
      if (received_seq_max_ - NACK_PACKETS_MAX_SIZE > sequence_number) {
        // We have a restart of the remote side
      } else {
        // We received a retransmit of a packet we already have
        return false;
      }
    }
  }
  return true;
}

WebRtc_UWord16 RTPReceiver::SequenceNumber() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_sequence_number_;
}

WebRtc_UWord32 RTPReceiver::TimeStamp() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_timestamp_;
}

int32_t RTPReceiver::LastReceivedTimeMs() const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  return last_received_frame_time_ms_;
}

WebRtc_UWord32 RTPReceiver::PayloadTypeToPayload(
  const WebRtc_UWord8 payload_type,
  Payload*& payload) const {

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
    payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return -1;
  }
  payload = it->second;
  return 0;
}

// Compute time stamp of the last incoming packet that is the first packet of
// its frame.
WebRtc_Word32 RTPReceiver::EstimatedRemoteTimeStamp(
    WebRtc_UWord32& timestamp) const {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);
  WebRtc_UWord32 frequency_hz = rtp_media_receiver_->GetFrequencyHz();

  if (local_time_last_received_timestamp_ == 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "%s invalid state", __FUNCTION__);
    return -1;
  }
  // Time in samples.
  WebRtc_UWord32 diff = GetCurrentRTP(&clock_, frequency_hz) -
                        local_time_last_received_timestamp_;

  timestamp = last_received_timestamp_ + diff;
  return 0;
}

// Get the currently configured SSRC filter.
WebRtc_Word32 RTPReceiver::SSRCFilter(WebRtc_UWord32& allowed_ssrc) const {
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
WebRtc_Word32 RTPReceiver::SetSSRCFilter(
    const bool enable, const WebRtc_UWord32 allowed_ssrc) {
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
  WebRtc_UWord32 frequency = kDefaultVideoFrequency;
  WebRtc_UWord8 channels = 1;
  WebRtc_UWord32 rate = 0;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    if (ssrc_ != rtp_header->header.ssrc ||
        (last_received_payload_type_ == -1 && ssrc_ == 0)) {
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
        if (rtp_header->header.payloadType == last_received_payload_type_) {
          re_initialize_decoder = true;

          std::map<WebRtc_Word8, Payload*>::iterator it =
            payload_type_map_.find(rtp_header->header.payloadType);

          if (it == payload_type_map_.end()) {
            return;
          }
          Payload* payload = it->second;
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
  }
  CriticalSectionScoped lock(critical_section_cbs_);
  if (cb_rtp_feedback_) {
    if (new_ssrc) {
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
}

// Implementation note: must not hold critsect when called.
// TODO(phoglund): Move as much as possible of this code path into the media
// specific receivers. Basically this method goes through a lot of trouble to
// compute something which is only used by the media specific parts later. If
// this code path moves we can get rid of some of the rtp_receiver ->
// media_specific interface (such as CheckPayloadChange, possibly get/set
// last known payload).
WebRtc_Word32 RTPReceiver::CheckPayloadChanged(
  const WebRtcRTPHeader* rtp_header,
  const WebRtc_Word8 first_payload_byte,
  bool& is_red,
  ModuleRTPUtility::PayloadUnion* specific_payload) {
  bool re_initialize_decoder = false;

  char payload_name[RTP_PAYLOAD_NAME_SIZE];
  WebRtc_Word8 payload_type = rtp_header->header.payloadType;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    if (payload_type != last_received_payload_type_) {
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
        if (payload_type == last_received_payload_type_) {
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

      std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
        payload_type_map_.find(payload_type);

      // Check that this is a registered payload type.
      if (it == payload_type_map_.end()) {
        return -1;
      }
      Payload* payload = it->second;
      assert(payload);
      payload_name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
      strncpy(payload_name, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);

      last_received_payload_type_ = payload_type;

      re_initialize_decoder = true;

      rtp_media_receiver_->SetLastMediaSpecificPayload(payload->typeSpecific);
      rtp_media_receiver_->GetLastMediaSpecificPayload(specific_payload);

      if (!payload->audio) {
        if (VideoCodecType() == kRtpFecVideo) {
          // Only reset the decoder on media packets.
          re_initialize_decoder = false;
        } else {
          if (last_received_media_payload_type_ ==
              last_received_payload_type_) {
            // Only reset the decoder if the media codec type has changed.
            re_initialize_decoder = false;
          }
          last_received_media_payload_type_ = last_received_payload_type_;
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
    CriticalSectionScoped lock(critical_section_cbs_);
    if (cb_rtp_feedback_) {
      if (-1 == rtp_media_receiver_->InvokeOnInitializeDecoder(
            cb_rtp_feedback_, id_, payload_type, payload_name,
            *specific_payload)) {
        return -1;  // Wrong payload type.
      }
    }
  }
  return 0;
}

// Implementation note: must not hold critsect when called.
void RTPReceiver::CheckCSRC(const WebRtcRTPHeader* rtp_header) {
  WebRtc_Word32 num_csrcs_diff = 0;
  WebRtc_UWord32 old_remote_csrc[kRtpCsrcSize];
  WebRtc_UWord8 old_num_csrcs = 0;

  {
    CriticalSectionScoped lock(critical_section_rtp_receiver_);

    if (rtp_receiver_audio_->TelephoneEventPayloadType(
          rtp_header->header.payloadType)) {
      // Don't do this for DTMF packets.
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
             num_csrcs_ * sizeof(WebRtc_UWord32));
    }
    const WebRtc_UWord8 num_csrcs = rtp_header->header.numCSRCs;
    if ((num_csrcs > 0) && (num_csrcs <= kRtpCsrcSize)) {
      // Copy new.
      memcpy(current_remote_csrc_,
             rtp_header->header.arrOfCSRCs,
             num_csrcs * sizeof(WebRtc_UWord32));
    }
    if (num_csrcs > 0 || old_num_csrcs > 0) {
      num_csrcs_diff = num_csrcs - old_num_csrcs;
      num_csrcs_ = num_csrcs;  // Update stored CSRCs.
    } else {
      // No change.
      return;
    }
  }  // End critsect.

  CriticalSectionScoped lock(critical_section_cbs_);
  if (cb_rtp_feedback_ == NULL) {
    return;
  }
  bool have_called_callback = false;
  // Search for new CSRC in old array.
  for (WebRtc_UWord8 i = 0; i < rtp_header->header.numCSRCs; ++i) {
    const WebRtc_UWord32 csrc = rtp_header->header.arrOfCSRCs[i];

    bool found_match = false;
    for (WebRtc_UWord8 j = 0; j < old_num_csrcs; ++j) {
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
  for (WebRtc_UWord8 i = 0; i < old_num_csrcs; ++i) {
    const WebRtc_UWord32 csrc = old_remote_csrc[i];

    bool found_match = false;
    for (WebRtc_UWord8 j = 0; j < rtp_header->header.numCSRCs; ++j) {
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

WebRtc_Word32 RTPReceiver::ResetStatistics() {
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

WebRtc_Word32 RTPReceiver::ResetDataCounters() {
  CriticalSectionScoped lock(critical_section_rtp_receiver_);

  received_byte_count_ = 0;
  received_old_packet_count_ = 0;
  received_inorder_packet_count_ = 0;
  last_report_inorder_packets_ = 0;

  return 0;
}

WebRtc_Word32 RTPReceiver::Statistics(
    WebRtc_UWord8*  fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* max_jitter,
    WebRtc_UWord32* jitter_transmission_time_offset,
    bool reset) const {
  WebRtc_Word32 missing;
  return Statistics(fraction_lost,
                    cum_lost,
                    ext_max,
                    jitter,
                    max_jitter,
                    jitter_transmission_time_offset,
                    &missing,
                    reset);
}

WebRtc_Word32 RTPReceiver::Statistics(
    WebRtc_UWord8*  fraction_lost,
    WebRtc_UWord32* cum_lost,
    WebRtc_UWord32* ext_max,
    WebRtc_UWord32* jitter,
    WebRtc_UWord32* max_jitter,
    WebRtc_UWord32* jitter_transmission_time_offset,
    WebRtc_Word32*  missing,
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
  WebRtc_UWord16 exp_since_last = (received_seq_max_ - last_report_seq_max_);

  if (last_report_seq_max_ > received_seq_max_) {
    // Can we assume that the seq_num can't go decrease over a full RTCP period?
    exp_since_last = 0;
  }

  // Number of received RTP packets since last report, counts all packets but
  // not re-transmissions.
  WebRtc_UWord32 rec_since_last =
      received_inorder_packet_count_ - last_report_inorder_packets_;

  if (nack_method_ == kNackOff) {
    // This is needed for re-ordered packets.
    WebRtc_UWord32 old_packets =
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
  WebRtc_UWord8 local_fraction_lost = 0;
  if (exp_since_last) {
    // Scale 0 to 255, where 255 is 100% loss.
    local_fraction_lost = (WebRtc_UWord8)((255 * (*missing)) / exp_since_last);
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

WebRtc_Word32 RTPReceiver::DataCounters(
    WebRtc_UWord32* bytes_received,
    WebRtc_UWord32* packets_received) const {
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
}

} // namespace webrtc
