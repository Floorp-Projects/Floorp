/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

RTPPayloadRegistry::RTPPayloadRegistry(
    const int32_t id,
    RTPPayloadStrategy* rtp_payload_strategy)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      id_(id),
      rtp_payload_strategy_(rtp_payload_strategy),
      red_payload_type_(-1),
      ulpfec_payload_type_(-1),
      incoming_payload_type_(-1),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1),
      rtx_(false),
      payload_type_rtx_(-1),
      ssrc_rtx_(0) {}

RTPPayloadRegistry::~RTPPayloadRegistry() {
  while (!payload_type_map_.empty()) {
    ModuleRTPUtility::PayloadTypeMap::iterator it = payload_type_map_.begin();
    delete it->second;
    payload_type_map_.erase(it);
  }
}

int32_t RTPPayloadRegistry::RegisterReceivePayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payload_type,
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate,
    bool* created_new_payload) {
  assert(payload_type >= 0);
  assert(payload_name);
  *created_new_payload = false;

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

  CriticalSectionScoped cs(crit_sect_.get());

  ModuleRTPUtility::PayloadTypeMap::iterator it =
    payload_type_map_.find(payload_type);

  if (it != payload_type_map_.end()) {
    // We already use this payload type.
    ModuleRTPUtility::Payload* payload = it->second;

    assert(payload);

    size_t name_length = strlen(payload->name);

    // Check if it's the same as we already have.
    // If same, ignore sending an error.
    if (payload_name_length == name_length &&
        ModuleRTPUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
      if (rtp_payload_strategy_->PayloadIsCompatible(*payload, frequency,
                                                     channels, rate)) {
        rtp_payload_strategy_->UpdatePayloadRate(payload, rate);
        return 0;
      }
    }
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument payload_type:%d already registered",
                 __FUNCTION__, payload_type);
    return -1;
  }

  if (rtp_payload_strategy_->CodecsMustBeUnique()) {
    DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
        payload_name, payload_name_length, frequency, channels, rate);
  }

  ModuleRTPUtility::Payload* payload = NULL;

  // Save the RED payload type. Used in both audio and video.
  if (ModuleRTPUtility::StringCompare(payload_name, "red", 3)) {
    red_payload_type_ = payload_type;
    payload = new ModuleRTPUtility::Payload;
    memset(payload, 0, sizeof(*payload));
    payload->audio = false;
    strncpy(payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1);
  } else if (ModuleRTPUtility::StringCompare(payload_name, "ulpfec", 3)) {
    ulpfec_payload_type_ = payload_type;
    payload = new ModuleRTPUtility::Payload;
    memset(payload, 0, sizeof(*payload));
    payload->audio = false;
    strncpy(payload->name, payload_name, RTP_PAYLOAD_NAME_SIZE - 1);
  } else {
    *created_new_payload = true;
    payload = rtp_payload_strategy_->CreatePayloadType(
        payload_name, payload_type, frequency, channels, rate);
  }
  payload_type_map_[payload_type] = payload;

  // Successful set of payload type, clear the value of last received payload
  // type since it might mean something else.
  last_received_payload_type_ = -1;
  last_received_media_payload_type_ = -1;
  return 0;
}

int32_t RTPPayloadRegistry::DeRegisterReceivePayload(
    const int8_t payload_type) {
  CriticalSectionScoped cs(crit_sect_.get());
  ModuleRTPUtility::PayloadTypeMap::iterator it =
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

// There can't be several codecs with the same rate, frequency and channels
// for audio codecs, but there can for video.
// Always called from within a critical section.
void RTPPayloadRegistry::DeregisterAudioCodecOrRedTypeRegardlessOfPayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const size_t payload_name_length,
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate) {
  ModuleRTPUtility::PayloadTypeMap::iterator iterator =
      payload_type_map_.begin();
  for (; iterator != payload_type_map_.end(); ++iterator) {
    ModuleRTPUtility::Payload* payload = iterator->second;
    size_t name_length = strlen(payload->name);

    if (payload_name_length == name_length
        && ModuleRTPUtility::StringCompare(payload->name, payload_name,
                                           payload_name_length)) {
      // We found the payload name in the list.
      // If audio, check frequency and rate.
      if (payload->audio) {
        if (rtp_payload_strategy_->PayloadIsCompatible(*payload, frequency,
                                                       channels, rate)) {
          // Remove old setting.
          delete payload;
          payload_type_map_.erase(iterator);
          break;
        }
      } else if (ModuleRTPUtility::StringCompare(payload_name, "red", 3)) {
        delete payload;
        payload_type_map_.erase(iterator);
        break;
      }
    }
  }
}

int32_t RTPPayloadRegistry::ReceivePayloadType(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const uint32_t frequency,
    const uint8_t channels,
    const uint32_t rate,
    int8_t* payload_type) const {
  if (payload_type == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  size_t payload_name_length = strlen(payload_name);

  CriticalSectionScoped cs(crit_sect_.get());

  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
      payload_type_map_.begin();

  for (; it != payload_type_map_.end(); ++it) {
    ModuleRTPUtility::Payload* payload = it->second;
    assert(payload);

    size_t name_length = strlen(payload->name);
    if (payload_name_length == name_length &&
        ModuleRTPUtility::StringCompare(
            payload->name, payload_name, payload_name_length)) {
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
  }
  return -1;
}

void RTPPayloadRegistry::SetRtxStatus(bool enable, uint32_t ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  rtx_ = enable;
  ssrc_rtx_ = ssrc;
}

bool RTPPayloadRegistry::RtxEnabled() const {
  CriticalSectionScoped cs(crit_sect_.get());
  return rtx_;
}

bool RTPPayloadRegistry::IsRtx(const RTPHeader& header) const {
  CriticalSectionScoped cs(crit_sect_.get());
  return IsRtxInternal(header);
}

bool RTPPayloadRegistry::IsRtxInternal(const RTPHeader& header) const {
  return rtx_ && ssrc_rtx_ == header.ssrc;
}

bool RTPPayloadRegistry::RestoreOriginalPacket(uint8_t** restored_packet,
                                               const uint8_t* packet,
                                               int* packet_length,
                                               uint32_t original_ssrc,
                                               const RTPHeader& header) const {
  if (kRtxHeaderSize + header.headerLength > *packet_length) {
    return false;
  }
  const uint8_t* rtx_header = packet + header.headerLength;
  uint16_t original_sequence_number = (rtx_header[0] << 8) + rtx_header[1];

  // Copy the packet into the restored packet, except for the RTX header.
  memcpy(*restored_packet, packet, header.headerLength);
  memcpy(*restored_packet + header.headerLength,
         packet + header.headerLength + kRtxHeaderSize,
         *packet_length - header.headerLength - kRtxHeaderSize);
  *packet_length -= kRtxHeaderSize;

  // Replace the SSRC and the sequence number with the originals.
  ModuleRTPUtility::AssignUWord16ToBuffer(*restored_packet + 2,
                                          original_sequence_number);
  ModuleRTPUtility::AssignUWord32ToBuffer(*restored_packet + 8, original_ssrc);

  CriticalSectionScoped cs(crit_sect_.get());

  if (payload_type_rtx_ != -1) {
    if (header.payloadType == payload_type_rtx_ &&
        incoming_payload_type_ != -1) {
      (*restored_packet)[1] = static_cast<uint8_t>(incoming_payload_type_);
      if (header.markerBit) {
        (*restored_packet)[1] |= kRtpMarkerBitMask;  // Marker bit is set.
      }
    } else {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "Incorrect RTX configuration, dropping packet.");
      return false;
    }
  }
  return true;
}

void RTPPayloadRegistry::SetRtxPayloadType(int payload_type) {
  CriticalSectionScoped cs(crit_sect_.get());
  payload_type_rtx_ = payload_type;
}

bool RTPPayloadRegistry::IsRed(const RTPHeader& header) const {
  CriticalSectionScoped cs(crit_sect_.get());
  return red_payload_type_ == header.payloadType;
}

bool RTPPayloadRegistry::IsEncapsulated(const RTPHeader& header) const {
  return IsRed(header) || IsRtx(header);
}

bool RTPPayloadRegistry::GetPayloadSpecifics(uint8_t payload_type,
                                             PayloadUnion* payload) const {
  CriticalSectionScoped cs(crit_sect_.get());
  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
    payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return false;
  }
  *payload = it->second->typeSpecific;
  return true;
}

int RTPPayloadRegistry::GetPayloadTypeFrequency(
    uint8_t payload_type) const {
  ModuleRTPUtility::Payload* payload;
  if (!PayloadTypeToPayload(payload_type, payload)) {
    return -1;
  }
  CriticalSectionScoped cs(crit_sect_.get());
  return rtp_payload_strategy_->GetPayloadTypeFrequency(*payload);
}

bool RTPPayloadRegistry::PayloadTypeToPayload(
  const uint8_t payload_type,
  ModuleRTPUtility::Payload*& payload) const {
  CriticalSectionScoped cs(crit_sect_.get());

  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
    payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return false;
  }

  payload = it->second;
  return true;
}

void RTPPayloadRegistry::SetIncomingPayloadType(const RTPHeader& header) {
  CriticalSectionScoped cs(crit_sect_.get());
  if (!IsRtxInternal(header))
    incoming_payload_type_ = header.payloadType;
}

bool RTPPayloadRegistry::ReportMediaPayloadType(uint8_t media_payload_type) {
  CriticalSectionScoped cs(crit_sect_.get());
  if (last_received_media_payload_type_ == media_payload_type) {
    // Media type unchanged.
    return true;
  }
  last_received_media_payload_type_ = media_payload_type;
  return false;
}

class RTPPayloadAudioStrategy : public RTPPayloadStrategy {
 public:
  virtual bool CodecsMustBeUnique() const OVERRIDE { return true; }

  virtual bool PayloadIsCompatible(
       const ModuleRTPUtility::Payload& payload,
       const uint32_t frequency,
       const uint8_t channels,
       const uint32_t rate) const OVERRIDE {
    return
        payload.audio &&
        payload.typeSpecific.Audio.frequency == frequency &&
        payload.typeSpecific.Audio.channels == channels &&
        (payload.typeSpecific.Audio.rate == rate ||
            payload.typeSpecific.Audio.rate == 0 || rate == 0);
  }

  virtual void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const uint32_t rate) const OVERRIDE {
    payload->typeSpecific.Audio.rate = rate;
  }

  virtual ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const OVERRIDE {
    ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    assert(frequency >= 1000);
    payload->typeSpecific.Audio.frequency = frequency;
    payload->typeSpecific.Audio.channels = channels;
    payload->typeSpecific.Audio.rate = rate;
    payload->audio = true;
    return payload;
  }

  int GetPayloadTypeFrequency(
      const ModuleRTPUtility::Payload& payload) const {
    return payload.typeSpecific.Audio.frequency;
  }
};

class RTPPayloadVideoStrategy : public RTPPayloadStrategy {
 public:
  virtual bool CodecsMustBeUnique() const OVERRIDE { return false; }

  virtual bool PayloadIsCompatible(
      const ModuleRTPUtility::Payload& payload,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const OVERRIDE {
    return !payload.audio;
  }

  virtual void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const uint32_t rate) const OVERRIDE {
    payload->typeSpecific.Video.maxRate = rate;
  }

  virtual ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const OVERRIDE {
    RtpVideoCodecTypes videoType = kRtpVideoGeneric;
    if (ModuleRTPUtility::StringCompare(payloadName, "VP8", 3)) {
      videoType = kRtpVideoVp8;
    } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
      videoType = kRtpVideoGeneric;
    } else if (ModuleRTPUtility::StringCompare(payloadName, "ULPFEC", 6)) {
      videoType = kRtpVideoNone;
    } else {
      videoType = kRtpVideoGeneric;
    }
    ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;

    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    payload->typeSpecific.Video.videoCodecType = videoType;
    payload->typeSpecific.Video.maxRate = rate;
    payload->audio = false;
    return payload;
  }

  int GetPayloadTypeFrequency(
      const ModuleRTPUtility::Payload& payload) const {
    return kVideoPayloadTypeFrequency;
  }
};

RTPPayloadStrategy* RTPPayloadStrategy::CreateStrategy(
    const bool handling_audio) {
  if (handling_audio) {
    return new RTPPayloadAudioStrategy();
  } else {
    return new RTPPayloadVideoStrategy();
  }
}

}  // namespace webrtc
