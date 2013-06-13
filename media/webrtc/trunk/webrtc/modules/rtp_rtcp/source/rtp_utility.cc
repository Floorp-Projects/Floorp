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
#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_BSD) || (defined WEBRTC_MAC))
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

/*
 * Time routines.
 */

#if defined(_WIN32)

struct reference_point {
  FILETIME      file_time;
  LARGE_INTEGER counterMS;
};

struct WindowsHelpTimer {
  volatile LONG _timeInMs;
  volatile LONG _numWrapTimeInMs;
  reference_point _ref_point;

  volatile LONG _sync_flag;
};

void Synchronize(WindowsHelpTimer* help_timer) {
  const LONG start_value = 0;
  const LONG new_value = 1;
  const LONG synchronized_value = 2;

  LONG compare_flag = new_value;
  while (help_timer->_sync_flag == start_value) {
    const LONG new_value = 1;
    compare_flag = InterlockedCompareExchange(
        &help_timer->_sync_flag, new_value, start_value);
  }
  if (compare_flag != start_value) {
    // This thread was not the one that incremented the sync flag.
    // Block until synchronization finishes.
    while (compare_flag != synchronized_value) {
      ::Sleep(0);
    }
    return;
  }
  // Only the synchronizing thread gets here so this part can be
  // considered single threaded.

  // set timer accuracy to 1 ms
  timeBeginPeriod(1);
  FILETIME    ft0 = { 0, 0 },
              ft1 = { 0, 0 };
  //
  // Spin waiting for a change in system time. Get the matching
  // performance counter value for that time.
  //
  ::GetSystemTimeAsFileTime(&ft0);
  do {
    ::GetSystemTimeAsFileTime(&ft1);

    help_timer->_ref_point.counterMS.QuadPart = ::timeGetTime();
    ::Sleep(0);
  } while ((ft0.dwHighDateTime == ft1.dwHighDateTime) &&
          (ft0.dwLowDateTime == ft1.dwLowDateTime));
    help_timer->_ref_point.file_time = ft1;
}

void get_time(WindowsHelpTimer* help_timer, FILETIME& current_time) {
  // we can't use query performance counter due to speed stepping
  DWORD t = timeGetTime();
  // NOTE: we have a missmatch in sign between _timeInMs(LONG) and
  // (DWORD) however we only use it here without +- etc
  volatile LONG* timeInMsPtr = &help_timer->_timeInMs;
  // Make sure that we only inc wrapper once.
  DWORD old = InterlockedExchange(timeInMsPtr, t);
  if(old > t) {
    // wrap
    help_timer->_numWrapTimeInMs++;
  }
  LARGE_INTEGER elapsedMS;
  elapsedMS.HighPart = help_timer->_numWrapTimeInMs;
  elapsedMS.LowPart = t;

  elapsedMS.QuadPart = elapsedMS.QuadPart -
      help_timer->_ref_point.counterMS.QuadPart;

  // Translate to 100-nanoseconds intervals (FILETIME resolution)
  // and add to reference FILETIME to get current FILETIME.
  ULARGE_INTEGER filetime_ref_as_ul;

  filetime_ref_as_ul.HighPart =
      help_timer->_ref_point.file_time.dwHighDateTime;
  filetime_ref_as_ul.LowPart =
      help_timer->_ref_point.file_time.dwLowDateTime;
  filetime_ref_as_ul.QuadPart +=
      (ULONGLONG)((elapsedMS.QuadPart)*1000*10);

  // Copy to result
  current_time.dwHighDateTime = filetime_ref_as_ul.HighPart;
  current_time.dwLowDateTime = filetime_ref_as_ul.LowPart;
  }

  // A clock reading times from the Windows API.
  class WindowsSystemClock : public RtpRtcpClock {
  public:
    WindowsSystemClock(WindowsHelpTimer* helpTimer)
      : _helpTimer(helpTimer) {}

    virtual ~WindowsSystemClock() {}

    virtual WebRtc_Word64 GetTimeInMS();

    virtual void CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac);

  private:
    WindowsHelpTimer* _helpTimer;
};

#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)

// A clock reading times from the POSIX API.
class UnixSystemClock : public RtpRtcpClock {
public:
  UnixSystemClock() {}
  virtual ~UnixSystemClock() {}

  virtual WebRtc_Word64 GetTimeInMS();

  virtual void CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac);
};
#endif

#if defined(_WIN32)
WebRtc_Word64 WindowsSystemClock::GetTimeInMS() {
  return TickTime::MillisecondTimestamp();
}

// Use the system time (roughly synchronised to the tick, and
// extrapolated using the system performance counter.
void WindowsSystemClock::CurrentNTP(WebRtc_UWord32& secs,
                                    WebRtc_UWord32& frac) {
  const WebRtc_UWord64 FILETIME_1970 = 0x019db1ded53e8000;

  FILETIME StartTime;
  WebRtc_UWord64 Time;
  struct timeval tv;

  // We can't use query performance counter since they can change depending on
  // speed steping
  get_time(_helpTimer, StartTime);

  Time = (((WebRtc_UWord64) StartTime.dwHighDateTime) << 32) +
         (WebRtc_UWord64) StartTime.dwLowDateTime;

  // Convert the hecto-nano second time to tv format
  Time -= FILETIME_1970;

  tv.tv_sec = (WebRtc_UWord32)(Time / (WebRtc_UWord64)10000000);
  tv.tv_usec = (WebRtc_UWord32)((Time % (WebRtc_UWord64)10000000) / 10);

  double dtemp;

  secs = tv.tv_sec + NTP_JAN_1970;
  dtemp = tv.tv_usec / 1e6;

  if (dtemp >= 1) {
    dtemp -= 1;
    secs++;
  } else if (dtemp < -1) {
    dtemp += 1;
    secs--;
  }
  dtemp *= NTP_FRAC;
  frac = (WebRtc_UWord32)dtemp;
}

#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_BSD) || (defined WEBRTC_MAC))

WebRtc_Word64 UnixSystemClock::GetTimeInMS() {
  return TickTime::MillisecondTimestamp();
}

// Use the system time.
void UnixSystemClock::CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac) {
  double dtemp;
  struct timeval tv;
  struct timezone tz;
  tz.tz_minuteswest  = 0;
  tz.tz_dsttime = 0;
  gettimeofday(&tv, &tz);

  secs = tv.tv_sec + NTP_JAN_1970;
  dtemp = tv.tv_usec / 1e6;
  if (dtemp >= 1) {
    dtemp -= 1;
    secs++;
  } else if (dtemp < -1) {
    dtemp += 1;
    secs--;
  }
  dtemp *= NTP_FRAC;
  frac = (WebRtc_UWord32)dtemp;
}
#endif

#if defined(_WIN32)
// Keeps the global state for the Windows implementation of RtpRtcpClock.
// Note that this is a POD. Only PODs are allowed to have static storage
// duration according to the Google Style guide.
static WindowsHelpTimer global_help_timer = {0, 0, {{ 0, 0}, 0}, 0};
#endif

RtpRtcpClock* GetSystemClock() {
#if defined(_WIN32)
  return new WindowsSystemClock(&global_help_timer);
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
  return new UnixSystemClock();
#else
  return NULL;
#endif
}

WebRtc_UWord32 GetCurrentRTP(RtpRtcpClock* clock, WebRtc_UWord32 freq) {
  const bool use_global_clock = (clock == NULL);
  RtpRtcpClock* local_clock = clock;
  if (use_global_clock) {
    local_clock = GetSystemClock();
  }
  WebRtc_UWord32 secs = 0, frac = 0;
  local_clock->CurrentNTP(secs, frac);
  if (use_global_clock) {
    delete local_clock;
  }
  return ConvertNTPTimeToRTP(secs, frac, freq);
}

WebRtc_UWord32 ConvertNTPTimeToRTP(WebRtc_UWord32 NTPsec,
                                   WebRtc_UWord32 NTPfrac,
                                   WebRtc_UWord32 freq) {
  float ftemp = (float)NTPfrac / (float)NTP_FRAC;
  WebRtc_UWord32 tmp = (WebRtc_UWord32)(ftemp * freq);
  return NTPsec * freq + tmp;
}

WebRtc_UWord32 ConvertNTPTimeToMS(WebRtc_UWord32 NTPsec,
                                  WebRtc_UWord32 NTPfrac) {
  int freq = 1000;
  float ftemp = (float)NTPfrac / (float)NTP_FRAC;
  WebRtc_UWord32 tmp = (WebRtc_UWord32)(ftemp * freq);
  WebRtc_UWord32 MStime = NTPsec * freq + tmp;
  return MStime;
}

bool OldTimestamp(uint32_t newTimestamp,
                  uint32_t existingTimestamp,
                  bool* wrapped) {
  bool tmpWrapped =
    (newTimestamp < 0x0000ffff && existingTimestamp > 0xffff0000) ||
    (newTimestamp > 0xffff0000 && existingTimestamp < 0x0000ffff);
  *wrapped = tmpWrapped;
  if (existingTimestamp > newTimestamp && !tmpWrapped) {
    return true;
  } else if (existingTimestamp <= newTimestamp && !tmpWrapped) {
    return false;
  } else if (existingTimestamp < newTimestamp && tmpWrapped) {
    return true;
  } else {
    return false;
  }
}

/*
 * Misc utility routines
 */

const WebRtc_UWord8* GetPayloadData(const WebRtcRTPHeader* rtp_header,
                                    const WebRtc_UWord8* packet) {
  return packet + rtp_header->header.headerLength;
}

WebRtc_UWord16 GetPayloadDataLength(const WebRtcRTPHeader* rtp_header,
                                    const WebRtc_UWord16 packet_length) {
  WebRtc_UWord16 length = packet_length - rtp_header->header.paddingLength -
      rtp_header->header.headerLength;
  return static_cast<WebRtc_UWord16>(length);
}

#if defined(_WIN32)
bool StringCompare(const char* str1, const char* str2,
                   const WebRtc_UWord32 length) {
  return (_strnicmp(str1, str2, length) == 0) ? true : false;
}
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
bool StringCompare(const char* str1, const char* str2,
                   const WebRtc_UWord32 length) {
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
void AssignUWord32ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<WebRtc_UWord8>(value >> 24);
  dataBuffer[1] = static_cast<WebRtc_UWord8>(value >> 16);
  dataBuffer[2] = static_cast<WebRtc_UWord8>(value >> 8);
  dataBuffer[3] = static_cast<WebRtc_UWord8>(value);
#else
  WebRtc_UWord32* ptr = reinterpret_cast<WebRtc_UWord32*>(dataBuffer);
  ptr[0] = value;
#endif
}

void AssignUWord24ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<WebRtc_UWord8>(value >> 16);
  dataBuffer[1] = static_cast<WebRtc_UWord8>(value >> 8);
  dataBuffer[2] = static_cast<WebRtc_UWord8>(value);
#else
  dataBuffer[0] = static_cast<WebRtc_UWord8>(value);
  dataBuffer[1] = static_cast<WebRtc_UWord8>(value >> 8);
  dataBuffer[2] = static_cast<WebRtc_UWord8>(value >> 16);
#endif
}

void AssignUWord16ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord16 value) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  dataBuffer[0] = static_cast<WebRtc_UWord8>(value >> 8);
  dataBuffer[1] = static_cast<WebRtc_UWord8>(value);
#else
  WebRtc_UWord16* ptr = reinterpret_cast<WebRtc_UWord16*>(dataBuffer);
  ptr[0] = value;
#endif
}

WebRtc_UWord16 BufferToUWord16(const WebRtc_UWord8* dataBuffer) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  return (dataBuffer[0] << 8) + dataBuffer[1];
#else
  return *reinterpret_cast<const WebRtc_UWord16*>(dataBuffer);
#endif
}

WebRtc_UWord32 BufferToUWord24(const WebRtc_UWord8* dataBuffer) {
  return (dataBuffer[0] << 16) + (dataBuffer[1] << 8) + dataBuffer[2];
}

WebRtc_UWord32 BufferToUWord32(const WebRtc_UWord8* dataBuffer) {
#if defined(WEBRTC_LITTLE_ENDIAN)
  return (dataBuffer[0] << 24) + (dataBuffer[1] << 16) + (dataBuffer[2] << 8) +
      dataBuffer[3];
#else
  return *reinterpret_cast<const WebRtc_UWord32*>(dataBuffer);
#endif
}

WebRtc_UWord32 pow2(WebRtc_UWord8 exp) {
  return 1 << exp;
}

void RTPPayload::SetType(RtpVideoCodecTypes videoType) {
  type = videoType;

  switch (type) {
    case kRtpNoVideo:
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

RTPHeaderParser::RTPHeaderParser(const WebRtc_UWord8* rtpData,
                                 const WebRtc_UWord32 rtpDataLength)
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

  const WebRtc_UWord8  payloadType = _ptrRTPDataBegin[1];

  bool RTCP = false;

  // check if this is a RTCP packet
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
  const WebRtc_UWord8 V  = _ptrRTPDataBegin[0] >> 6;
  // Padding
  const bool          P  = ((_ptrRTPDataBegin[0] & 0x20) == 0) ? false : true;
  // eXtension
  const bool          X  = ((_ptrRTPDataBegin[0] & 0x10) == 0) ? false : true;
  const WebRtc_UWord8 CC = _ptrRTPDataBegin[0] & 0x0f;
  const bool          M  = ((_ptrRTPDataBegin[1] & 0x80) == 0) ? false : true;

  const WebRtc_UWord8 PT = _ptrRTPDataBegin[1] & 0x7f;

  const WebRtc_UWord16 sequenceNumber = (_ptrRTPDataBegin[2] << 8) +
      _ptrRTPDataBegin[3];

  const WebRtc_UWord8* ptr = &_ptrRTPDataBegin[4];

  WebRtc_UWord32 RTPTimestamp = *ptr++ << 24;
  RTPTimestamp += *ptr++ << 16;
  RTPTimestamp += *ptr++ << 8;
  RTPTimestamp += *ptr++;

  WebRtc_UWord32 SSRC = *ptr++ << 24;
  SSRC += *ptr++ << 16;
  SSRC += *ptr++ << 8;
  SSRC += *ptr++;

  if (V != 2) {
    return false;
  }

  const WebRtc_UWord8 CSRCocts = CC * 4;

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
    WebRtc_UWord32 CSRC = *ptr++ << 24;
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

    WebRtc_UWord16 definedByProfile = *ptr++ << 8;
    definedByProfile += *ptr++;

    WebRtc_UWord16 XLen = *ptr++ << 8;
    XLen += *ptr++; // in 32 bit words
    XLen *= 4; // in octs

    if (remain < (4 + XLen)) {
      return false;
    }
    if (definedByProfile == RTP_ONE_BYTE_HEADER_EXTENSION) {
      const WebRtc_UWord8* ptrRTPDataExtensionEnd = ptr + XLen;
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
    const WebRtc_UWord8* ptrRTPDataExtensionEnd,
    const WebRtc_UWord8* ptr) const {
  if (!ptrExtensionMap) {
    return;
  }

  while (ptrRTPDataExtensionEnd - ptr > 0) {
    //  0
    //  0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // |  ID   |  len  |
    // +-+-+-+-+-+-+-+-+

    const WebRtc_UWord8 id = (*ptr & 0xf0) >> 4;
    const WebRtc_UWord8 len = (*ptr & 0x0f);
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

        WebRtc_Word32 transmissionTimeOffset = *ptr++ << 16;
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
        // const WebRtc_UWord8 V = (*ptr & 0x80) >> 7;
        // const WebRtc_UWord8 level = (*ptr & 0x7f);
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
    WebRtc_UWord8 num_bytes = ParsePaddingBytes(ptrRTPDataExtensionEnd, ptr);
    ptr += num_bytes;
  }
}

WebRtc_UWord8 RTPHeaderParser::ParsePaddingBytes(
  const WebRtc_UWord8* ptrRTPDataExtensionEnd,
  const WebRtc_UWord8* ptr) const {

  WebRtc_UWord8 num_zero_bytes = 0;
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
                                   const WebRtc_UWord8* payloadData,
                                   WebRtc_UWord16 payloadDataLength,
                                   WebRtc_Word32 id)
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
    case kRtpNoVideo:
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
  const WebRtc_UWord8* dataPtr = _dataPtr;
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
                                        const WebRtc_UWord8* dataPtr,
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
                                        const WebRtc_UWord8* dataPtr,
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
                                        const WebRtc_UWord8** dataPtr,
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
                                        const WebRtc_UWord8** dataPtr,
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
                                           const WebRtc_UWord8** dataPtr,
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
