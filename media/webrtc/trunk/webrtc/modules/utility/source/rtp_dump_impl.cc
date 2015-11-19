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
#include <limits>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

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
} RtpDumpPacketHeader;

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
        LOG(LS_ERROR) << "Failed to open file.";
        return -1;
    }

    // Store start of RTP dump (to be used for offset calculation later).
    _startTime = GetTimeInMS();

    // All rtp dump files start with #!rtpplay.
    char magic[16];
    sprintf(magic, "#!rtpplay%s \n", RTPFILE_VERSION);
    if (_file.WriteText(magic) == -1)
    {
        LOG(LS_ERROR) << "Error writing to file.";
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
        LOG(LS_ERROR) << "Error writing to file.";
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

int32_t RtpDumpImpl::DumpPacket(const uint8_t* packet, size_t packetLength)
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

    RtpDumpPacketHeader hdr;
    size_t total_size = packetLength + sizeof hdr;
    if (packetLength < 1 || total_size > std::numeric_limits<uint16_t>::max())
    {
        return -1;
    }

    // If the packet doesn't contain a valid RTCP header the packet will be
    // considered RTP (without further verification).
    bool isRTCP = RTCP(packet);

    // Offset is relative to when recording was started.
    uint32_t offset = GetTimeInMS();
    if (offset < _startTime)
    {
        // Compensate for wraparound.
        offset += MAX_UWORD32 - _startTime + 1;
    } else {
        offset -= _startTime;
    }
    hdr.offset = RtpDumpHtonl(offset);

    hdr.length = RtpDumpHtons((uint16_t)(total_size));
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
        LOG(LS_ERROR) << "Error writing to file.";
        return -1;
    }
    if (!_file.Write(packet, packetLength))
    {
        LOG(LS_ERROR) << "Error writing to file.";
        return -1;
    }

    return 0;
}

bool RtpDumpImpl::RTCP(const uint8_t* packet) const
{
    return packet[1] == 192 || packet[1] == 200 || packet[1] == 201 ||
        packet[1] == 202 || packet[1] == 203 || packet[1] == 204 ||
        packet[1] == 205 || packet[1] == 206 || packet[1] == 207;
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
#endif
}

inline uint32_t RtpDumpImpl::RtpDumpHtonl(uint32_t x) const
{
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
    return x;
#elif defined(WEBRTC_ARCH_LITTLE_ENDIAN)
    return (x >> 24) + ((((x >> 16) & 0xFF) << 8) + ((((x >> 8) & 0xFF) << 16) +
                                                     ((x & 0xFF) << 24)));
#endif
}

inline uint16_t RtpDumpImpl::RtpDumpHtons(uint16_t x) const
{
#if defined(WEBRTC_ARCH_BIG_ENDIAN)
    return x;
#elif defined(WEBRTC_ARCH_LITTLE_ENDIAN)
    return (x >> 8) + ((x & 0xFF) << 8);
#endif
}
}  // namespace webrtc
