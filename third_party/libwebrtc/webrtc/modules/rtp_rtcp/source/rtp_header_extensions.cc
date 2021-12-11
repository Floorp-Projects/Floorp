/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_header_extensions.h"

#include "modules/rtp_rtcp/include/rtp_cvo.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
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
constexpr RTPExtensionType AbsoluteSendTime::kId;
constexpr uint8_t AbsoluteSendTime::kValueSizeBytes;
constexpr const char AbsoluteSendTime::kUri[];

bool AbsoluteSendTime::Parse(rtc::ArrayView<const uint8_t> data,
                             uint32_t* time_24bits) {
  if (data.size() != 3)
    return false;
  *time_24bits = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool AbsoluteSendTime::Write(uint8_t* data, uint32_t time_24bits) {
  RTC_DCHECK_LE(time_24bits, 0x00FFFFFF);
  ByteWriter<uint32_t, 3>::WriteBigEndian(data, time_24bits);
  return true;
}

// An RTP Header Extension for Client-to-Mixer Audio Level Indication
//
// https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/
//
// The form of the audio level extension block:
//
//    0                   1
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=0 |V|   level     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
constexpr RTPExtensionType AudioLevel::kId;
constexpr uint8_t AudioLevel::kValueSizeBytes;
constexpr const char AudioLevel::kUri[];

bool AudioLevel::Parse(rtc::ArrayView<const uint8_t> data,
                       bool* voice_activity,
                       uint8_t* audio_level) {
  if (data.size() != 1)
    return false;
  *voice_activity = (data[0] & 0x80) != 0;
  *audio_level = data[0] & 0x7F;
  return true;
}

bool AudioLevel::Write(uint8_t* data,
                       bool voice_activity,
                       uint8_t audio_level) {
  RTC_CHECK_LE(audio_level, 0x7f);
  data[0] = (voice_activity ? 0x80 : 0x00) | audio_level;
  return true;
}

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
constexpr RTPExtensionType TransmissionOffset::kId;
constexpr uint8_t TransmissionOffset::kValueSizeBytes;
constexpr const char TransmissionOffset::kUri[];

bool TransmissionOffset::Parse(rtc::ArrayView<const uint8_t> data,
                               int32_t* rtp_time) {
  if (data.size() != 3)
    return false;
  *rtp_time = ByteReader<int32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool TransmissionOffset::Write(uint8_t* data, int32_t rtp_time) {
  RTC_DCHECK_LE(rtp_time, 0x00ffffff);
  ByteWriter<int32_t, 3>::WriteBigEndian(data, rtp_time);
  return true;
}

//   0                   1                   2
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=1   |transport wide sequence number |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType TransportSequenceNumber::kId;
constexpr uint8_t TransportSequenceNumber::kValueSizeBytes;
constexpr const char TransportSequenceNumber::kUri[];

bool TransportSequenceNumber::Parse(rtc::ArrayView<const uint8_t> data,
                                    uint16_t* value) {
  if (data.size() != 2)
    return false;
  *value = ByteReader<uint16_t>::ReadBigEndian(data.data());
  return true;
}

bool TransportSequenceNumber::Write(uint8_t* data, uint16_t value) {
  ByteWriter<uint16_t>::WriteBigEndian(data, value);
  return true;
}

// Coordination of Video Orientation in RTP streams.
//
// Coordination of Video Orientation consists in signaling of the current
// orientation of the image captured on the sender side to the receiver for
// appropriate rendering and displaying.
//
//    0                   1
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=0 |0 0 0 0 C F R R|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType VideoOrientation::kId;
constexpr uint8_t VideoOrientation::kValueSizeBytes;
constexpr const char VideoOrientation::kUri[];

bool VideoOrientation::Parse(rtc::ArrayView<const uint8_t> data,
                             VideoRotation* rotation) {
  if (data.size() != 1)
    return false;
  *rotation = ConvertCVOByteToVideoRotation(data[0]);
  return true;
}

bool VideoOrientation::Write(uint8_t* data, VideoRotation rotation) {
  data[0] = ConvertVideoRotationToCVOByte(rotation);
  return true;
}

bool VideoOrientation::Parse(rtc::ArrayView<const uint8_t> data,
                             uint8_t* value) {
  if (data.size() != 1)
    return false;
  *value = data[0];
  return true;
}

bool VideoOrientation::Write(uint8_t* data, uint8_t value) {
  data[0] = value;
  return true;
}

//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | len=2 |   MIN delay           |   MAX delay           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType PlayoutDelayLimits::kId;
constexpr uint8_t PlayoutDelayLimits::kValueSizeBytes;
constexpr const char PlayoutDelayLimits::kUri[];

bool PlayoutDelayLimits::Parse(rtc::ArrayView<const uint8_t> data,
                               PlayoutDelay* playout_delay) {
  RTC_DCHECK(playout_delay);
  if (data.size() != 3)
    return false;
  uint32_t raw = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  uint16_t min_raw = (raw >> 12);
  uint16_t max_raw = (raw & 0xfff);
  if (min_raw > max_raw)
    return false;
  playout_delay->min_ms = min_raw * kGranularityMs;
  playout_delay->max_ms = max_raw * kGranularityMs;
  return true;
}

bool PlayoutDelayLimits::Write(uint8_t* data,
                               const PlayoutDelay& playout_delay) {
  RTC_DCHECK_LE(0, playout_delay.min_ms);
  RTC_DCHECK_LE(playout_delay.min_ms, playout_delay.max_ms);
  RTC_DCHECK_LE(playout_delay.max_ms, kMaxMs);
  // Convert MS to value to be sent on extension header.
  uint32_t min_delay = playout_delay.min_ms / kGranularityMs;
  uint32_t max_delay = playout_delay.max_ms / kGranularityMs;
  ByteWriter<uint32_t, 3>::WriteBigEndian(data, (min_delay << 12) | max_delay);
  return true;
}

// CSRCAudioLevel
//  Sample Audio Level Encoding Using the One-Byte Header Format
//  Note that the range of len is 1 to 15 which is encoded as 0 to 14
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  ID   | len=2 |0|   level 1   |0|   level 2   |0|   level 3   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


constexpr RTPExtensionType CsrcAudioLevel::kId;
constexpr const char* CsrcAudioLevel::kUri;

bool CsrcAudioLevel::Parse(rtc::ArrayView<const uint8_t> data,
                           CsrcAudioLevelList* csrcAudioLevels) {
  if (data.size() < 1 || data.size() > kRtpCsrcSize)
    return false;
  csrcAudioLevels->numAudioLevels = data.size();
  for(uint8_t i = 0; i < csrcAudioLevels->numAudioLevels; i++) {
    // Ensure range is 0 to 127 inclusive
    csrcAudioLevels->arrOfAudioLevels[i] = 0x7f & data[i];
  }
  return true;
}

size_t CsrcAudioLevel::ValueSize(const CsrcAudioLevelList& csrcAudioLevels) {
  return csrcAudioLevels.numAudioLevels;
}

bool CsrcAudioLevel::Write(uint8_t* data,
                           const CsrcAudioLevelList& csrcAudioLevels) {
  RTC_DCHECK_GE(csrcAudioLevels.numAudioLevels, 0);
  for(uint8_t i = 0; i < csrcAudioLevels.numAudioLevels; i++) {
    data[i] = csrcAudioLevels.arrOfAudioLevels[i] & 0x7f;
  }
  // This extension if used must have at least one audio level
  return csrcAudioLevels.numAudioLevels;
}

// Video Content Type.
//
// E.g. default video or screenshare.
//
//    0                   1
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=0 | Content type  |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType VideoContentTypeExtension::kId;
constexpr uint8_t VideoContentTypeExtension::kValueSizeBytes;
constexpr const char VideoContentTypeExtension::kUri[];

bool VideoContentTypeExtension::Parse(rtc::ArrayView<const uint8_t> data,
                                      VideoContentType* content_type) {
  if (data.size() == 1 &&
      videocontenttypehelpers::IsValidContentType(data[0])) {
    *content_type = static_cast<VideoContentType>(data[0]);
    return true;
  }
  return false;
}

bool VideoContentTypeExtension::Write(uint8_t* data,
                                      VideoContentType content_type) {
  data[0] = static_cast<uint8_t>(content_type);
  return true;
}

// Video Timing.
// 6 timestamps in milliseconds counted from capture time stored in rtp header:
// encode start/finish, packetization complete, pacer exit and reserved for
// modification by the network modification. |flags| is a bitmask and has the
// following allowed values:
// 0 = Valid data, but no flags available (backwards compatibility)
// 1 = Frame marked as timing frame due to cyclic timer.
// 2 = Frame marked as timing frame due to size being outside limit.
// 255 = Invalid. The whole timing frame extension should be ignored.
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=12|     flags     |     encode start ms delta       |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    encode finish ms delta     |   packetizer finish ms delta    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |     pacer exit ms delta       |   network timestamp ms delta    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  network2 timestamp ms delta  |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

constexpr RTPExtensionType VideoTimingExtension::kId;
constexpr uint8_t VideoTimingExtension::kValueSizeBytes;
constexpr const char VideoTimingExtension::kUri[];

bool VideoTimingExtension::Parse(rtc::ArrayView<const uint8_t> data,
                                 VideoSendTiming* timing) {
  RTC_DCHECK(timing);
  // TODO(sprang): Deprecate support for old wire format.
  ptrdiff_t off = 0;
  switch (data.size()) {
    case kValueSizeBytes - 1:
      timing->flags = 0;
      off = 1;  // Old wire format without the flags field.
      break;
    case kValueSizeBytes:
      timing->flags = ByteReader<uint8_t>::ReadBigEndian(data.data());
      break;
    default:
      return false;
  }

  timing->encode_start_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kEncodeStartDeltaOffset - off);
  timing->encode_finish_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kEncodeFinishDeltaOffset - off);
  timing->packetization_finish_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kPacketizationFinishDeltaOffset - off);
  timing->pacer_exit_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kPacerExitDeltaOffset - off);
  timing->network_timestamp_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kNetworkTimestampDeltaOffset - off);
  timing->network2_timestamp_delta_ms = ByteReader<uint16_t>::ReadBigEndian(
      data.data() + VideoSendTiming::kNetwork2TimestampDeltaOffset - off);
  return true;
}

bool VideoTimingExtension::Write(uint8_t* data, const VideoSendTiming& timing) {
  ByteWriter<uint8_t>::WriteBigEndian(data + VideoSendTiming::kFlagsOffset,
                                      timing.flags);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kEncodeStartDeltaOffset,
      timing.encode_start_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kEncodeFinishDeltaOffset,
      timing.encode_finish_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kPacketizationFinishDeltaOffset,
      timing.packetization_finish_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kPacerExitDeltaOffset,
      timing.pacer_exit_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kNetworkTimestampDeltaOffset,
      timing.network_timestamp_delta_ms);
  ByteWriter<uint16_t>::WriteBigEndian(
      data + VideoSendTiming::kNetwork2TimestampDeltaOffset,
      timing.network2_timestamp_delta_ms);
  return true;
}

bool VideoTimingExtension::Write(uint8_t* data,
                                 uint16_t time_delta_ms,
                                 uint8_t offset) {
  RTC_DCHECK_LE(offset, kValueSizeBytes - sizeof(uint16_t));
  ByteWriter<uint16_t>::WriteBigEndian(data + offset, time_delta_ms);
  return true;
}

bool BaseRtpStringExtension::Parse(rtc::ArrayView<const uint8_t> data,
                                   StringRtpHeaderExtension* str) {
  if (data.empty() || data[0] == 0)  // Valid string extension can't be empty.
    return false;
  str->Set(data);
  RTC_DCHECK(!str->empty());
  return true;
}

bool BaseRtpStringExtension::Write(uint8_t* data,
                                   const StringRtpHeaderExtension& str) {
  RTC_DCHECK_GE(str.size(), 1);
  RTC_DCHECK_LE(str.size(), StringRtpHeaderExtension::kMaxSize);
  memcpy(data, str.data(), str.size());
  return true;
}

bool BaseRtpStringExtension::Parse(rtc::ArrayView<const uint8_t> data,
                                   std::string* str) {
  if (data.empty() || data[0] == 0)  // Valid string extension can't be empty.
    return false;
  const char* cstr = reinterpret_cast<const char*>(data.data());
  // If there is a \0 character in the middle of the |data|, treat it as end
  // of the string. Well-formed string extensions shouldn't contain it.
  str->assign(cstr, strnlen(cstr, data.size()));
  RTC_DCHECK(!str->empty());
  return true;
}

bool BaseRtpStringExtension::Write(uint8_t* data, const std::string& str) {
  RTC_DCHECK_GE(str.size(), 1);
  RTC_DCHECK_LE(str.size(), StringRtpHeaderExtension::kMaxSize);
  memcpy(data, str.data(), str.size());
  return true;
}

// Constant declarations for string RTP header extension types.

constexpr RTPExtensionType RtpStreamId::kId;
constexpr const char RtpStreamId::kUri[];

constexpr RTPExtensionType RepairedRtpStreamId::kId;
constexpr const char RepairedRtpStreamId::kUri[];

constexpr RTPExtensionType RtpMid::kId;
constexpr const char RtpMid::kUri[];

}  // namespace webrtc
