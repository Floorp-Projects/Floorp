/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_RTP_DUMP_IMPL_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_RTP_DUMP_IMPL_H_

#include "rtp_dump.h"

namespace webrtc {
class CriticalSectionWrapper;
class FileWrapper;
class RtpDumpImpl : public RtpDump
{
public:
    RtpDumpImpl();
    virtual ~RtpDumpImpl();

    virtual WebRtc_Word32 Start(const char* fileNameUTF8);
    virtual WebRtc_Word32 Stop();
    virtual bool IsActive() const;
    virtual WebRtc_Word32 DumpPacket(const WebRtc_UWord8* packet,
                                     WebRtc_UWord16 packetLength);
private:
    // Return the system time in ms.
    inline WebRtc_UWord32 GetTimeInMS() const;
    // Return x in network byte order (big endian).
    inline WebRtc_UWord32 RtpDumpHtonl(WebRtc_UWord32 x) const;
    // Return x in network byte order (big endian).
    inline WebRtc_UWord16 RtpDumpHtons(WebRtc_UWord16 x) const;

    // Return true if the packet starts with a valid RTCP header.
    // Note: See ModuleRTPUtility::RTPHeaderParser::RTCP() for details on how
    //       to determine if the packet is an RTCP packet.
    bool RTCP(const WebRtc_UWord8* packet) const;

private:
    CriticalSectionWrapper* _critSect;
    FileWrapper& _file;
    WebRtc_UWord32 _startTime;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_SOURCE_RTP_DUMP_IMPL_H_
