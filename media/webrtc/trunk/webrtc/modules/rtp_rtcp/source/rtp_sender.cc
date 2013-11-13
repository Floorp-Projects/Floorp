/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"

#include <stdlib.h>  // srand

#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_audio.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

// Max in the RFC 3550 is 255 bytes, we limit it to be modulus 32 for SRTP.
const int kMaxPaddingLength = 224;

namespace {

const char* FrameTypeToString(const FrameType frame_type) {
  switch (frame_type) {
    case kFrameEmpty: return "empty";
    case kAudioFrameSpeech: return "audio_speech";
    case kAudioFrameCN: return "audio_cn";
    case kVideoFrameKey: return "video_key";
    case kVideoFrameDelta: return "video_delta";
    case kVideoFrameGolden: return "video_golden";
    case kVideoFrameAltRef: return "video_altref";
  }
  return "";
}

}  // namespace

RTPSender::RTPSender(const int32_t id, const bool audio, Clock *clock,
                     Transport *transport, RtpAudioFeedback *audio_feedback,
                     PacedSender *paced_sender)
    : Bitrate(clock), id_(id), audio_configured_(audio), audio_(NULL),
      video_(NULL), paced_sender_(paced_sender),
      send_critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      transport_(transport), sending_media_(true),  // Default to sending media.
      max_payload_length_(IP_PACKET_SIZE - 28),     // Default is IP-v4/UDP.
      target_send_bitrate_(0), packet_over_head_(28), payload_type_(-1),
      payload_type_map_(), rtp_header_extension_map_(),
      transmission_time_offset_(0), absolute_send_time_(0),
      // NACK.
      nack_byte_count_times_(), nack_byte_count_(), nack_bitrate_(clock),
      packet_history_(new RTPPacketHistory(clock)),
      // Statistics
      statistics_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      packets_sent_(0), payload_bytes_sent_(0), start_time_stamp_forced_(false),
      start_time_stamp_(0), ssrc_db_(*SSRCDatabase::GetSSRCDatabase()),
      remote_ssrc_(0), sequence_number_forced_(false), ssrc_forced_(false),
      timestamp_(0), capture_time_ms_(0), last_packet_marker_bit_(false),
      num_csrcs_(0), csrcs_(), include_csrcs_(true),
      rtx_(kRtxOff), payload_type_rtx_(-1) {
  memset(nack_byte_count_times_, 0, sizeof(nack_byte_count_times_));
  memset(nack_byte_count_, 0, sizeof(nack_byte_count_));
  memset(csrcs_, 0, sizeof(csrcs_));
  // We need to seed the random generator.
  srand(static_cast<uint32_t>(clock_->TimeInMilliseconds()));
  ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  ssrc_rtx_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  // Random start, 16 bits. Can't be 0.
  sequence_number_rtx_ = static_cast<uint16_t>(rand() + 1) & 0x7FFF;
  sequence_number_ = static_cast<uint16_t>(rand() + 1) & 0x7FFF;

  if (audio) {
    audio_ = new RTPSenderAudio(id, clock_, this);
    audio_->RegisterAudioCallback(audio_feedback);
  } else {
    video_ = new RTPSenderVideo(id, clock_, this);
  }
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPSender::~RTPSender() {
  if (remote_ssrc_ != 0) {
    ssrc_db_.ReturnSSRC(remote_ssrc_);
  }
  ssrc_db_.ReturnSSRC(ssrc_);

  SSRCDatabase::ReturnSSRCDatabase();
  delete send_critsect_;
  while (!payload_type_map_.empty()) {
    std::map<int8_t, ModuleRTPUtility::Payload *>::iterator it =
        payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
  delete packet_history_;
  delete audio_;
  delete video_;

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id_, "%s deleted", __FUNCTION__);
}

void RTPSender::SetTargetSendBitrate(const uint32_t bits) {
  target_send_bitrate_ = static_cast<uint16_t>(bits / 1000);
}

uint16_t RTPSender::ActualSendBitrateKbit() const {
  return (uint16_t)(Bitrate::BitrateNow() / 1000);
}

uint32_t RTPSender::VideoBitrateSent() const {
  if (video_) {
    return video_->VideoBitrateSent();
  }
  return 0;
}

uint32_t RTPSender::FecOverheadRate() const {
  if (video_) {
    return video_->FecOverheadRate();
  }
  return 0;
}

uint32_t RTPSender::NackOverheadRate() const {
  return nack_bitrate_.BitrateLast();
}

int32_t RTPSender::SetTransmissionTimeOffset(
    const int32_t transmission_time_offset) {
  if (transmission_time_offset > (0x800000 - 1) ||
      transmission_time_offset < -(0x800000 - 1)) {  // Word24.
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_);
  transmission_time_offset_ = transmission_time_offset;
  return 0;
}

int32_t RTPSender::SetAbsoluteSendTime(
    const uint32_t absolute_send_time) {
  if (absolute_send_time > 0xffffff) {  // UWord24.
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_);
  absolute_send_time_ = absolute_send_time;
  return 0;
}

int32_t RTPSender::RegisterRtpHeaderExtension(const RTPExtensionType type,
                                              const uint8_t id) {
  CriticalSectionScoped cs(send_critsect_);
  return rtp_header_extension_map_.Register(type, id);
}

int32_t RTPSender::DeregisterRtpHeaderExtension(
    const RTPExtensionType type) {
  CriticalSectionScoped cs(send_critsect_);
  return rtp_header_extension_map_.Deregister(type);
}

uint16_t RTPSender::RtpHeaderExtensionTotalLength() const {
  CriticalSectionScoped cs(send_critsect_);
  return rtp_header_extension_map_.GetTotalLengthInBytes();
}

int32_t RTPSender::RegisterPayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_number, const uint32_t frequency,
    const uint8_t channels, const uint32_t rate) {
  assert(payload_name);
  CriticalSectionScoped cs(send_critsect_);

  std::map<int8_t, ModuleRTPUtility::Payload *>::iterator it =
      payload_type_map_.find(payload_number);

  if (payload_type_map_.end() != it) {
    // We already use this payload type.
    ModuleRTPUtility::Payload *payload = it->second;
    assert(payload);

    // Check if it's the same as we already have.
    if (ModuleRTPUtility::StringCompare(payload->name, payload_name,
                                        RTP_PAYLOAD_NAME_SIZE - 1)) {
      if (audio_configured_ && payload->audio &&
          payload->typeSpecific.Audio.frequency == frequency &&
          (payload->typeSpecific.Audio.rate == rate ||
           payload->typeSpecific.Audio.rate == 0 || rate == 0)) {
        payload->typeSpecific.Audio.rate = rate;
        // Ensure that we update the rate if new or old is zero.
        return 0;
      }
      if (!audio_configured_ && !payload->audio) {
        return 0;
      }
    }
    return -1;
  }
  int32_t ret_val = -1;
  ModuleRTPUtility::Payload *payload = NULL;
  if (audio_configured_) {
    ret_val = audio_->RegisterAudioPayload(payload_name, payload_number,
                                           frequency, channels, rate, payload);
  } else {
    ret_val = video_->RegisterVideoPayload(payload_name, payload_number, rate,
                                           payload);
  }
  if (payload) {
    payload_type_map_[payload_number] = payload;
  }
  return ret_val;
}

int32_t RTPSender::DeRegisterSendPayload(
    const int8_t payload_type) {
  CriticalSectionScoped lock(send_critsect_);

  std::map<int8_t, ModuleRTPUtility::Payload *>::iterator it =
      payload_type_map_.find(payload_type);

  if (payload_type_map_.end() == it) {
    return -1;
  }
  ModuleRTPUtility::Payload *payload = it->second;
  delete payload;
  payload_type_map_.erase(it);
  return 0;
}

int8_t RTPSender::SendPayloadType() const { return payload_type_; }

int RTPSender::SendPayloadFrequency() const {
  return audio_ != NULL ? audio_->AudioFrequency() : kVideoPayloadTypeFrequency;
}

int32_t RTPSender::SetMaxPayloadLength(
    const uint16_t max_payload_length,
    const uint16_t packet_over_head) {
  // Sanity check.
  if (max_payload_length < 100 || max_payload_length > IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_, "%s invalid argument",
                 __FUNCTION__);
    return -1;
  }
  CriticalSectionScoped cs(send_critsect_);
  max_payload_length_ = max_payload_length;
  packet_over_head_ = packet_over_head;

  WEBRTC_TRACE(kTraceInfo, kTraceRtpRtcp, id_, "SetMaxPayloadLength to %d.",
               max_payload_length);
  return 0;
}

uint16_t RTPSender::MaxDataPayloadLength() const {
  if (audio_configured_) {
    return max_payload_length_ - RTPHeaderLength();
  } else {
    return max_payload_length_ - RTPHeaderLength() -
           video_->FECPacketOverhead() - ((rtx_) ? 2 : 0);
    // Include the FEC/ULP/RED overhead.
  }
}

uint16_t RTPSender::MaxPayloadLength() const {
  return max_payload_length_;
}

uint16_t RTPSender::PacketOverHead() const { return packet_over_head_; }

void RTPSender::SetRTXStatus(RtxMode mode, bool set_ssrc, uint32_t ssrc) {
  CriticalSectionScoped cs(send_critsect_);
  rtx_ = mode;
  if (rtx_ != kRtxOff) {
    if (set_ssrc) {
      ssrc_rtx_ = ssrc;
    } else {
      ssrc_rtx_ = ssrc_db_.CreateSSRC();  // Can't be 0.
    }
  }
}

void RTPSender::RTXStatus(RtxMode* mode, uint32_t* ssrc,
                          int* payload_type) const {
  CriticalSectionScoped cs(send_critsect_);
  *mode = rtx_;
  *ssrc = ssrc_rtx_;
  *payload_type = payload_type_rtx_;
}


void RTPSender::SetRtxPayloadType(int payload_type) {
  CriticalSectionScoped cs(send_critsect_);
  payload_type_rtx_ = payload_type;
}

int32_t RTPSender::CheckPayloadType(const int8_t payload_type,
                                    RtpVideoCodecTypes *video_type) {
  CriticalSectionScoped cs(send_critsect_);

  if (payload_type < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_, "\tinvalid payload_type (%d)",
                 payload_type);
    return -1;
  }
  if (audio_configured_) {
    int8_t red_pl_type = -1;
    if (audio_->RED(red_pl_type) == 0) {
      // We have configured RED.
      if (red_pl_type == payload_type) {
        // And it's a match...
        return 0;
      }
    }
  }
  if (payload_type_ == payload_type) {
    if (!audio_configured_) {
      *video_type = video_->VideoCodecType();
    }
    return 0;
  }
  std::map<int8_t, ModuleRTPUtility::Payload *>::iterator it =
      payload_type_map_.find(payload_type);
  if (it == payload_type_map_.end()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "\tpayloadType:%d not registered", payload_type);
    return -1;
  }
  payload_type_ = payload_type;
  ModuleRTPUtility::Payload *payload = it->second;
  assert(payload);
  if (!payload->audio && !audio_configured_) {
    video_->SetVideoCodecType(payload->typeSpecific.Video.videoCodecType);
    *video_type = payload->typeSpecific.Video.videoCodecType;
    video_->SetMaxConfiguredBitrateVideo(payload->typeSpecific.Video.maxRate);
  }
  return 0;
}

int32_t RTPSender::SendOutgoingData(
    const FrameType frame_type, const int8_t payload_type,
    const uint32_t capture_timestamp, int64_t capture_time_ms,
    const uint8_t *payload_data, const uint32_t payload_size,
    const RTPFragmentationHeader *fragmentation,
    VideoCodecInformation *codec_info, const RTPVideoTypeHeader *rtp_type_hdr) {
  {
    // Drop this packet if we're not sending media packets.
    CriticalSectionScoped cs(send_critsect_);
    if (!sending_media_) {
      return 0;
    }
  }
  RtpVideoCodecTypes video_type = kRtpVideoGeneric;
  if (CheckPayloadType(payload_type, &video_type) != 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument failed to find payload_type:%d",
                 __FUNCTION__, payload_type);
    return -1;
  }

  if (audio_configured_) {
    TRACE_EVENT_ASYNC_STEP1("webrtc", "Audio", capture_timestamp,
                            "Send", "type", FrameTypeToString(frame_type));
    assert(frame_type == kAudioFrameSpeech || frame_type == kAudioFrameCN ||
           frame_type == kFrameEmpty);

    return audio_->SendAudio(frame_type, payload_type, capture_timestamp,
                             payload_data, payload_size, fragmentation);
  } else {
    TRACE_EVENT_ASYNC_STEP1("webrtc", "Video", capture_time_ms,
                            "Send", "type", FrameTypeToString(frame_type));
    assert(frame_type != kAudioFrameSpeech && frame_type != kAudioFrameCN);

    if (frame_type == kFrameEmpty) {
      if (paced_sender_->Enabled()) {
        // Padding is driven by the pacer and not by the encoder.
        return 0;
      }
      return SendPaddingAccordingToBitrate(payload_type, capture_timestamp,
                                           capture_time_ms) ? 0 : -1;
    }
    return video_->SendVideo(video_type, frame_type, payload_type,
                             capture_timestamp, capture_time_ms, payload_data,
                             payload_size, fragmentation, codec_info,
                             rtp_type_hdr);
  }
}

bool RTPSender::SendPaddingAccordingToBitrate(
    int8_t payload_type, uint32_t capture_timestamp,
    int64_t capture_time_ms) {
  // Current bitrate since last estimate(1 second) averaged with the
  // estimate since then, to get the most up to date bitrate.
  uint32_t current_bitrate = BitrateNow();
  int bitrate_diff = target_send_bitrate_ * 1000 - current_bitrate;
  if (bitrate_diff <= 0) {
    return true;
  }
  int bytes = 0;
  if (current_bitrate == 0) {
    // Start up phase. Send one 33.3 ms batch to start with.
    bytes = (bitrate_diff / 8) / 30;
  } else {
    bytes = (bitrate_diff / 8);
    // Cap at 200 ms of target send data.
    int bytes_cap = target_send_bitrate_ * 25;  // 1000 / 8 / 5.
    if (bytes > bytes_cap) {
      bytes = bytes_cap;
    }
  }
  uint32_t timestamp;
  {
    CriticalSectionScoped cs(send_critsect_);
    // Add the random RTP timestamp offset and store the capture time for
    // later calculation of the send time offset.
    timestamp = start_time_stamp_ + capture_timestamp;
    timestamp_ = timestamp;
    capture_time_ms_ = capture_time_ms;
  }
  int bytes_sent = SendPadData(payload_type, timestamp, capture_time_ms,
                               bytes, kDontRetransmit, false, false);
  // We did not manage to send all bytes. Comparing with 31 due to modulus 32.
  return bytes - bytes_sent < 31;
}

int RTPSender::BuildPaddingPacket(uint8_t* packet, int header_length,
                                  int32_t bytes) {
  int padding_bytes_in_packet = kMaxPaddingLength;
  if (bytes < kMaxPaddingLength) {
    padding_bytes_in_packet = bytes;
  }
  packet[0] |= 0x20;  // Set padding bit.
  int32_t *data =
      reinterpret_cast<int32_t *>(&(packet[header_length]));

  // Fill data buffer with random data.
  for (int j = 0; j < (padding_bytes_in_packet >> 2); ++j) {
    data[j] = rand();  // NOLINT
  }
  // Set number of padding bytes in the last byte of the packet.
  packet[header_length + padding_bytes_in_packet - 1] = padding_bytes_in_packet;
  return padding_bytes_in_packet;
}

int RTPSender::SendPadData(int payload_type, uint32_t timestamp,
                           int64_t capture_time_ms, int32_t bytes,
                           StorageType store, bool force_full_size_packets,
                           bool only_pad_after_markerbit) {
  // Drop this packet if we're not sending media packets.
  if (!sending_media_) {
    return bytes;
  }
  int padding_bytes_in_packet = 0;
  int bytes_sent = 0;
  for (; bytes > 0; bytes -= padding_bytes_in_packet) {
    // Always send full padding packets.
    if (force_full_size_packets && bytes < kMaxPaddingLength)
      bytes = kMaxPaddingLength;
    if (bytes < kMaxPaddingLength) {
      if (force_full_size_packets) {
        bytes = kMaxPaddingLength;
      } else {
        // Round to the nearest multiple of 32.
        bytes = (bytes + 16) & 0xffe0;
      }
    }
    if (bytes < 32) {
      // Sanity don't send empty packets.
      break;
    }
    uint32_t ssrc;
    uint16_t sequence_number;
    {
      CriticalSectionScoped cs(send_critsect_);
      // Only send padding packets following the last packet of a frame,
      // indicated by the marker bit.
      if (only_pad_after_markerbit && !last_packet_marker_bit_)
        return bytes_sent;
      if (rtx_ == kRtxOff) {
        ssrc = ssrc_;
        sequence_number = sequence_number_;
        ++sequence_number_;
      } else {
        ssrc = ssrc_rtx_;
        sequence_number = sequence_number_rtx_;
        ++sequence_number_rtx_;
      }
    }
    uint8_t padding_packet[IP_PACKET_SIZE];
    int header_length = CreateRTPHeader(padding_packet, payload_type, ssrc,
                                        false, timestamp, sequence_number, NULL,
                                        0);
    padding_bytes_in_packet = BuildPaddingPacket(padding_packet, header_length,
                                                 bytes);
    if (0 > SendToNetwork(padding_packet, padding_bytes_in_packet,
                          header_length, capture_time_ms, store,
                          PacedSender::kLowPriority)) {
      // Error sending the packet.
      break;
    }
    bytes_sent += padding_bytes_in_packet;
  }
  return bytes_sent;
}

void RTPSender::SetStorePacketsStatus(const bool enable,
                                      const uint16_t number_to_store) {
  packet_history_->SetStorePacketsStatus(enable, number_to_store);
}

bool RTPSender::StorePackets() const {
  return packet_history_->StorePackets();
}

int32_t RTPSender::ReSendPacket(uint16_t packet_id, uint32_t min_resend_time) {
  uint16_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  uint8_t *buffer_to_send_ptr = data_buffer;
  int64_t capture_time_ms;
  StorageType type;
  if (!packet_history_->GetRTPPacket(packet_id, min_resend_time, data_buffer,
                                     &length, &capture_time_ms, &type)) {
    // Packet not found.
    return 0;
  }
  if (length == 0 || type == kDontRetransmit) {
    // No bytes copied (packet recently resent, skip resending) or
    // packet should not be retransmitted.
    return 0;
  }

  uint8_t data_buffer_rtx[IP_PACKET_SIZE];
  if (rtx_ != kRtxOff) {
    BuildRtxPacket(data_buffer, &length, data_buffer_rtx);
    buffer_to_send_ptr = data_buffer_rtx;
  }

  ModuleRTPUtility::RTPHeaderParser rtp_parser(data_buffer, length);
  RTPHeader header;
  rtp_parser.Parse(header);

  // Store the time when the packet was last sent or added to pacer.
  packet_history_->UpdateResendTime(packet_id);

  {
    // Update send statistics prior to pacer.
    CriticalSectionScoped lock(statistics_crit_.get());
    Bitrate::Update(length);
    ++packets_sent_;
    // We on purpose don't add to payload_bytes_sent_ since this is a
    // re-transmit and not new payload data.
  }

  TRACE_EVENT_INSTANT2("webrtc_rtp", "RTPSender::ReSendPacket",
                       "timestamp", header.timestamp,
                       "seqnum", header.sequenceNumber);

  if (paced_sender_) {
    if (!paced_sender_->SendPacket(PacedSender::kHighPriority,
                                   header.ssrc,
                                   header.sequenceNumber,
                                   capture_time_ms,
                                   length - header.headerLength)) {
      // We can't send the packet right now.
      // We will be called when it is time.
      return length;
    }
  }

  if (SendPacketToNetwork(buffer_to_send_ptr, length)) {
    return length;
  }
  return -1;
}

bool RTPSender::SendPacketToNetwork(const uint8_t *packet, uint32_t size) {
  int bytes_sent = -1;
  if (transport_) {
    bytes_sent = transport_->SendPacket(id_, packet, size);
  }
  TRACE_EVENT_INSTANT2("webrtc_rtp", "RTPSender::SendPacketToNetwork",
                       "size", size, "sent", bytes_sent);
  // TODO(pwesin): Add a separate bitrate for sent bitrate after pacer.
  if (bytes_sent <= 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "Transport failed to send packet");
    return false;
  }
  return true;
}

int RTPSender::SelectiveRetransmissions() const {
  if (!video_)
    return -1;
  return video_->SelectiveRetransmissions();
}

int RTPSender::SetSelectiveRetransmissions(uint8_t settings) {
  if (!video_)
    return -1;
  return video_->SetSelectiveRetransmissions(settings);
}

void RTPSender::OnReceivedNACK(
    const std::list<uint16_t>& nack_sequence_numbers,
    const uint16_t avg_rtt) {
  TRACE_EVENT2("webrtc_rtp", "RTPSender::OnReceivedNACK",
               "num_seqnum", nack_sequence_numbers.size(), "avg_rtt", avg_rtt);
  const int64_t now = clock_->TimeInMilliseconds();
  uint32_t bytes_re_sent = 0;

  // Enough bandwidth to send NACK?
  if (!ProcessNACKBitRate(now)) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "NACK bitrate reached. Skip sending NACK response. Target %d",
                 target_send_bitrate_);
    return;
  }

  for (std::list<uint16_t>::const_iterator it = nack_sequence_numbers.begin();
      it != nack_sequence_numbers.end(); ++it) {
    const int32_t bytes_sent = ReSendPacket(*it, 5 + avg_rtt);
    if (bytes_sent > 0) {
      bytes_re_sent += bytes_sent;
    } else if (bytes_sent == 0) {
      // The packet has previously been resent.
      // Try resending next packet in the list.
      continue;
    } else if (bytes_sent < 0) {
      // Failed to send one Sequence number. Give up the rest in this nack.
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "Failed resending RTP packet %d, Discard rest of packets",
                   *it);
      break;
    }
    // Delay bandwidth estimate (RTT * BW).
    if (target_send_bitrate_ != 0 && avg_rtt) {
      // kbits/s * ms = bits => bits/8 = bytes
      uint32_t target_bytes =
          (static_cast<uint32_t>(target_send_bitrate_) * avg_rtt) >> 3;
      if (bytes_re_sent > target_bytes) {
        break;  // Ignore the rest of the packets in the list.
      }
    }
  }
  if (bytes_re_sent > 0) {
    // TODO(pwestin) consolidate these two methods.
    UpdateNACKBitRate(bytes_re_sent, now);
    nack_bitrate_.Update(bytes_re_sent);
  }
}

bool RTPSender::ProcessNACKBitRate(const uint32_t now) {
  uint32_t num = 0;
  int32_t byte_count = 0;
  const uint32_t avg_interval = 1000;

  CriticalSectionScoped cs(send_critsect_);

  if (target_send_bitrate_ == 0) {
    return true;
  }
  for (num = 0; num < NACK_BYTECOUNT_SIZE; ++num) {
    if ((now - nack_byte_count_times_[num]) > avg_interval) {
      // Don't use data older than 1sec.
      break;
    } else {
      byte_count += nack_byte_count_[num];
    }
  }
  int32_t time_interval = avg_interval;
  if (num == NACK_BYTECOUNT_SIZE) {
    // More than NACK_BYTECOUNT_SIZE nack messages has been received
    // during the last msg_interval.
    time_interval = now - nack_byte_count_times_[num - 1];
    if (time_interval < 0) {
      time_interval = avg_interval;
    }
  }
  return (byte_count * 8) < (target_send_bitrate_ * time_interval);
}

void RTPSender::UpdateNACKBitRate(const uint32_t bytes,
                                  const uint32_t now) {
  CriticalSectionScoped cs(send_critsect_);

  // Save bitrate statistics.
  if (bytes > 0) {
    if (now == 0) {
      // Add padding length.
      nack_byte_count_[0] += bytes;
    } else {
      if (nack_byte_count_times_[0] == 0) {
        // First no shift.
      } else {
        // Shift.
        for (int i = (NACK_BYTECOUNT_SIZE - 2); i >= 0; i--) {
          nack_byte_count_[i + 1] = nack_byte_count_[i];
          nack_byte_count_times_[i + 1] = nack_byte_count_times_[i];
        }
      }
      nack_byte_count_[0] = bytes;
      nack_byte_count_times_[0] = now;
    }
  }
}

// Called from pacer when we can send the packet.
bool RTPSender::TimeToSendPacket(uint16_t sequence_number,
                                 int64_t capture_time_ms) {
  StorageType type;
  uint16_t length = IP_PACKET_SIZE;
  uint8_t data_buffer[IP_PACKET_SIZE];
  int64_t stored_time_ms;

  if (packet_history_ == NULL) {
    // Packet cannot be found. Allow sending to continue.
    return true;
  }
  if (!packet_history_->GetRTPPacket(sequence_number, 0, data_buffer, &length,
                                     &stored_time_ms, &type)) {
    // Packet cannot be found. Allow sending to continue.
    return true;
  }
  assert(length > 0);

  ModuleRTPUtility::RTPHeaderParser rtp_parser(data_buffer, length);
  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);
  TRACE_EVENT_INSTANT2("webrtc_rtp", "RTPSender::TimeToSendPacket",
                       "timestamp", rtp_header.timestamp,
                       "seqnum", sequence_number);

  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t diff_ms = now_ms - capture_time_ms;
  bool updated_transmission_time_offset =
      UpdateTransmissionTimeOffset(data_buffer, length, rtp_header, diff_ms);
  bool updated_abs_send_time =
      UpdateAbsoluteSendTime(data_buffer, length, rtp_header, now_ms);
  if (updated_transmission_time_offset || updated_abs_send_time) {
    // Update stored packet in case of receiving a re-transmission request.
    packet_history_->ReplaceRTPHeader(data_buffer,
                                      rtp_header.sequenceNumber,
                                      rtp_header.headerLength);
  }
  return SendPacketToNetwork(data_buffer, length);
}

int RTPSender::TimeToSendPadding(int bytes) {
  if (!sending_media_) {
    return 0;
  }
  int payload_type;
  int64_t capture_time_ms;
  uint32_t timestamp;
  {
    CriticalSectionScoped cs(send_critsect_);
    payload_type = (rtx_ == kRtxOff) ? payload_type_ : payload_type_rtx_;
    timestamp = timestamp_;
    capture_time_ms = capture_time_ms_;
  }
  return SendPadData(payload_type, timestamp, capture_time_ms, bytes,
                     kDontStore, true, true);
}

// TODO(pwestin): send in the RTPHeaderParser to avoid parsing it again.
int32_t RTPSender::SendToNetwork(
    uint8_t *buffer, int payload_length, int rtp_header_length,
    int64_t capture_time_ms, StorageType storage,
    PacedSender::Priority priority) {
  ModuleRTPUtility::RTPHeaderParser rtp_parser(
      buffer, payload_length + rtp_header_length);
  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);

  int64_t now_ms = clock_->TimeInMilliseconds();

  // |capture_time_ms| <= 0 is considered invalid.
  // TODO(holmer): This should be changed all over Video Engine so that negative
  // time is consider invalid, while 0 is considered a valid time.
  if (capture_time_ms > 0) {
    UpdateTransmissionTimeOffset(buffer, payload_length + rtp_header_length,
                                 rtp_header, now_ms - capture_time_ms);
  }

  UpdateAbsoluteSendTime(buffer, payload_length + rtp_header_length,
                         rtp_header, now_ms);

  // Used for NACK and to spread out the transmission of packets.
  if (packet_history_->PutRTPPacket(buffer, rtp_header_length + payload_length,
                                    max_payload_length_, capture_time_ms,
                                    storage) != 0) {
    return -1;
  }

  // Create and send RTX Packet.
  // TODO(pwesin): This should be moved to its own code path triggered by pacer.
  bool rtx_sent = false;
  if (rtx_ == kRtxAll && storage == kAllowRetransmission) {
    uint16_t length_rtx = payload_length + rtp_header_length;
    uint8_t data_buffer_rtx[IP_PACKET_SIZE];
    BuildRtxPacket(buffer, &length_rtx, data_buffer_rtx);
    if (!SendPacketToNetwork(data_buffer_rtx, length_rtx)) return -1;
    rtx_sent = true;
  }
  {
    // Update send statistics prior to pacer.
    CriticalSectionScoped lock(statistics_crit_.get());
    Bitrate::Update(payload_length + rtp_header_length);
    ++packets_sent_;
    payload_bytes_sent_ += payload_length;
    if (rtx_sent) {
      // The RTX packet.
      ++packets_sent_;
      payload_bytes_sent_ += payload_length;
    }
  }

  if (paced_sender_ && storage != kDontStore) {
    if (!paced_sender_->SendPacket(priority, rtp_header.ssrc,
                                   rtp_header.sequenceNumber, capture_time_ms,
                                   payload_length)) {
      // We can't send the packet right now.
      // We will be called when it is time.
      return 0;
    }
  }
  if (SendPacketToNetwork(buffer, payload_length + rtp_header_length)) {
    return 0;
  }
  return -1;
}

void RTPSender::ProcessBitrate() {
  CriticalSectionScoped cs(send_critsect_);
  Bitrate::Process();
  nack_bitrate_.Process();
  if (audio_configured_) {
    return;
  }
  video_->ProcessBitrate();
}

uint16_t RTPSender::RTPHeaderLength() const {
  uint16_t rtp_header_length = 12;
  if (include_csrcs_) {
    rtp_header_length += sizeof(uint32_t) * num_csrcs_;
  }
  rtp_header_length += RtpHeaderExtensionTotalLength();
  return rtp_header_length;
}

uint16_t RTPSender::IncrementSequenceNumber() {
  CriticalSectionScoped cs(send_critsect_);
  return sequence_number_++;
}

void RTPSender::ResetDataCounters() {
  CriticalSectionScoped lock(statistics_crit_.get());
  packets_sent_ = 0;
  payload_bytes_sent_ = 0;
}

uint32_t RTPSender::Packets() const {
  CriticalSectionScoped lock(statistics_crit_.get());
  return packets_sent_;
}

// Number of sent RTP bytes.
uint32_t RTPSender::Bytes() const {
  CriticalSectionScoped lock(statistics_crit_.get());
  return payload_bytes_sent_;
}

int RTPSender::CreateRTPHeader(
    uint8_t* header, int8_t payload_type, uint32_t ssrc, bool marker_bit,
    uint32_t timestamp, uint16_t sequence_number, const uint32_t* csrcs,
    uint8_t num_csrcs) const {
  header[0] = 0x80;  // version 2.
  header[1] = static_cast<uint8_t>(payload_type);
  if (marker_bit) {
    header[1] |= kRtpMarkerBitMask;  // Marker bit is set.
  }
  ModuleRTPUtility::AssignUWord16ToBuffer(header + 2, sequence_number);
  ModuleRTPUtility::AssignUWord32ToBuffer(header + 4, timestamp);
  ModuleRTPUtility::AssignUWord32ToBuffer(header + 8, ssrc);
  int32_t rtp_header_length = 12;

  // Add the CSRCs if any.
  if (num_csrcs > 0) {
    if (num_csrcs > kRtpCsrcSize) {
      // error
      assert(false);
      return -1;
    }
    uint8_t *ptr = &header[rtp_header_length];
    for (int i = 0; i < num_csrcs; ++i) {
      ModuleRTPUtility::AssignUWord32ToBuffer(ptr, csrcs[i]);
      ptr += 4;
    }
    header[0] = (header[0] & 0xf0) | num_csrcs;

    // Update length of header.
    rtp_header_length += sizeof(uint32_t) * num_csrcs;
  }

  uint16_t len = BuildRTPHeaderExtension(header + rtp_header_length);
  if (len > 0) {
    header[0] |= 0x10;  // Set extension bit.
    rtp_header_length += len;
  }
  return rtp_header_length;
}

int32_t RTPSender::BuildRTPheader(
    uint8_t *data_buffer, const int8_t payload_type,
    const bool marker_bit, const uint32_t capture_timestamp,
    int64_t capture_time_ms, const bool time_stamp_provided,
    const bool inc_sequence_number) {
  assert(payload_type >= 0);
  CriticalSectionScoped cs(send_critsect_);

  if (time_stamp_provided) {
    timestamp_ = start_time_stamp_ + capture_timestamp;
  } else {
    // Make a unique time stamp.
    // We can't inc by the actual time, since then we increase the risk of back
    // timing.
    timestamp_++;
  }
  uint32_t sequence_number = sequence_number_++;
  capture_time_ms_ = capture_time_ms;
  last_packet_marker_bit_ = marker_bit;
  int csrcs_length = 0;
  if (include_csrcs_)
    csrcs_length = num_csrcs_;
  return CreateRTPHeader(data_buffer, payload_type, ssrc_, marker_bit,
                         timestamp_, sequence_number, csrcs_, csrcs_length);
}

uint16_t RTPSender::BuildRTPHeaderExtension(uint8_t* data_buffer) const {
  if (rtp_header_extension_map_.Size() <= 0) {
    return 0;
  }
  // RTP header extension, RFC 3550.
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |      defined by profile       |           length              |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                        header extension                       |
  //  |                             ....                              |
  //
  const uint32_t kPosLength = 2;
  const uint32_t kHeaderLength = kRtpOneByteHeaderLength;

  // Add extension ID (0xBEDE).
  ModuleRTPUtility::AssignUWord16ToBuffer(data_buffer,
                                          kRtpOneByteHeaderExtensionId);

  // Add extensions.
  uint16_t total_block_length = 0;

  RTPExtensionType type = rtp_header_extension_map_.First();
  while (type != kRtpExtensionNone) {
    uint8_t block_length = 0;
    switch (type) {
      case kRtpExtensionTransmissionTimeOffset:
        block_length = BuildTransmissionTimeOffsetExtension(
            data_buffer + kHeaderLength + total_block_length);
        break;
      case kRtpExtensionAudioLevel:
        // Because AudioLevel is handled specially by RTPSenderAudio, we pretend
        // we don't have to care about it here, which is true until we wan't to
        // use it together with any of the other extensions we support.
        break;
      case kRtpExtensionAbsoluteSendTime:
        block_length = BuildAbsoluteSendTimeExtension(
            data_buffer + kHeaderLength + total_block_length);
        break;
      default:
        assert(false);
    }
    total_block_length += block_length;
    type = rtp_header_extension_map_.Next(type);
  }
  if (total_block_length == 0) {
    // No extension added.
    return 0;
  }
  // Set header length (in number of Word32, header excluded).
  assert(total_block_length % 4 == 0);
  ModuleRTPUtility::AssignUWord16ToBuffer(data_buffer + kPosLength,
                                          total_block_length / 4);
  // Total added length.
  return kHeaderLength + total_block_length;
}

uint8_t RTPSender::BuildTransmissionTimeOffsetExtension(
    uint8_t* data_buffer) const {
  // From RFC 5450: Transmission Time Offsets in RTP Streams.
  //
  // The transmission time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit signed integer.
  // When added to the RTP timestamp of the packet, it represents the
  // "effective" RTP transmission time of the packet, on the RTP
  // timescale.
  //
  // The form of the transmission offset extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              transmission offset              |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransmissionTimeOffset,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ModuleRTPUtility::AssignUWord24ToBuffer(data_buffer + pos,
                                          transmission_time_offset_);
  pos += 3;
  assert(pos == kTransmissionTimeOffsetLength);
  return kTransmissionTimeOffsetLength;
}

uint8_t RTPSender::BuildAbsoluteSendTimeExtension(
    uint8_t* data_buffer) const {
  // Absolute send time in RTP streams.
  //
  // The absolute send time is signaled to the receiver in-band using the
  // general mechanism for RTP header extensions [RFC5285]. The payload
  // of this extension (the transmitted value) is a 24-bit unsigned integer
  // containing the sender's current time in seconds as a fixed point number
  // with 18 bits fractional part.
  //
  // The form of the absolute send time extension block:
  //
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |  ID   | len=2 |              absolute send time               |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Get id defined by user.
  uint8_t id;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAbsoluteSendTime,
                                      &id) != 0) {
    // Not registered.
    return 0;
  }
  size_t pos = 0;
  const uint8_t len = 2;
  data_buffer[pos++] = (id << 4) + len;
  ModuleRTPUtility::AssignUWord24ToBuffer(data_buffer + pos,
                                          absolute_send_time_);
  pos += 3;
  assert(pos == kAbsoluteSendTimeLength);
  return kAbsoluteSendTimeLength;
}

bool RTPSender::UpdateTransmissionTimeOffset(
    uint8_t *rtp_packet, const uint16_t rtp_packet_length,
    const RTPHeader &rtp_header, const int64_t time_diff_ms) const {
  CriticalSectionScoped cs(send_critsect_);

  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(
          kRtpExtensionTransmissionTimeOffset);
  if (extension_block_pos < 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update transmission time offset, not registered.");
    return false;
  }
  int block_pos = 12 + rtp_header.numCSRCs + extension_block_pos;
  if (rtp_packet_length < block_pos + kTransmissionTimeOffsetLength ||
      rtp_header.headerLength <
          block_pos + kTransmissionTimeOffsetLength) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update transmission time offset, invalid length.");
    return false;
  }
  // Verify that header contains extension.
  if (!((rtp_packet[12 + rtp_header.numCSRCs] == 0xBE) &&
        (rtp_packet[12 + rtp_header.numCSRCs + 1] == 0xDE))) {
    WEBRTC_TRACE(
        kTraceStream, kTraceRtpRtcp, id_,
        "Failed to update transmission time offset, hdr extension not found.");
    return false;
  }
  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionTransmissionTimeOffset,
                                      &id) != 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update transmission time offset, no id.");
    return false;
  }
  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 2;
  if (rtp_packet[block_pos] != first_block_byte) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update transmission time offset.");
    return false;
  }
  // Update transmission offset field (converting to a 90 kHz timestamp).
  ModuleRTPUtility::AssignUWord24ToBuffer(rtp_packet + block_pos + 1,
                                          time_diff_ms * 90);  // RTP timestamp.
  return true;
}

bool RTPSender::UpdateAbsoluteSendTime(
    uint8_t *rtp_packet, const uint16_t rtp_packet_length,
    const RTPHeader &rtp_header, const int64_t now_ms) const {
  CriticalSectionScoped cs(send_critsect_);

  // Get length until start of header extension block.
  int extension_block_pos =
      rtp_header_extension_map_.GetLengthUntilBlockStartInBytes(
          kRtpExtensionAbsoluteSendTime);
  if (extension_block_pos < 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update absolute send time, not registered.");
    return false;
  }
  int block_pos = 12 + rtp_header.numCSRCs + extension_block_pos;
  if (rtp_packet_length < block_pos + kAbsoluteSendTimeLength ||
      rtp_header.headerLength < block_pos + kAbsoluteSendTimeLength) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update absolute send time, invalid length.");
    return false;
  }
  // Verify that header contains extension.
  if (!((rtp_packet[12 + rtp_header.numCSRCs] == 0xBE) &&
        (rtp_packet[12 + rtp_header.numCSRCs + 1] == 0xDE))) {
    WEBRTC_TRACE(
        kTraceStream, kTraceRtpRtcp, id_,
        "Failed to update absolute send time, hdr extension not found.");
    return false;
  }
  // Get id.
  uint8_t id = 0;
  if (rtp_header_extension_map_.GetId(kRtpExtensionAbsoluteSendTime,
                                      &id) != 0) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update absolute send time, no id.");
    return false;
  }
  // Verify first byte in block.
  const uint8_t first_block_byte = (id << 4) + 2;
  if (rtp_packet[block_pos] != first_block_byte) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, id_,
                 "Failed to update absolute send time.");
    return false;
  }
  // Update absolute send time field (convert ms to 24-bit unsigned with 18 bit
  // fractional part).
  ModuleRTPUtility::AssignUWord24ToBuffer(rtp_packet + block_pos + 1,
                                          ((now_ms << 18) / 1000) & 0x00ffffff);
  return true;
}

void RTPSender::SetSendingStatus(bool enabled) {
  if (enabled) {
    uint32_t frequency_hz = SendPayloadFrequency();
    uint32_t RTPtime = ModuleRTPUtility::GetCurrentRTP(clock_, frequency_hz);

    // Will be ignored if it's already configured via API.
    SetStartTimestamp(RTPtime, false);
  } else {
    if (!ssrc_forced_) {
      // Generate a new SSRC.
      ssrc_db_.ReturnSSRC(ssrc_);
      ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
    }
    // Don't initialize seq number if SSRC passed externally.
    if (!sequence_number_forced_ && !ssrc_forced_) {
      // Generate a new sequence number.
      sequence_number_ =
          rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);  // NOLINT
    }
  }
}

void RTPSender::SetSendingMediaStatus(const bool enabled) {
  CriticalSectionScoped cs(send_critsect_);
  sending_media_ = enabled;
}

bool RTPSender::SendingMedia() const {
  CriticalSectionScoped cs(send_critsect_);
  return sending_media_;
}

uint32_t RTPSender::Timestamp() const {
  CriticalSectionScoped cs(send_critsect_);
  return timestamp_;
}

void RTPSender::SetStartTimestamp(uint32_t timestamp, bool force) {
  CriticalSectionScoped cs(send_critsect_);
  if (force) {
    start_time_stamp_forced_ = force;
    start_time_stamp_ = timestamp;
  } else {
    if (!start_time_stamp_forced_) {
      start_time_stamp_ = timestamp;
    }
  }
}

uint32_t RTPSender::StartTimestamp() const {
  CriticalSectionScoped cs(send_critsect_);
  return start_time_stamp_;
}

uint32_t RTPSender::GenerateNewSSRC() {
  // If configured via API, return 0.
  CriticalSectionScoped cs(send_critsect_);

  if (ssrc_forced_) {
    return 0;
  }
  ssrc_ = ssrc_db_.CreateSSRC();  // Can't be 0.
  return ssrc_;
}

void RTPSender::SetSSRC(uint32_t ssrc) {
  // This is configured via the API.
  CriticalSectionScoped cs(send_critsect_);

  if (ssrc_ == ssrc && ssrc_forced_) {
    return;  // Since it's same ssrc, don't reset anything.
  }
  ssrc_forced_ = true;
  ssrc_db_.ReturnSSRC(ssrc_);
  ssrc_db_.RegisterSSRC(ssrc);
  ssrc_ = ssrc;
  if (!sequence_number_forced_) {
    sequence_number_ =
        rand() / (RAND_MAX / MAX_INIT_RTP_SEQ_NUMBER);  // NOLINT
  }
}

uint32_t RTPSender::SSRC() const {
  CriticalSectionScoped cs(send_critsect_);
  return ssrc_;
}

void RTPSender::SetCSRCStatus(const bool include) {
  include_csrcs_ = include;
}

void RTPSender::SetCSRCs(const uint32_t arr_of_csrc[kRtpCsrcSize],
                         const uint8_t arr_length) {
  assert(arr_length <= kRtpCsrcSize);
  CriticalSectionScoped cs(send_critsect_);

  for (int i = 0; i < arr_length; i++) {
    csrcs_[i] = arr_of_csrc[i];
  }
  num_csrcs_ = arr_length;
}

int32_t RTPSender::CSRCs(uint32_t arr_of_csrc[kRtpCsrcSize]) const {
  assert(arr_of_csrc);
  CriticalSectionScoped cs(send_critsect_);
  for (int i = 0; i < num_csrcs_ && i < kRtpCsrcSize; i++) {
    arr_of_csrc[i] = csrcs_[i];
  }
  return num_csrcs_;
}

void RTPSender::SetSequenceNumber(uint16_t seq) {
  CriticalSectionScoped cs(send_critsect_);
  sequence_number_forced_ = true;
  sequence_number_ = seq;
}

uint16_t RTPSender::SequenceNumber() const {
  CriticalSectionScoped cs(send_critsect_);
  return sequence_number_;
}

// Audio.
int32_t RTPSender::SendTelephoneEvent(const uint8_t key,
                                      const uint16_t time_ms,
                                      const uint8_t level) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SendTelephoneEvent(key, time_ms, level);
}

bool RTPSender::SendTelephoneEventActive(int8_t *telephone_event) const {
  if (!audio_configured_) {
    return false;
  }
  return audio_->SendTelephoneEventActive(*telephone_event);
}

int32_t RTPSender::SetAudioPacketSize(
    const uint16_t packet_size_samples) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetAudioPacketSize(packet_size_samples);
}

int32_t RTPSender::SetAudioLevelIndicationStatus(const bool enable,
                                                 const uint8_t ID) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetAudioLevelIndicationStatus(enable, ID);
}

int32_t RTPSender::AudioLevelIndicationStatus(bool *enable,
                                              uint8_t* id) const {
  return audio_->AudioLevelIndicationStatus(*enable, *id);
}

int32_t RTPSender::SetAudioLevel(const uint8_t level_d_bov) {
  return audio_->SetAudioLevel(level_d_bov);
}

int32_t RTPSender::SetRED(const int8_t payload_type) {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->SetRED(payload_type);
}

int32_t RTPSender::RED(int8_t *payload_type) const {
  if (!audio_configured_) {
    return -1;
  }
  return audio_->RED(*payload_type);
}

// Video
VideoCodecInformation *RTPSender::CodecInformationVideo() {
  if (audio_configured_) {
    return NULL;
  }
  return video_->CodecInformationVideo();
}

RtpVideoCodecTypes RTPSender::VideoCodecType() const {
  assert(!audio_configured_ && "Sender is an audio stream!");
  return video_->VideoCodecType();
}

uint32_t RTPSender::MaxConfiguredBitrateVideo() const {
  if (audio_configured_) {
    return 0;
  }
  return video_->MaxConfiguredBitrateVideo();
}

int32_t RTPSender::SendRTPIntraRequest() {
  if (audio_configured_) {
    return -1;
  }
  return video_->SendRTPIntraRequest();
}

int32_t RTPSender::SetGenericFECStatus(
    const bool enable, const uint8_t payload_type_red,
    const uint8_t payload_type_fec) {
  if (audio_configured_) {
    return -1;
  }
  return video_->SetGenericFECStatus(enable, payload_type_red,
                                     payload_type_fec);
}

int32_t RTPSender::GenericFECStatus(
    bool *enable, uint8_t *payload_type_red,
    uint8_t *payload_type_fec) const {
  if (audio_configured_) {
    return -1;
  }
  return video_->GenericFECStatus(
      *enable, *payload_type_red, *payload_type_fec);
}

int32_t RTPSender::SetFecParameters(
    const FecProtectionParams *delta_params,
    const FecProtectionParams *key_params) {
  if (audio_configured_) {
    return -1;
  }
  return video_->SetFecParameters(delta_params, key_params);
}

void RTPSender::BuildRtxPacket(uint8_t* buffer, uint16_t* length,
                               uint8_t* buffer_rtx) {
  CriticalSectionScoped cs(send_critsect_);
  uint8_t* data_buffer_rtx = buffer_rtx;
  // Add RTX header.
  ModuleRTPUtility::RTPHeaderParser rtp_parser(
      reinterpret_cast<const uint8_t *>(buffer), *length);

  RTPHeader rtp_header;
  rtp_parser.Parse(rtp_header);

  // Add original RTP header.
  memcpy(data_buffer_rtx, buffer, rtp_header.headerLength);

  // Replace payload type, if a specific type is set for RTX.
  if (payload_type_rtx_ != -1) {
    data_buffer_rtx[1] = static_cast<uint8_t>(payload_type_rtx_);
    if (rtp_header.markerBit)
      data_buffer_rtx[1] |= kRtpMarkerBitMask;
  }

  // Replace sequence number.
  uint8_t *ptr = data_buffer_rtx + 2;
  ModuleRTPUtility::AssignUWord16ToBuffer(ptr, sequence_number_rtx_++);

  // Replace SSRC.
  ptr += 6;
  ModuleRTPUtility::AssignUWord32ToBuffer(ptr, ssrc_rtx_);

  // Add OSN (original sequence number).
  ptr = data_buffer_rtx + rtp_header.headerLength;
  ModuleRTPUtility::AssignUWord16ToBuffer(ptr, rtp_header.sequenceNumber);
  ptr += 2;

  // Add original payload data.
  memcpy(ptr, buffer + rtp_header.headerLength,
         *length - rtp_header.headerLength);
  *length += 2;
}

}  // namespace webrtc
