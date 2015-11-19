/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file implements a class that writes a stream of RTP and RTCP packets
// to a file according to the format specified by rtpplay. See
// http://www.cs.columbia.edu/irt/software/rtptools/.
// Notes: supported platforms are Windows, Linux and Mac OSX

#ifndef WEBRTC_MODULES_UTILITY_INTERFACE_RTP_DUMP_H_
#define WEBRTC_MODULES_UTILITY_INTERFACE_RTP_DUMP_H_

#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class RtpDump
{
public:
    // Factory method.
    static RtpDump* CreateRtpDump();

    // Delete function. Destructor disabled.
    static void DestroyRtpDump(RtpDump* object);

    // Open the file fileNameUTF8 for writing RTP/RTCP packets.
    // Note: this API also adds the rtpplay header.
    virtual int32_t Start(const char* fileNameUTF8) = 0;

    // Close the existing file. No more packets will be recorded.
    virtual int32_t Stop() = 0;

    // Return true if a file is open for recording RTP/RTCP packets.
    virtual bool IsActive() const = 0;

    // Writes the RTP/RTCP packet in packet with length packetLength in bytes.
    // Note: packet should contain the RTP/RTCP part of the packet. I.e. the
    // first bytes of packet should be the RTP/RTCP header.
    virtual int32_t DumpPacket(const uint8_t* packet,
                               size_t packetLength) = 0;

protected:
    virtual ~RtpDump();
};
}  // namespace webrtc
#endif // WEBRTC_MODULES_UTILITY_INTERFACE_RTP_DUMP_H_
