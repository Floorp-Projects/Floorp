/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

RTPPayloadRegistry::RTPPayloadRegistry(
    const int32_t id,
    RTPPayloadStrategy* rtp_payload_strategy)
    : id_(id),
      rtp_payload_strategy_(rtp_payload_strategy),
      red_payload_type_(-1),
      last_received_payload_type_(-1),
      last_received_media_payload_type_(-1) {
}

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
    payload->audio = false;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
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

int32_t RTPPayloadRegistry::PayloadTypeToPayload(
  const uint8_t payload_type,
  ModuleRTPUtility::Payload*& payload) const {

  ModuleRTPUtility::PayloadTypeMap::const_iterator it =
    payload_type_map_.find(payload_type);

  // Check that this is a registered payload type.
  if (it == payload_type_map_.end()) {
    return -1;
  }
  payload = it->second;
  return 0;
}

bool RTPPayloadRegistry::ReportMediaPayloadType(
    uint8_t media_payload_type) {
  if (last_received_media_payload_type_ == media_payload_type) {
    // Media type unchanged.
    return true;
  }
  last_received_media_payload_type_ = media_payload_type;
  return false;
}

class RTPPayloadAudioStrategy : public RTPPayloadStrategy {
 public:
  bool CodecsMustBeUnique() const { return true; }

  bool PayloadIsCompatible(
       const ModuleRTPUtility::Payload& payload,
       const uint32_t frequency,
       const uint8_t channels,
       const uint32_t rate) const {
    return
        payload.audio &&
        payload.typeSpecific.Audio.frequency == frequency &&
        payload.typeSpecific.Audio.channels == channels &&
        (payload.typeSpecific.Audio.rate == rate ||
            payload.typeSpecific.Audio.rate == 0 || rate == 0);
  }

  void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const uint32_t rate) const {
    payload->typeSpecific.Audio.rate = rate;
  }

  ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const {
    ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    payload->typeSpecific.Audio.frequency = frequency;
    payload->typeSpecific.Audio.channels = channels;
    payload->typeSpecific.Audio.rate = rate;
    payload->audio = true;
    return payload;
  }
};

class RTPPayloadVideoStrategy : public RTPPayloadStrategy {
 public:
  bool CodecsMustBeUnique() const { return false; }

  bool PayloadIsCompatible(
      const ModuleRTPUtility::Payload& payload,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const {
    return !payload.audio;
  }

  void UpdatePayloadRate(
      ModuleRTPUtility::Payload* payload,
      const uint32_t rate) const {
    payload->typeSpecific.Video.maxRate = rate;
  }

  ModuleRTPUtility::Payload* CreatePayloadType(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate) const {
    RtpVideoCodecTypes videoType = kRtpGenericVideo;
    if (ModuleRTPUtility::StringCompare(payloadName, "VP8", 3)) {
      videoType = kRtpVp8Video;
    } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
      videoType = kRtpGenericVideo;
    } else if (ModuleRTPUtility::StringCompare(payloadName, "ULPFEC", 6)) {
      videoType = kRtpFecVideo;
    } else {
      videoType = kRtpGenericVideo;
    }
    ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;

    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
    payload->typeSpecific.Video.videoCodecType = videoType;
    payload->typeSpecific.Video.maxRate = rate;
    payload->audio = false;
    return payload;
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
