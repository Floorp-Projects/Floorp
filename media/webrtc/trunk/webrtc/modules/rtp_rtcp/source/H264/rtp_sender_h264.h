/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_RTP_SENDER_H264_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_RTP_SENDER_H264_H_

#include "typedefs.h"
#include "ModuleRTPRTCPConfig.h"
#include "rtp_rtcp_defines.h"
#include "h264_information.h"

#include "RTPSender.h"

namespace webrtc {
class RTPSenderH264
{
public:
    WebRtc_Word32 SendH264(const FrameType frameType,
                  const WebRtc_Word8 payloadType,
                          const WebRtc_UWord32 captureTimeStamp,
                          const WebRtc_UWord8* payloadData,
                          const WebRtc_UWord32 payloadSize,
                          H264Information& h264Information);

    WebRtc_Word32 SendH264SVC(const FrameType frameType,
                              const WebRtc_Word8 payloadType,
                              const WebRtc_UWord32 captureTimeStamp,
                              const WebRtc_UWord8* payloadData,
                              const WebRtc_UWord32 payloadSize,
                              H264Information& h264Information);

    // H.264 AVC
    WebRtc_Word32 SetH264PacketizationMode(const H264PacketizationMode mode);

    WebRtc_Word32 SetH264SendModeNALU_PPS_SPS(const bool dontSend);

    // H.264 SVC
    WebRtc_Word32 SetHighestSendLayer(const WebRtc_UWord8 dependencyQualityLayer,
                                    const WebRtc_UWord8 temporalLayer);

    WebRtc_Word32 HighestSendLayer(WebRtc_UWord8& dependencyQualityLayer,
                                 WebRtc_UWord8& temporalLayer);

protected:
    RTPSenderH264(RTPSenderInterface* rtpSender);
    virtual ~RTPSenderH264();

    WebRtc_Word32 Init();

    virtual WebRtc_UWord16 FECPacketOverhead() const = 0;
    virtual RtpVideoCodecTypes VideoCodecType() const = 0;

    virtual WebRtc_Word32 SendVideoPacket(const FrameType frameType,
                                        const WebRtc_UWord8* dataBuffer,
                                        const WebRtc_UWord16 payloadLength,
                                        const WebRtc_UWord16 rtpHeaderLength,
                                        bool baseLayerVideoPacket=false) = 0;


    bool SendH264SVCLayer(const FrameType frameType,
                          const WebRtc_UWord8 temporalID,
                          const WebRtc_UWord8 dependencyQualityID,
                          bool& higestLayer);

    // H.264 SVC
    WebRtc_Word32 AddH264PACSINALU(const bool firstPacketInNALU,
                                 const bool lastPacketInNALU,
                                 const H264_PACSI_NALU& paci,
                                 const H264_SVC_NALUHeader& svc,
                                 const WebRtc_UWord16 DONC,
                                 WebRtc_UWord8* databuffer,
                                 WebRtc_Word32& curByte) const;

    WebRtc_Word32 SendH264FillerData(const WebRtcRTPHeader* rtpHeader,
                                   const WebRtc_UWord16 bytesToSend,
                                   const WebRtc_UWord32 ssrc);

    WebRtc_Word32 SendH264FillerData(const WebRtc_UWord32 captureTimestamp,
                                   const WebRtc_UWord8 payloadType,
                                   const WebRtc_UWord32 bytesToSend);

    WebRtc_Word32 SendH264SVCRelayPacket(const WebRtcRTPHeader* rtpHeader,
                                       const WebRtc_UWord8* incomingRTPPacket,
                                       const WebRtc_UWord16 incomingRTPPacketSize,
                                       const WebRtc_UWord32 ssrc,
                                       const bool higestLayer);

    WebRtc_Word32 SetH264RelaySequenceNumber(const WebRtc_UWord16 seqNum);

    WebRtc_Word32 SetH264RelayCompleteLayer(const bool complete);

    // H.264
    H264PacketizationMode _h264Mode;
    bool                      _h264SendPPS_SPS;

    // H.264-SVC
    WebRtc_Word8                _h264SVCPayloadType;
    WebRtc_UWord16              _h264SVCRelaySequenceNumber;
    WebRtc_UWord32              _h264SVCRelayTimeStamp;
    bool                      _h264SVCRelayLayerComplete;


private:
    // H.264
    WebRtc_Word32 SendH264_SingleMode(const FrameType frameType,
                                const H264Info* ptrH264Info,
                                    WebRtc_UWord16 &idxNALU,
                                    const WebRtc_Word8 payloadType,
                                    const WebRtc_UWord32 captureTimeStamp,
                                    WebRtc_Word32 &payloadBytesToSend,
                                    const WebRtc_UWord8*& data,
                                    const WebRtc_UWord16 rtpHeaderLength,
                                    const bool sendSVCPACSI=false);

    WebRtc_Word32 SendH264_FU_A(const FrameType frameType,
                              const H264Info* ptrH264Info,
                              WebRtc_UWord16 &idxNALU,
                              const WebRtc_Word8 payloadType,
                              const WebRtc_UWord32 captureTimeStamp,
                              WebRtc_Word32 &payloadBytesToSend,
                              const WebRtc_UWord8*& data,
                              const WebRtc_UWord16 rtpHeaderLength,
                              const bool sendSVCPACSI = false);

    WebRtc_Word32 SendH264_STAP_A(const FrameType frameType,
                            const H264Info* ptrH264Info,
                                WebRtc_UWord16 &idxNALU,
                                const WebRtc_Word8 payloadType,
                                const WebRtc_UWord32 captureTimeStamp,
                                bool& switchToFUA,
                                WebRtc_Word32 &payloadBytesToSend,
                                const WebRtc_UWord8*& data,
                                const WebRtc_UWord16 rtpHeaderLength);

    WebRtc_Word32 SendH264_STAP_A_PACSI(const FrameType frameType,
                                      const H264Info* ptrH264Info,
                                      WebRtc_UWord16 &idxNALU,
                                      const WebRtc_Word8 payloadType,
                                      const WebRtc_UWord32 captureTimeStamp,
                                      bool& switchToFUA,
                                      WebRtc_Word32 &payloadBytesToSend,
                                      const WebRtc_UWord8*& data,
                                      const WebRtc_UWord16 rtpHeaderLengh)

    WebRtc_Word32 SendH264_SinglePACSI(const FrameType frameType,
                                 const H264Info* ptrH264Info,
                                     const WebRtc_UWord16 idxNALU,
                                     const WebRtc_Word8 payloadType,
                                     const WebRtc_UWord32 captureTimeStamp,
                                     const bool firstPacketInNALU,
                                     const bool lastPacketInNALU);

    bool AddH264SVCNALUHeader(const H264_SVC_NALUHeader& svc,
                              WebRtc_UWord8* databuffer,
                              WebRtc_Word32& curByte) const;

    RTPSenderInterface&        _rtpSender;

    // relay
    bool                    _useHighestSendLayer;
    WebRtc_UWord8             _highestDependencyLayerOld;
    WebRtc_UWord8             _highestDependencyQualityIDOld;
    WebRtc_UWord8             _highestDependencyLayer;
    WebRtc_UWord8             _highestDependencyQualityID;
    WebRtc_UWord8             _highestTemporalLayer;


};

} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_RTP_SENDER_H264_H_
