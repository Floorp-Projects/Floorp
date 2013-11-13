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
    int32_t SendH264(const FrameType frameType,
                     const int8_t payloadType,
                     const uint32_t captureTimeStamp,
                     const uint8_t* payloadData,
                     const uint32_t payloadSize,
                     H264Information& h264Information);

    int32_t SendH264SVC(const FrameType frameType,
                        const int8_t payloadType,
                        const uint32_t captureTimeStamp,
                        const uint8_t* payloadData,
                        const uint32_t payloadSize,
                        H264Information& h264Information);

    // H.264 AVC
    int32_t SetH264PacketizationMode(const H264PacketizationMode mode);

    int32_t SetH264SendModeNALU_PPS_SPS(const bool dontSend);

    // H.264 SVC
    int32_t SetHighestSendLayer(const uint8_t dependencyQualityLayer,
                                const uint8_t temporalLayer);

    int32_t HighestSendLayer(uint8_t& dependencyQualityLayer,
                             uint8_t& temporalLayer);

protected:
    RTPSenderH264(RTPSenderInterface* rtpSender);
    virtual ~RTPSenderH264();

    int32_t Init();

    virtual uint16_t FECPacketOverhead() const = 0;
    virtual RtpVideoCodecTypes VideoCodecType() const = 0;

    virtual int32_t SendVideoPacket(const FrameType frameType,
                                   const uint8_t* dataBuffer,
                                   const uint16_t payloadLength,
                                   const uint16_t rtpHeaderLength,
                                   bool baseLayerVideoPacket=false) = 0;


    bool SendH264SVCLayer(const FrameType frameType,
                          const uint8_t temporalID,
                          const uint8_t dependencyQualityID,
                          bool& higestLayer);

    // H.264 SVC
    int32_t AddH264PACSINALU(const bool firstPacketInNALU,
                             const bool lastPacketInNALU,
                             const H264_PACSI_NALU& paci,
                             const H264_SVC_NALUHeader& svc,
                             const uint16_t DONC,
                             uint8_t* databuffer,
                             int32_t& curByte) const;

    int32_t SendH264FillerData(const WebRtcRTPHeader* rtpHeader,
                              const uint16_t bytesToSend,
                              const uint32_t ssrc);

    int32_t SendH264FillerData(const uint32_t captureTimestamp,
                               const uint8_t payloadType,
                               const uint32_t bytesToSend);

    int32_t SendH264SVCRelayPacket(const WebRtcRTPHeader* rtpHeader,
                                   const uint8_t* incomingRTPPacket,
                                   const uint16_t incomingRTPPacketSize,
                                   const uint32_t ssrc,
                                   const bool higestLayer);

    int32_t SetH264RelaySequenceNumber(const uint16_t seqNum);

    int32_t SetH264RelayCompleteLayer(const bool complete);

    // H.264
    H264PacketizationMode _h264Mode;
    bool                      _h264SendPPS_SPS;

    // H.264-SVC
    int8_t                _h264SVCPayloadType;
    uint16_t              _h264SVCRelaySequenceNumber;
    uint32_t              _h264SVCRelayTimeStamp;
    bool                      _h264SVCRelayLayerComplete;


private:
    // H.264
    int32_t SendH264_SingleMode(const FrameType frameType,
                                const H264Info* ptrH264Info,
                                uint16_t &idxNALU,
                                const int8_t payloadType,
                                const uint32_t captureTimeStamp,
                                int32_t &payloadBytesToSend,
                                const uint8_t*& data,
                                const uint16_t rtpHeaderLength,
                                const bool sendSVCPACSI=false);

    int32_t SendH264_FU_A(const FrameType frameType,
                          const H264Info* ptrH264Info,
                          uint16_t &idxNALU,
                          const int8_t payloadType,
                          const uint32_t captureTimeStamp,
                          int32_t &payloadBytesToSend,
                          const uint8_t*& data,
                          const uint16_t rtpHeaderLength,
                          const bool sendSVCPACSI = false);

    int32_t SendH264_STAP_A(const FrameType frameType,
                            const H264Info* ptrH264Info,
                            uint16_t &idxNALU,
                            const int8_t payloadType,
                            const uint32_t captureTimeStamp,
                            bool& switchToFUA,
                            int32_t &payloadBytesToSend,
                            const uint8_t*& data,
                            const uint16_t rtpHeaderLength);

    int32_t SendH264_STAP_A_PACSI(const FrameType frameType,
                                  const H264Info* ptrH264Info,
                                  uint16_t &idxNALU,
                                  const int8_t payloadType,
                                  const uint32_t captureTimeStamp,
                                  bool& switchToFUA,
                                  int32_t &payloadBytesToSend,
                                  const uint8_t*& data,
                                  const uint16_t rtpHeaderLengh)

    int32_t SendH264_SinglePACSI(const FrameType frameType,
                                 const H264Info* ptrH264Info,
                                 const uint16_t idxNALU,
                                 const int8_t payloadType,
                                 const uint32_t captureTimeStamp,
                                 const bool firstPacketInNALU,
                                 const bool lastPacketInNALU);

    bool AddH264SVCNALUHeader(const H264_SVC_NALUHeader& svc,
                              uint8_t* databuffer,
                              int32_t& curByte) const;

    RTPSenderInterface&        _rtpSender;

    // relay
    bool                    _useHighestSendLayer;
    uint8_t             _highestDependencyLayerOld;
    uint8_t             _highestDependencyQualityIDOld;
    uint8_t             _highestDependencyLayer;
    uint8_t             _highestDependencyQualityID;
    uint8_t             _highestTemporalLayer;


};

}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_H264_RTP_SENDER_H264_H_
