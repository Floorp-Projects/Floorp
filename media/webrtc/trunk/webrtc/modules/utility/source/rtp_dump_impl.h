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

#include "webrtc/modules/utility/interface/rtp_dump.h"

namespace webrtc {
class CriticalSectionWrapper;
class FileWrapper;
class RtpDumpImpl : public RtpDump
{
public:
    RtpDumpImpl();
    virtual ~RtpDumpImpl();

    virtual int32_t Start(const char* fileNameUTF8) OVERRIDE;
    virtual int32_t Stop() OVERRIDE;
    virtual bool IsActive() const OVERRIDE;
    virtual int32_t DumpPacket(const uint8_t* packet,
                               uint16_t packetLength) OVERRIDE;
private:
    // Return the system time in ms.
    inline uint32_t GetTimeInMS() const;
    // Return x in network byte order (big endian).
    inline uint32_t RtpDumpHtonl(uint32_t x) const;
    // Return x in network byte order (big endian).
    inline uint16_t RtpDumpHtons(uint16_t x) const;

    // Return true if the packet starts with a valid RTCP header.
    // Note: See RtpUtility::RtpHeaderParser::RTCP() for details on how
    //       to determine if the packet is an RTCP packet.
    bool RTCP(const uint8_t* packet) const;

private:
    CriticalSectionWrapper* _critSect;
    FileWrapper& _file;
    uint32_t _startTime;
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_SOURCE_RTP_DUMP_IMPL_H_
