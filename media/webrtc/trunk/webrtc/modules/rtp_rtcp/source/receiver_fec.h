/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_

#include "rtp_rtcp_defines.h"
// This header is included to get the nested declaration of Packet structure.
#include "forward_error_correction.h"

#include "typedefs.h"

namespace webrtc {
class RTPReceiverVideo;

class ReceiverFEC
{
public:
    ReceiverFEC(const int32_t id, RTPReceiverVideo* owner);
    virtual ~ReceiverFEC();

    int32_t AddReceivedFECPacket(const WebRtcRTPHeader* rtpHeader,
                                 const uint8_t* incomingRtpPacket,
                                 const uint16_t payloadDataLength,
                                 bool& FECpacket);

    int32_t ProcessReceivedFEC();

    void SetPayloadTypeFEC(const int8_t payloadType);

private:
    int ParseAndReceivePacket(const ForwardErrorCorrection::Packet* packet);

    int _id;
    RTPReceiverVideo* _owner;
    ForwardErrorCorrection* _fec;
    // TODO(holmer): In the current version _receivedPacketList is never more
    // than one packet, since we process FEC every time a new packet
    // arrives. We should remove the list.
    ForwardErrorCorrection::ReceivedPacketList _receivedPacketList;
    ForwardErrorCorrection::RecoveredPacketList _recoveredPacketList;
    int8_t _payloadTypeFEC;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
