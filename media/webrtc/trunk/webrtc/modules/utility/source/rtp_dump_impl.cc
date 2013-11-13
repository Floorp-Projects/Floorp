/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/source/rtp_dump_impl.h"

#include <assert.h>
#include <stdio.h>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#if defined(_WIN32)
#include <Windows.h>
#include <mmsystem.h>
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC) || defined(WEBRTC_BSD)
#include <string.h>
#include <sys/time.h>
#include <time.h>
#endif

#if (defined(_DEBUG) && defined(_WIN32))
#define DEBUG_PRINT(expr)   OutputDebugString(##expr)
#define DEBUG_PRINTP(expr, p)   \
{                               \
    char msg[128];              \
    sprintf(msg, ##expr, p);    \
    OutputDebugString(msg);     \
}
#else
#define DEBUG_PRINT(expr)    ((void)0)
#define DEBUG_PRINTP(expr,p) ((void)0)
#endif  // defined(_DEBUG) && defined(_WIN32)

namespace webrtc {
const char RTPFILE_VERSION[] = "1.0";
const uint32_t MAX_UWORD32 = 0xffffffff;

// This stucture is specified in the rtpdump documentation.
// This struct corresponds to RD_packet_t in
// http://www.cs.columbia.edu/irt/software/rtptools/
typedef struct
{
    // Length of packet, including this header (may be smaller than plen if not
    // whole packet recorded).
    uint16_t length;
    // Actual header+payload length for RTP, 0 for RTCP.
    uint16_t plen;
    // Milliseconds since the start of recording.
    uint32_t offset;
} rtpDumpPktHdr_t;

RtpDump* RtpDump::CreateRtpDump()
{
    return new RtpDumpImpl();
}

void RtpDump::DestroyRtpDump(RtpDump* object)
{
    delete object;
}

RtpDumpImpl::RtpDumpImpl()
    : _critSect(CriticalSectionWrapper::CreateCriticalSection()),
      _file(*FileWrapper::Create()),
      _startTime(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1, "%s created", __FUNCTION__);
}

RtpDump::~RtpDump()
{
}

RtpDumpImpl::~RtpDumpImpl()
{
    _file.Flush();
    _file.CloseFile();
    delete &_file;
    delete _critSect;
    WEBRTC_TRACE(kTraceMemory, kTraceUtility, -1, "%s deleted", __FUNCTION__);
}

int32_t RtpDumpImpl::Start(const char* fileNameUTF8)
{

    if (fileNameUTF8 == NULL)
    {
        return -1;
    }

    CriticalSectionScoped lock(_critSect);
    _file.Flush();
    _file.CloseFile();
    if (_file.OpenFile(fileNameUTF8, false, false, false) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "failed to open the specified file");
        return -1;
    }

    // Store start of RTP dump (to be used for offset calculation later).
    _startTime = GetTimeInMS();

    // All rtp dump files start with #!rtpplay.
    char magic[16];
    sprintf(magic, "#!rtpplay%s \n", RTPFILE_VERSION);
    if (_file.WriteText(magic) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "error writing to file");
        return -1;
    }

    // The header according to the rtpdump documentation is sizeof(RD_hdr_t)
    // which is 8 + 4 + 2 = 14 bytes for 32-bit architecture (and 22 bytes on
    // 64-bit architecture). However, Wireshark use 16 bytes for the header
    // regardless of if the binary is 32-bit or 64-bit. Go by the same approach
    // as Wireshark since it makes more sense.
    // http://wiki.wireshark.org/rtpdump explains that an additional 2 bytes
    // of padding should be added to the header.
    char dummyHdr[16];
    memset(dummyHdr, 0, 16);
    if (!_file.Write(dummyHdr, sizeof(dummyHdr)))
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "error writing to file");
        return -1;
    }
    return 0;
}

int32_t RtpDumpImpl::Stop()
{
    CriticalSectionScoped lock(_critSect);
    _file.Flush();
    _file.CloseFile();
    return 0;
}

bool RtpDumpImpl::IsActive() const
{
    CriticalSectionScoped lock(_critSect);
    return _file.Open();
}

int32_t RtpDumpImpl::DumpPacket(const uint8_t* packet, uint16_t packetLength)
{
    CriticalSectionScoped lock(_critSect);
    if (!IsActive())
    {
        return 0;
    }

    if (packet == NULL)
    {
        return -1;
    }

    if (packetLength < 1)
    {
        return -1;
    }

    // If the packet doesn't contain a valid RTCP header the packet will be
    // considered RTP (without further verification).
    bool isRTCP = RTCP(packet);

    rtpDumpPktHdr_t hdr;
    uint32_t offset;

    // Offset is relative to when recording was started.
    offset = GetTimeInMS();
    if (offset < _startTime)
    {
        // Compensate for wraparound.
        offset += MAX_UWORD32 - _startTime + 1;
    } else {
        offset -= _startTime;
    }
    hdr.offset = RtpDumpHtonl(offset);

    hdr.length = RtpDumpHtons((uint16_t)(packetLength + sizeof(hdr)));
    if (isRTCP)
    {
        hdr.plen = 0;
    }
    else
    {
        hdr.plen = RtpDumpHtons((uint16_t)packetLength);
    }

    if (!_file.Write(&hdr, sizeof(hdr)))
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "error writing to file");
        return -1;
    }
    if (!_file.Write(packet, packetLength))
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "error writing to file");
        return -1;
    }

    return 0;
}

bool RtpDumpImpl::RTCP(const uint8_t* packet) const
{
    const uint8_t payloadType = packet[1];
    bool is_rtcp = false;

    switch(payloadType)
    {
    case 192:
        is_rtcp = true;
        break;
    case 193: case 195:
        break;
    case 200: case 201: case 202: case 203:
    case 204: case 205: case 206: case 207:
        is_rtcp = true;
        break;
    }
    return is_rtcp;
}

// TODO (hellner): why is TickUtil not used here?
inline uint32_t RtpDumpImpl::GetTimeInMS() const
{
#if defined(_WIN32)
    return timeGetTime();
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD) || defined(WEBRTC_MAC)
    struct timeval tv;
    struct timezone tz;
    unsigned long val;

    gettimeofday(&tv, &tz);
    val = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return val;
#else
    #error Either _WIN32 or LINUX or WEBRTC_MAC has to be defined!
    assert(false);
    return 0;
#endif
}

inline uint32_t RtpDumpImpl::RtpDumpHtonl(uint32_t x) const
{
#if defined(WEBRTC_BIG_ENDIAN)
    return x;
#elif defined(WEBRTC_LITTLE_ENDIAN)
    return (x >> 24) + ((((x >> 16) & 0xFF) << 8) + ((((x >> 8) & 0xFF) << 16) +
                                                     ((x & 0xFF) << 24)));
#else
#error Either WEBRTC_BIG_ENDIAN or WEBRTC_LITTLE_ENDIAN has to be defined!
    assert(false);
    return 0;
#endif
}

inline uint16_t RtpDumpImpl::RtpDumpHtons(uint16_t x) const
{
#if defined(WEBRTC_BIG_ENDIAN)
    return x;
#elif defined(WEBRTC_LITTLE_ENDIAN)
    return (x >> 8) + ((x & 0xFF) << 8);
#else
    #error Either WEBRTC_BIG_ENDIAN or WEBRTC_LITTLE_ENDIAN has to be defined!
    assert(false);
    return 0;
#endif
}
}  // namespace webrtc
