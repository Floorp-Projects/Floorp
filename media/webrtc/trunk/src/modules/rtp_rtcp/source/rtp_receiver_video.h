/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"

#include "typedefs.h"

#include "overuse_detector.h"
#include "remote_rate_control.h"
#include "Bitrate.h"

namespace webrtc {
class ReceiverFEC;
class ModuleRtpRtcpImpl;
class CriticalSectionWrapper;

class RTPReceiverVideo
{
public:
    RTPReceiverVideo();
    RTPReceiverVideo(const WebRtc_Word32 id, ModuleRtpRtcpImpl* owner);

    virtual ~RTPReceiverVideo();

    virtual void ChangeUniqueId(const WebRtc_Word32 id);

    WebRtc_Word32 Init();

    WebRtc_Word32 RegisterIncomingVideoCallback(
        RtpVideoFeedback* incomingMessagesCallback);

    void UpdateBandwidthManagement(const WebRtc_UWord32 bitrateBps,
                                   const WebRtc_UWord8 fractionLost,
                                   const WebRtc_UWord16 roundTripTimeMs);

    ModuleRTPUtility::Payload* RegisterReceiveVideoPayload(
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const WebRtc_Word8 payloadType,
        const WebRtc_UWord32 maxRate);

    WebRtc_Word32 ParseVideoCodecSpecific(
        WebRtcRTPHeader* rtpHeader,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord16 payloadDataLength,
        const RtpVideoCodecTypes videoType,
        const bool isRED,
        const WebRtc_UWord8* incomingRtpPacket,
        const WebRtc_UWord16 incomingRtpPacketSize,
        const WebRtc_Word64 nowMS);

    virtual WebRtc_Word32 ReceiveRecoveredPacketCallback(
        WebRtcRTPHeader* rtpHeader,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord16 payloadDataLength);

    void SetPacketOverHead(WebRtc_UWord16 packetOverHead);

protected:
    void ResetOverUseDetector();

    WebRtc_UWord16 EstimateBandwidth( const WebRtc_UWord16 bufferLength);

    virtual WebRtc_Word32 CallbackOfReceivedPayloadData(
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord16 payloadSize,
        const WebRtcRTPHeader* rtpHeader) = 0;

    virtual WebRtc_UWord32 TimeStamp() const = 0;
    virtual WebRtc_UWord16 SequenceNumber() const = 0;

    virtual WebRtc_UWord32 PayloadTypeToPayload(
        const WebRtc_UWord8 payloadType,
        ModuleRTPUtility::Payload*& payload) const = 0;

    virtual bool RetransmitOfOldPacket(
        const WebRtc_UWord16 sequenceNumber,
        const WebRtc_UWord32 rtpTimeStamp) const  = 0;

    virtual WebRtc_Word8 REDPayloadType() const = 0;

    WebRtc_Word32 SetCodecType(const RtpVideoCodecTypes videoType,
                               WebRtcRTPHeader* rtpHeader) const;

    WebRtc_Word32 ParseVideoCodecSpecificSwitch(
        WebRtcRTPHeader* rtpHeader,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord16 payloadDataLength,
        const RtpVideoCodecTypes videoType);

    WebRtc_Word32 ReceiveGenericCodec(WebRtcRTPHeader *rtpHeader,
                                      const WebRtc_UWord8* payloadData,
                                      const WebRtc_UWord16 payloadDataLength);

    WebRtc_Word32 ReceiveVp8Codec(WebRtcRTPHeader *rtpHeader,
                                  const WebRtc_UWord8* payloadData,
                                  const WebRtc_UWord16 payloadDataLength);

    WebRtc_Word32 BuildRTPheader(const WebRtcRTPHeader* rtpHeader,
                                 WebRtc_UWord8* dataBuffer) const;

private:
    WebRtc_Word32             _id;
    ModuleRtpRtcpImpl*        _rtpRtcp;

    CriticalSectionWrapper*   _criticalSectionFeedback;
    RtpVideoFeedback*         _cbVideoFeedback;

    CriticalSectionWrapper*   _criticalSectionReceiverVideo;

    // bandwidth
    bool                      _completeFrame;
    WebRtc_UWord32            _packetStartTimeMs;
    WebRtc_UWord16            _receivedBW[BW_HISTORY_SIZE];
    WebRtc_UWord16            _estimatedBW;

      // FEC
    bool                      _currentFecFrameDecoded;
    ReceiverFEC*              _receiveFEC;

    // BWE
    OverUseDetector           _overUseDetector;
    BitRateStats              _videoBitRate;
    WebRtc_Word64             _lastBitRateChange;
    WebRtc_UWord16            _packetOverHead;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
