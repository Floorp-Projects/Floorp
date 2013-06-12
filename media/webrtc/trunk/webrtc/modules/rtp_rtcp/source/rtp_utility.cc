/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_utility.h"

#include <cassert>
#include <cmath>  // ceil
#include <cstring>  // memcpy

#if defined(_WIN32)
#include <Windows.h>  // FILETIME
#include <WinSock.h>  // timeval
#include <MMSystem.h>  // timeGetTime
#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_MAC))
#include <sys/time.h>  // gettimeofday
#include <time.h>
#endif
#if (defined(_DEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
#include <stdio.h>
#endif

#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"

#if (defined(_DEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
#define DEBUG_PRINT(...)           \
  {                                \
    char msg[256];                 \
    sprintf(msg, __VA_ARGS__);     \
    OutputDebugString(msg);        \
  }
#else
// special fix for visual 2003
#define DEBUG_PRINT(exp)        ((void)0)
#endif  // defined(_DEBUG) && defined(_WIN32)

namespace webrtc {

namespace ModuleRTPUtility {

enum {
  kRtcpMinHeaderLength = 4,
  kRtcpExpectedVersion = 2
};

/*
 * Time routines.
 */

uint32_t GetCurrentRTP(Clock* clock, uint32_t freq) {
  const bool use_global_clock = (clock == NULL);
  Clock* local_clock = clock;
  if (use_global_clock) {
    local_clock = Clock::GetRealTimeClock();
  }
  uint32_t secs = 0, frac = 0;
  local_clock->CurrentNtp(secs, frac);
  if (use_global_clock) {
    delete local_clock;
  }
  return ConvertNTPTimeToRTP(secs, frac, freq);
}

uint32_t ConvertNTPTimeToRTP(uint32_t NTPsec, uint32_t NTPfrac, uint32_t freq) {
  float ftemp = (float)NTPfrac / (float)NTP_FRAC;
  uint32_t tmp = (uint32_t)(ftemp * freq);
  return NTPsec * freq + tmp;
}

uint32_t ConvertNTPTimeToMS(uint32_t NTPsec, uint32_t NTPfrac) {
  int freq = 1000;
  float ftemp = (float)NTPfrac / (float)NTP_FRAC;
  uint32_t tmp = (uint32_t)(ftemp * freq);
  uint32_t MStime = NTPsec * freq + tmp;
  return MStime;
}

/*
 * Misc utility routines
 */

const uint8_t* GetPayloadData(const WebRtcRTPHeader* rtp_header,
                              const uint8_t* packet) {
  return packet + rtp_header->header.headerLength;
}

uint16_t GetPayloadDataLength(const WebRtcRTPHeader* rtp_header,
                              const uint16_t packet_length) {
  uint16_t length = packet_length - rtp_header->header.paddingLength -
      rtp_header->header.headerLength;
  return static_cast<uint16_t>(length);
}

#if defined(_WIN32)
bool StringCompare(const char* str1, const char* str2,
                   const uint32_t length) {
  return (_strnicmp(str1, str2, length) == 0) ? true : false;
}
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
bool StringCompare(const char* str1, const char* str2,
                   const uint32_t length) {
  return (strncasecmp(str1, str2, length) == 0) ? true : false;
}
#endif

#if !defined(WEBRTC_LITTLE_ENDIAN) && !defined(WEBRTC_BIG_ENDIAN)
#error Either WEBRTC_LITTLE_ENDIAN or WEBRTC_BIG_ENDIAN must be defined
#endif

/* for RTP/RTCP
    All integer fields are carried in network byte order, that is, most
    significant byte (octet) first.  AKA big-endian.
*/
void AssignUWord32ToBuffer(uint8_t* dataBuffer, uint32_t value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<uint8_t>(value >> 24);
  dataBuffer[1] = static_cast<uint8_t>(value >> 16);
  dataBuffer[2] = static_cast<uint8_t>(value >> 8);
  dataBuffer[3] = static_cast<uint8_t>(value);
#else
  uint32_t* ptr = reinterpret_cast<uint32_t*>(dataBuffer);
  ptr[0] = value;
#endif
}

void AssignUWord24ToBuffer(uint8_t* dataBuffer, uint32_t value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<uint8_t>(value >> 16);
  dataBuffer[1] = static_cast<uint8_t>(value >> 8);
  dataBuffer[2] = static_cast<uint8_t>(value);
#else
  dataBuffer[0] = static_cast<uint8_t>(value);
  dataBuffer[1] = static_cast<uint8_t>(value >> 8);
  dataBuffer[2] = static_cast<uint8_t>(value >> 16);
#endif
}

void AssignUWord16ToBuffer(uint8_t* dataBuffer, uint16_t value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<uint8_t>(value >> 8);
  dataBuffer[1] = static_cast<uint8_t>(value);
#else
  uint16_t* ptr = reinterpret_cast<uint16_t*>(dataBuffer);
  ptr[0] = value;
#endif
}

uint16_t BufferToUWord16(const uint8_t* dataBuffer) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  return (dataBuffer[0] << 8) + dataBuffer[1];
#else
  return *reinterpret_cast<const uint16_t*>(dataBuffer);
#endif
}

uint32_t BufferToUWord24(const uint8_t* dataBuffer) {
  return (dataBuffer[0] << 16) + (dataBuffer[1] << 8) + dataBuffer[2];
}

uint32_t BufferToUWord32(const uint8_t* dataBuffer) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  return (dataBuffer[0] << 24) + (dataBuffer[1] << 16) + (dataBuffer[2] << 8) +
      dataBuffer[3];
#else
  return *reinterpret_cast<const uint32_t*>(dataBuffer);
#endif
}

uint32_t pow2(uint8_t exp) {
  return 1 << exp;
}

void RTPPayload::SetType(RtpVideoCodecTypes videoType) {
  type = videoType;

  switch (type) {
    case kRtpGenericVideo:
      break;
    case kRtpVp8Video: {
      info.VP8.nonReferenceFrame = false;
      info.VP8.beginningOfPartition = false;
      info.VP8.partitionID = 0;
      info.VP8.hasPictureID = false;
      info.VP8.hasTl0PicIdx = false;
      info.VP8.hasTID = false;
      info.VP8.hasKeyIdx = false;
      info.VP8.pictureID = -1;
      info.VP8.tl0PicIdx = -1;
      info.VP8.tID = -1;
      info.VP8.layerSync = false;
      info.VP8.frameWidth = 0;
      info.VP8.frameHeight = 0;
      break;
    }
    default:
      break;
  }
}

RTPHeaderParser::RTPHeaderParser(const uint8_t* rtpData,
                                 const uint32_t rtpDataLength)
  : _ptrRTPDataBegin(rtpData),
    _ptrRTPDataEnd(rtpData ? (rtpData + rtpDataLength) : NULL) {
}

RTPHeaderParser::~RTPHeaderParser() {
}

bool RTPHeaderParser::RTCP() const {
  // 72 to 76 is reserved for RTP
  // 77 to 79 is not reserver but  they are not assigned we will block them
  // for RTCP 200 SR  == marker bit + 72
  // for RTCP 204 APP == marker bit + 76
  /*
  *       RTCP
  *
  * FIR      full INTRA-frame request             192     [RFC2032]   supported
  * NACK     negative acknowledgement             193     [RFC2032]
  * IJ       Extended inter-arrival jitter report 195     [RFC-ietf-avt-rtp-toff
  * set-07.txt] http://tools.ietf.org/html/draft-ietf-avt-rtp-toffset-07
  * SR       sender report                        200     [RFC3551]   supported
  * RR       receiver report                      201     [RFC3551]   supported
  * SDES     source description                   202     [RFC3551]   supported
  * BYE      goodbye                              203     [RFC3551]   supported
  * APP      application-defined                  204     [RFC3551]   ignored
  * RTPFB    Transport layer FB message           205     [RFC4585]   supported
  * PSFB     Payload-specific FB message          206     [RFC4585]   supported
  * XR       extended report                      207     [RFC3611]   supported
  */

  /* 205       RFC 5104
   * FMT 1      NACK       supported
   * FMT 2      reserved
   * FMT 3      TMMBR      supported
   * FMT 4      TMMBN      supported
   */

  /* 206      RFC 5104
  * FMT 1:     Picture Loss Indication (PLI)                      supported
  * FMT 2:     Slice Lost Indication (SLI)
  * FMT 3:     Reference Picture Selection Indication (RPSI)
  * FMT 4:     Full Intra Request (FIR) Command                   supported
  * FMT 5:     Temporal-Spatial Trade-off Request (TSTR)
  * FMT 6:     Temporal-Spatial Trade-off Notification (TSTN)
  * FMT 7:     Video Back Channel Message (VBCM)
  * FMT 15:    Application layer FB message
  */

  const ptrdiff_t length = _ptrRTPDataEnd - _ptrRTPDataBegin;
  if (length < kRtcpMinHeaderLength) {
    return false;
  }

  const uint8_t V  = _ptrRTPDataBegin[0] >> 6;
  if (V != kRtcpExpectedVersion) {
    return false;
  }

  const uint8_t  payloadType = _ptrRTPDataBegin[1];
  bool RTCP = false;
  switch (payloadType) {
    case 192:
      RTCP = true;
      break;
    case 193:
      // not supported
      // pass through and check for a potential RTP packet
      break;
    case 195:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
      RTCP = true;
      break;
  }
  return RTCP;
}

bool RTPHeaderParser::Parse(WebRtcRTPHeader& parsedPacket,
                            RtpHeaderExtensionMap* ptrExtensionMap) const {
  const ptrdiff_t length = _ptrRTPDataEnd - _ptrRTPDataBegin;

  if (length < 12) {
    return false;
  }

  // Version
  const uint8_t V  = _ptrRTPDataBegin[0] >> 6;
  // Padding
  const bool          P  = ((_ptrRTPDataBegin[0] & 0x20) == 0) ? false : true;
  // eXtension
  const bool          X  = ((_ptrRTPDataBegin[0] & 0x10) == 0) ? false : true;
  const uint8_t CC = _ptrRTPDataBegin[0] & 0x0f;
  const bool          M  = ((_ptrRTPDataBegin[1] & 0x80) == 0) ? false : true;

  const uint8_t PT = _ptrRTPDataBegin[1] & 0x7f;

  const uint16_t sequenceNumber = (_ptrRTPDataBegin[2] << 8) +
      _ptrRTPDataBegin[3];

  const uint8_t* ptr = &_ptrRTPDataBegin[4];

  uint32_t RTPTimestamp = *ptr++ << 24;
  RTPTimestamp += *ptr++ << 16;
  RTPTimestamp += *ptr++ << 8;
  RTPTimestamp += *ptr++;

  uint32_t SSRC = *ptr++ << 24;
  SSRC += *ptr++ << 16;
  SSRC += *ptr++ << 8;
  SSRC += *ptr++;

  if (V != 2) {
    return false;
  }

  const uint8_t CSRCocts = CC * 4;

  if ((ptr + CSRCocts) > _ptrRTPDataEnd) {
    return false;
  }

  parsedPacket.header.markerBit      = M;
  parsedPacket.header.payloadType    = PT;
  parsedPacket.header.sequenceNumber = sequenceNumber;
  parsedPacket.header.timestamp      = RTPTimestamp;
  parsedPacket.header.ssrc           = SSRC;
  parsedPacket.header.numCSRCs       = CC;
  parsedPacket.header.paddingLength  = P ? *(_ptrRTPDataEnd - 1) : 0;

  for (unsigned int i = 0; i < CC; ++i) {
    uint32_t CSRC = *ptr++ << 24;
    CSRC += *ptr++ << 16;
    CSRC += *ptr++ << 8;
    CSRC += *ptr++;
    parsedPacket.header.arrOfCSRCs[i] = CSRC;
  }
  parsedPacket.type.Audio.numEnergy = parsedPacket.header.numCSRCs;

  parsedPacket.header.headerLength   = 12 + CSRCocts;

  // If in effect, MAY be omitted for those packets for which the offset
  // is zero.
  parsedPacket.extension.transmissionTimeOffset = 0;

  if (X) {
    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */
    const ptrdiff_t remain = _ptrRTPDataEnd - ptr;
    if (remain < 4) {
      return false;
    }

    parsedPacket.header.headerLength += 4;

    uint16_t definedByProfile = *ptr++ << 8;
    definedByProfile += *ptr++;

    uint16_t XLen = *ptr++ << 8;
    XLen += *ptr++; // in 32 bit words
    XLen *= 4; // in octs

    if (remain < (4 + XLen)) {
      return false;
    }
    if (definedByProfile == RTP_ONE_BYTE_HEADER_EXTENSION) {
      const uint8_t* ptrRTPDataExtensionEnd = ptr + XLen;
      ParseOneByteExtensionHeader(parsedPacket,
                                  ptrExtensionMap,
                                  ptrRTPDataExtensionEnd,
                                  ptr);
    }
    parsedPacket.header.headerLength += XLen;
  }
  return true;
}

void RTPHeaderParser::ParseOneByteExtensionHeader(
    WebRtcRTPHeader& parsedPacket,
    const RtpHeaderExtensionMap* ptrExtensionMap,
    const uint8_t* ptrRTPDataExtensionEnd,
    const uint8_t* ptr) const {
  if (!ptrExtensionMap) {
    return;
  }

  while (ptrRTPDataExtensionEnd - ptr > 0) {
    //  0
    //  0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // |  ID   |  len  |
    // +-+-+-+-+-+-+-+-+

    const uint8_t id = (*ptr & 0xf0) >> 4;
    const uint8_t len = (*ptr & 0x0f);
    ptr++;

    if (id == 15) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                   "Ext id: 15 encountered, parsing terminated.");
      return;
    }

    RTPExtensionType type;
    if (ptrExtensionMap->GetType(id, &type) != 0) {
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
                   "Failed to find extension id: %d", id);
      return;
    }

    switch (type) {
      case kRtpExtensionTransmissionTimeOffset: {
        if (len != 2) {
          WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                       "Incorrect transmission time offset len: %d", len);
          return;
        }
        //  0                   1                   2                   3
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | len=2 |              transmission offset              |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        int32_t transmissionTimeOffset = *ptr++ << 16;
        transmissionTimeOffset += *ptr++ << 8;
        transmissionTimeOffset += *ptr++;
        parsedPacket.extension.transmissionTimeOffset = transmissionTimeOffset;
        if (transmissionTimeOffset & 0x800000) {
          // Negative offset, correct sign for Word24 to Word32.
          parsedPacket.extension.transmissionTimeOffset |= 0xFF000000;
        }
        break;
      }
      case kRtpExtensionAudioLevel: {
        //   --- Only used for debugging ---
        //  0                   1                   2                   3
        //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // |  ID   | len=0 |V|   level     |      0x00     |      0x00     |
        // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //

        // Parse out the fields but only use it for debugging for now.
        // const uint8_t V = (*ptr & 0x80) >> 7;
        // const uint8_t level = (*ptr & 0x7f);
        // DEBUG_PRINT("RTP_AUDIO_LEVEL_UNIQUE_ID: ID=%u, len=%u, V=%u,
        // level=%u", ID, len, V, level);
        break;
      }
      default: {
        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
                     "Extension type not implemented.");
        return;
      }
    }
    uint8_t num_bytes = ParsePaddingBytes(ptrRTPDataExtensionEnd, ptr);
    ptr += num_bytes;
  }
}

uint8_t RTPHeaderParser::ParsePaddingBytes(
  const uint8_t* ptrRTPDataExtensionEnd,
  const uint8_t* ptr) const {

  uint8_t num_zero_bytes = 0;
  while (ptrRTPDataExtensionEnd - ptr > 0) {
    if (*ptr != 0) {
      return num_zero_bytes;
    }
    ptr++;
    num_zero_bytes++;
  }
  return num_zero_bytes;
}

// RTP payload parser
RTPPayloadParser::RTPPayloadParser(const RtpVideoCodecTypes videoType,
                                   const uint8_t* payloadData,
                                   uint16_t payloadDataLength,
                                   int32_t id)
  :
  _id(id),
  _dataPtr(payloadData),
  _dataLength(payloadDataLength),
  _videoType(videoType) {
}

RTPPayloadParser::~RTPPayloadParser() {
}

bool RTPPayloadParser::Parse(RTPPayload& parsedPacket) const {
  parsedPacket.SetType(_videoType);

  switch (_videoType) {
    case kRtpGenericVideo:
      return ParseGeneric(parsedPacket);
    case kRtpVp8Video:
      return ParseVP8(parsedPacket);
    default:
      return false;
  }
}

bool RTPPayloadParser::ParseGeneric(RTPPayload& /*parsedPacket*/) const {
  return false;
}

//
// VP8 format:
//
// Payload descriptor
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |X|R|N|S|PartID | (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// X:   |I|L|T|K|  RSV  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// I:   |   PictureID   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// L:   |   TL0PICIDX   | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
// T/K: |TID:Y| KEYIDX  | (OPTIONAL)
//      +-+-+-+-+-+-+-+-+
//
// Payload header (considered part of the actual payload, sent to decoder)
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |Size0|H| VER |P|
//      +-+-+-+-+-+-+-+-+
//      |      ...      |
//      +               +

bool RTPPayloadParser::ParseVP8(RTPPayload& parsedPacket) const {
  RTPPayloadVP8* vp8 = &parsedPacket.info.VP8;
  const uint8_t* dataPtr = _dataPtr;
  int dataLength = _dataLength;

  // Parse mandatory first byte of payload descriptor
  bool extension = (*dataPtr & 0x80) ? true : false;            // X bit
  vp8->nonReferenceFrame = (*dataPtr & 0x20) ? true : false;    // N bit
  vp8->beginningOfPartition = (*dataPtr & 0x10) ? true : false; // S bit
  vp8->partitionID = (*dataPtr & 0x0F);          // PartID field

  if (vp8->partitionID > 8) {
    // Weak check for corrupt data: PartID MUST NOT be larger than 8.
    return false;
  }

  // Advance dataPtr and decrease remaining payload size
  dataPtr++;
  dataLength--;

  if (extension) {
    const int parsedBytes = ParseVP8Extension(vp8, dataPtr, dataLength);
    if (parsedBytes < 0) return false;
    dataPtr += parsedBytes;
    dataLength -= parsedBytes;
  }

  if (dataLength <= 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "Error parsing VP8 payload descriptor; payload too short");
    return false;
  }

  // Read P bit from payload header (only at beginning of first partition)
  if (dataLength > 0 && vp8->beginningOfPartition && vp8->partitionID == 0) {
    parsedPacket.frameType = (*dataPtr & 0x01) ? kPFrame : kIFrame;
  } else {
    parsedPacket.frameType = kPFrame;
  }
  if (0 != ParseVP8FrameSize(parsedPacket, dataPtr, dataLength)) {
    return false;
  }
  parsedPacket.info.VP8.data       = dataPtr;
  parsedPacket.info.VP8.dataLength = dataLength;
  return true;
}

int RTPPayloadParser::ParseVP8FrameSize(RTPPayload& parsedPacket,
                                        const uint8_t* dataPtr,
                                        int dataLength) const {
  if (parsedPacket.frameType != kIFrame) {
    // Included in payload header for I-frames.
    return 0;
  }
  if (dataLength < 10) {
    // For an I-frame we should always have the uncompressed VP8 header
    // in the beginning of the partition.
    return -1;
  }
  RTPPayloadVP8* vp8 = &parsedPacket.info.VP8;
  vp8->frameWidth = ((dataPtr[7] << 8) + dataPtr[6]) & 0x3FFF;
  vp8->frameHeight = ((dataPtr[9] << 8) + dataPtr[8]) & 0x3FFF;
  return 0;
}

int RTPPayloadParser::ParseVP8Extension(RTPPayloadVP8* vp8,
                                        const uint8_t* dataPtr,
                                        int dataLength) const {
  int parsedBytes = 0;
  if (dataLength <= 0) return -1;
  // Optional X field is present
  vp8->hasPictureID = (*dataPtr & 0x80) ? true : false; // I bit
  vp8->hasTl0PicIdx = (*dataPtr & 0x40) ? true : false; // L bit
  vp8->hasTID = (*dataPtr & 0x20) ? true : false;       // T bit
  vp8->hasKeyIdx = (*dataPtr & 0x10) ? true : false;    // K bit

  // Advance dataPtr and decrease remaining payload size
  dataPtr++;
  parsedBytes++;
  dataLength--;

  if (vp8->hasPictureID) {
    if (ParseVP8PictureID(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
      return -1;
    }
  }

  if (vp8->hasTl0PicIdx) {
    if (ParseVP8Tl0PicIdx(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
      return -1;
    }
  }

  if (vp8->hasTID || vp8->hasKeyIdx) {
    if (ParseVP8TIDAndKeyIdx(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
      return -1;
    }
  }
  return parsedBytes;
}

int RTPPayloadParser::ParseVP8PictureID(RTPPayloadVP8* vp8,
                                        const uint8_t** dataPtr,
                                        int* dataLength,
                                        int* parsedBytes) const {
  if (*dataLength <= 0) return -1;
  vp8->pictureID = (**dataPtr & 0x7F);
  if (**dataPtr & 0x80) {
    (*dataPtr)++;
    (*parsedBytes)++;
    if (--(*dataLength) <= 0) return -1;
    // PictureID is 15 bits
    vp8->pictureID = (vp8->pictureID << 8) +** dataPtr;
  }
  (*dataPtr)++;
  (*parsedBytes)++;
  (*dataLength)--;
  return 0;
}

int RTPPayloadParser::ParseVP8Tl0PicIdx(RTPPayloadVP8* vp8,
                                        const uint8_t** dataPtr,
                                        int* dataLength,
                                        int* parsedBytes) const {
  if (*dataLength <= 0) return -1;
  vp8->tl0PicIdx = **dataPtr;
  (*dataPtr)++;
  (*parsedBytes)++;
  (*dataLength)--;
  return 0;
}

int RTPPayloadParser::ParseVP8TIDAndKeyIdx(RTPPayloadVP8* vp8,
                                           const uint8_t** dataPtr,
                                           int* dataLength,
                                           int* parsedBytes) const {
  if (*dataLength <= 0) return -1;
  if (vp8->hasTID) {
    vp8->tID = ((**dataPtr >> 6) & 0x03);
    vp8->layerSync = (**dataPtr & 0x20) ? true : false;  // Y bit
  }
  if (vp8->hasKeyIdx) {
    vp8->keyIdx = (**dataPtr & 0x1F);
  }
  (*dataPtr)++;
  (*parsedBytes)++;
  (*dataLength)--;
  return 0;
}

}  // namespace ModuleRTPUtility

}  // namespace webrtc
