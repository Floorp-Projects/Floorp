/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

#include <assert.h>
#include <math.h>  // ceil
#include <string.h>  // memcpy

#if defined(_WIN32)
// Order for these headers are important
#include <Windows.h>  // FILETIME

#include <WinSock.h>  // timeval

#include <MMSystem.h>  // timeGetTime
#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_BSD) || (defined WEBRTC_MAC))
#include <sys/time.h>  // gettimeofday
#include <time.h>
#endif
#if (defined(_DEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
#include <stdio.h>
#endif

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/logging.h"

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

RtpData* NullObjectRtpData() {
  static NullRtpData null_rtp_data;
  return &null_rtp_data;
}

RtpFeedback* NullObjectRtpFeedback() {
  static NullRtpFeedback null_rtp_feedback;
  return &null_rtp_feedback;
}

RtpAudioFeedback* NullObjectRtpAudioFeedback() {
  static NullRtpAudioFeedback null_rtp_audio_feedback;
  return &null_rtp_audio_feedback;
}

ReceiveStatistics* NullObjectReceiveStatistics() {
  static NullReceiveStatistics null_receive_statistics;
  return &null_receive_statistics;
}

namespace RtpUtility {

enum {
  kRtcpExpectedVersion = 2,
  kRtcpMinHeaderLength = 4,
  kRtcpMinParseLength = 8,

  kRtpExpectedVersion = 2,
  kRtpMinParseLength = 12
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

#if defined(_WIN32)
bool StringCompare(const char* str1, const char* str2,
                   const uint32_t length) {
  return (_strnicmp(str1, str2, length) == 0) ? true : false;
}
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
bool StringCompare(const char* str1, const char* str2,
                   const uint32_t length) {
  return (strncasecmp(str1, str2, length) == 0) ? true : false;
}
#endif

size_t Word32Align(size_t size) {
  uint32_t remainder = size % 4;
  if (remainder != 0)
    return size + 4 - remainder;
  return size;
}

uint32_t pow2(uint8_t exp) {
  return 1 << exp;
}

RtpHeaderParser::RtpHeaderParser(const uint8_t* rtpData,
                                 const size_t rtpDataLength)
    : _ptrRTPDataBegin(rtpData),
      _ptrRTPDataEnd(rtpData ? (rtpData + rtpDataLength) : NULL) {
}

RtpHeaderParser::~RtpHeaderParser() {
}

bool RtpHeaderParser::RTCP() const {
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

  const uint8_t V = _ptrRTPDataBegin[0] >> 6;
  if (V != kRtcpExpectedVersion) {
    return false;
  }

  const uint8_t payloadType = _ptrRTPDataBegin[1];
  switch (payloadType) {
    case 192:
      return true;
    case 193:
      // not supported
      // pass through and check for a potential RTP packet
      return false;
    case 195:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
      return true;
    default:
      return false;
  }
}

bool RtpHeaderParser::ParseRtcp(RTPHeader* header) const {
  assert(header != NULL);

  const ptrdiff_t length = _ptrRTPDataEnd - _ptrRTPDataBegin;
  if (length < kRtcpMinParseLength) {
    return false;
  }

  const uint8_t V = _ptrRTPDataBegin[0] >> 6;
  if (V != kRtcpExpectedVersion) {
    return false;
  }

  const uint8_t PT = _ptrRTPDataBegin[1];
  const size_t len = (_ptrRTPDataBegin[2] << 8) + _ptrRTPDataBegin[3];
  const uint8_t* ptr = &_ptrRTPDataBegin[4];

  uint32_t SSRC = ByteReader<uint32_t>::ReadBigEndian(ptr);
  ptr += 4;

  header->payloadType  = PT;
  header->ssrc         = SSRC;
  header->headerLength = 4 + (len << 2);
  if (header->headerLength > static_cast<size_t>(length)) {
    return false;
  }

  return true;
}

bool RtpHeaderParser::Parse(RTPHeader& header,
                            RtpHeaderExtensionMap* ptrExtensionMap) const {
  const ptrdiff_t length = _ptrRTPDataEnd - _ptrRTPDataBegin;
  if (length < kRtpMinParseLength) {
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

  uint32_t RTPTimestamp = ByteReader<uint32_t>::ReadBigEndian(ptr);
  ptr += 4;

  uint32_t SSRC = ByteReader<uint32_t>::ReadBigEndian(ptr);
  ptr += 4;

  if (V != kRtpExpectedVersion) {
    return false;
  }

  header.markerBit      = M;
  header.payloadType    = PT;
  header.sequenceNumber = sequenceNumber;
  header.timestamp      = RTPTimestamp;
  header.ssrc           = SSRC;
  header.numCSRCs       = CC;
  header.paddingLength  = P ? *(_ptrRTPDataEnd - 1) : 0;

  // 12 == sizeof(RFC rtp header) == kRtpMinParseLength, each CSRC=4 bytes
  header.headerLength   = 12 + (CC * 4);
  // not a full validation, just safety against underflow.  Padding must
  // start after the header.  We can have 0 payload bytes left, note.
  if (header.paddingLength + header.headerLength > (size_t) length) {
    return false;
  }

  for (uint8_t i = 0; i < CC; ++i) {
    uint32_t CSRC = ByteReader<uint32_t>::ReadBigEndian(ptr);
    ptr += 4;
    header.arrOfCSRCs[i] = CSRC;
  }
  assert((ptr - _ptrRTPDataBegin) == (ptrdiff_t) header.headerLength);

  // If in effect, MAY be omitted for those packets for which the offset
  // is zero.
  header.extension.hasTransmissionTimeOffset = false;
  header.extension.transmissionTimeOffset = 0;

  // May not be present in packet.
  header.extension.hasAbsoluteSendTime = false;
  header.extension.absoluteSendTime = 0;

  // May not be present in packet.
  header.extension.hasAudioLevel = false;
  header.extension.audioLevel = 0;

  // May not be present in packet.
  header.extension.hasVideoRotation = false;
  header.extension.videoRotation = 0;

  // May not be present in packet.
  header.extension.hasRID = false;
  header.extension.rid = NULL;

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
    // earlier test ensures we have at least paddingLength bytes left
    const ptrdiff_t remain = (_ptrRTPDataEnd - ptr) - header.paddingLength;
    if (remain < 4) { // minimum header extension length = 32 bits
      return false;
    }

    header.headerLength += 4;

    uint16_t definedByProfile = ByteReader<uint16_t>::ReadBigEndian(ptr);
    ptr += 2;

    // in 32 bit words
    size_t XLen = ByteReader<uint16_t>::ReadBigEndian(ptr);
    ptr += 2;
    XLen *= 4;  // in bytes

    if (static_cast<size_t>(remain) < (4 + XLen)) {
      return false;
    }
    if (definedByProfile == kRtpOneByteHeaderExtensionId) {
      const uint8_t* ptrRTPDataExtensionEnd = ptr + XLen;
      ParseOneByteExtensionHeader(header,
                                  ptrExtensionMap,
                                  ptrRTPDataExtensionEnd,
                                  ptr);
    }
    header.headerLength += XLen;
  }
  return true;
}

void RtpHeaderParser::ParseOneByteExtensionHeader(
    RTPHeader& header,
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

    // Note that 'len' is the header extension element length, which is the
    // number of bytes - 1.
    const uint8_t id = (*ptr & 0xf0) >> 4;
    const uint8_t len = (*ptr & 0x0f);
    ptr++;

    if (id == 15) {
      LOG(LS_WARNING)
          << "RTP extension header 15 encountered. Terminate parsing.";
      return;
    }

    RTPExtensionType type;
    if (ptrExtensionMap->GetType(id, &type) != 0) {
      // If we encounter an unknown extension, just skip over it.
      LOG(LS_WARNING) << "Failed to find extension id: "
                      << static_cast<int>(id);
    } else {
      switch (type) {
        case kRtpExtensionTransmissionTimeOffset: {
          if (len != 2) {
            LOG(LS_WARNING) << "Incorrect transmission time offset len: "
                            << len;
            return;
          }
          //  0                   1                   2                   3
          //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          // |  ID   | len=2 |              transmission offset              |
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

          header.extension.transmissionTimeOffset =
              ByteReader<int32_t, 3>::ReadBigEndian(ptr);
          header.extension.hasTransmissionTimeOffset = true;
          break;
        }
        case kRtpExtensionAudioLevel: {
          if (len != 0) {
            LOG(LS_WARNING) << "Incorrect audio level len: " << len;
            return;
          }
          //  0                   1
          //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          // |  ID   | len=0 |V|   level     |
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          //

          // Parse out the fields but only use it for debugging for now.
          // const uint8_t V = (*ptr & 0x80) >> 7;
          // const uint8_t level = (*ptr & 0x7f);
          // DEBUG_PRINT("RTP_AUDIO_LEVEL_UNIQUE_ID: ID=%u, len=%u, V=%u,
          // level=%u", ID, len, V, level);

          header.extension.audioLevel = ptr[0];
          header.extension.hasAudioLevel = true;
          break;
        }
        case kRtpExtensionAbsoluteSendTime: {
          if (len != 2) {
            LOG(LS_WARNING) << "Incorrect absolute send time len: " << len;
            return;
          }
          //  0                   1                   2                   3
          //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          // |  ID   | len=2 |              absolute send time               |
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

          header.extension.absoluteSendTime =
              ByteReader<uint32_t, 3>::ReadBigEndian(ptr);
          header.extension.hasAbsoluteSendTime = true;
          break;
        }
        case kRtpExtensionVideoRotation: {
          if (len != 0) {
            LOG(LS_WARNING)
                << "Incorrect coordination of video coordination len: " << len;
            return;
          }
          //  0                   1
          //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          // |  ID   | len=0 |0 0 0 0 C F R R|
          // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          header.extension.hasVideoRotation = true;
          header.extension.videoRotation = ptr[0];
          break;
        }
        case kRtpExtensionTransportSequenceNumber: {
          if (len != 1) {
            LOG(LS_WARNING)
                << "Incorrect peer connection sequence number len: " << len;
            return;
          }
          //   0                   1                   2
          //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
          //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          //  |  ID   | L=1   |transport wide sequence number |
          //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

          uint16_t sequence_number = ptr[0] << 8;
          sequence_number += ptr[1];
          header.extension.transportSequenceNumber = sequence_number;
          header.extension.hasTransportSequenceNumber = true;
          break;
        }
        case kRtpExtensionRtpStreamId: {
          //   0                   1                   2
          //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
          //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          //  |  ID   | L=?   |UTF-8 RID value......          |...
          //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

          // TODO(jesup) - avoid allocating on each packet - high watermark the RID buffer?
          char* ptrRID = new char[len+1];
          memcpy(ptrRID, ptr, len);
          ptrRID[len] = '\0';
          header.extension.rid = ptrRID;
          header.extension.hasRID = true;
          break;
        }
        default: {
          LOG(LS_WARNING) << "Extension type not implemented: " << type;
          return;
        }
      }
    }
    ptr += (len + 1);
    uint8_t num_bytes = ParsePaddingBytes(ptrRTPDataExtensionEnd, ptr);
    ptr += num_bytes;
  }
}

uint8_t RtpHeaderParser::ParsePaddingBytes(
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
}  // namespace RtpUtility

}  // namespace webrtc
