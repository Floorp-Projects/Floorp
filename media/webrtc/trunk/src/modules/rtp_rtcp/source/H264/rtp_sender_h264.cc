/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_sender_h264.h"

#include "rtp_utility.h"

namespace webrtc {
RTPSenderH264::RTPSenderH264(RTPSenderInterface* rtpSender) :
    // H264
    _rtpSender(*rtpSender),
    _h264Mode(H264_SINGLE_NAL_MODE),
    _h264SendPPS_SPS(true),
    _h264SVCPayloadType(-1),
    _h264SVCRelaySequenceNumber(0),
    _h264SVCRelayTimeStamp(0),
    _h264SVCRelayLayerComplete(false),

    _useHighestSendLayer(false),
    _highestDependencyLayerOld(MAX_NUMBER_OF_TEMPORAL_ID-1),
    _highestDependencyQualityIDOld(MAX_NUMBER_OF_DEPENDENCY_QUALITY_ID-1),
    _highestDependencyLayer(0),
    _highestDependencyQualityID(0),
    _highestTemporalLayer(0)
{
}

RTPSenderH264::~RTPSenderH264()
{
}

WebRtc_Word32
RTPSenderH264::Init()
{
    _h264SendPPS_SPS = true;
    _h264Mode = H264_SINGLE_NAL_MODE;
    return 0;
}

/*
    multi-session
    3 modes supported
    NI-T        timestamps
    NI-TC        timestamps/CS-DON
    NI-C        CS-DON

    Non-interleaved timestamp based mode (NI-T)
    Non-interleaved cross-session decoding order number (CS-DON) based mode (NI-C)
    Non-interleaved combined timestamp and CS-DON mode (NI-TC)

    NOT supported  Interleaved CS-DON (I-C) mode.

    NI-T and NI-TC modes both use timestamps to recover the decoding
    order.  In order to be able to do so, it is necessary for the RTP
    packet stream to contain data for all sampling instances of a given
    RTP session in all enhancement RTP sessions that depend on the given
    RTP session.  The NI-C and I-C modes do not have this limitation,
    and use the CS-DON values as a means to explicitly indicate decoding
    order, either directly coded in PACSI NAL units, or inferred from
    them using the packetization rules.  It is noted that the NI-TC mode
    offers both alternatives and it is up to the receiver to select
    which one to use.
*/

bool
RTPSenderH264::AddH264SVCNALUHeader(const H264_SVC_NALUHeader& svc,
                                    WebRtc_UWord8* databuffer,
                                    WebRtc_Word32& curByte) const
{
   // +---------------+---------------+---------------+
   // |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
   // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   // |R|I|   PRID    |N| DID |  QID  | TID |U|D|O| RR|
   // +---------------+---------------+---------------+

   // R    - Reserved for future extensions (MUST be 1). Receivers SHOULD ignore the value of R.
   // I    - Is layer representation an IDR layer (1) or not (0).
   // PRID - Priority identifier for the NAL unit.
   // N    - Specifies whether inter-layer prediction may be used for decoding the coded slice (1) or not (0).
   // DID  - Indicates the WebRtc_Word32er-layer coding dependency level of a layer representation.
   // QID  - Indicates the quality level of an MGS layer representation.
   // TID  - Indicates the temporal level of a layer representation.
   // U    - Use only reference base pictures during the WebRtc_Word32er prediction process (1) or not (0).
   // D    - Discardable flag.
   // O    - Output_flag. Affects the decoded picture output process as defined in Annex C of [H.264].
   // RR   - Reserved_three_2bits (MUST be '11'). Receivers SHOULD ignore the value of RR.

   // Add header data
   databuffer[curByte++] = (svc.r << 7)              + (svc.idr << 6)           + (svc.priorityID & 0x3F);
   databuffer[curByte++] = (svc.interLayerPred << 7) + (svc.dependencyID << 4)  + (svc.qualityID & 0x0F);
   databuffer[curByte++] = (svc.temporalID << 5)     + (svc.useRefBasePic << 4) + (svc.discardable << 3) +
                           (svc.output << 2)         + (svc.rr & 0x03);
   return true;
}

WebRtc_Word32
RTPSenderH264::AddH264PACSINALU(const bool firstPacketInNALU,
                                const bool lastPacketInNALU,
                                const H264_PACSI_NALU& pacsi,
                                const H264_SVC_NALUHeader& svc,
                                const WebRtc_UWord16 DONC,
                                WebRtc_UWord8* databuffer,
                                WebRtc_Word32& curByte) const
{
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |F|NRI|Type(30) |              SVC NAL unit header              |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |X|Y|T|A|P|C|S|E| TL0PICIDX (o.)|        IDRPICID (o.)          |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |          DONC (o.)            |        NAL unit size 1        |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                                                               |
    // |                 SEI NAL unit 1                                |
    // |                                                               |
    // |                         +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                         |        NAL unit size 2        |     |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+     |
    // |                                                               |
    // |            SEI NAL unit 2                                     |
    // |                                           +-+-+-+-+-+-+-+-+-+-+
    // |                                           |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


    // If present, MUST be first NAL unit in aggregation packet + there MUST be at least
    // one additional unit in the same packet! The RTPHeader and payload header are set as if the 2nd NAL unit
    // (first non-PACSI NAL unit) is encapsulated in the same packet.
    // contains scalability info common for all remaining NAL units.

    // todo add API to configure this required for multisession
    const bool addDONC = false;

    if (svc.length == 0 || pacsi.NALlength == 0)
    {
      return 0;
    }

    WebRtc_Word32 startByte = curByte;

    // NAL unit header
    databuffer[curByte++] = 30; // NRI will be added later

    // Extended SVC header
    AddH264SVCNALUHeader(svc, databuffer, curByte);

    // Flags
    databuffer[curByte++] = (pacsi.X << 7) +
                            (pacsi.Y << 6) +
                            (addDONC << 5) +
                            (pacsi.A << 4) +
                            (pacsi.P << 3) +
                            (pacsi.C << 2) +
                            firstPacketInNALU?(pacsi.S << 1):0 +
                            lastPacketInNALU?(pacsi.E):0;

    // Optional fields
    if (pacsi.Y)
    {
        databuffer[curByte++] = pacsi.TL0picIDx;
        databuffer[curByte++] = (WebRtc_UWord8)(pacsi.IDRpicID >> 8);
        databuffer[curByte++] = (WebRtc_UWord8)(pacsi.IDRpicID);
    }
    // Decoding order number
    if (addDONC) // pacsi.T
    {
        databuffer[curByte++] = (WebRtc_UWord8)(DONC >> 8);
        databuffer[curByte++] = (WebRtc_UWord8)(DONC);
    }

    // SEI NALU
    if(firstPacketInNALU) // IMPROVEMENT duplicate it to make sure it arrives...
    {
        // we only set this for NALU 0 to make sure we send it only once per frame
        for (WebRtc_UWord32 i = 0; i < pacsi.numSEINALUs; i++)
        {
            // NALU size
            databuffer[curByte++] = (WebRtc_UWord8)(pacsi.seiMessageLength[i] >> 8);
            databuffer[curByte++] = (WebRtc_UWord8)(pacsi.seiMessageLength[i]);

            // NALU data
            memcpy(databuffer + curByte, pacsi.seiMessageData[i], pacsi.seiMessageLength[i]);
            curByte += pacsi.seiMessageLength[i];
        }
    }
    return curByte - startByte;
}

WebRtc_Word32
RTPSenderH264::SetH264RelaySequenceNumber(const WebRtc_UWord16 seqNum)
{
    _h264SVCRelaySequenceNumber = seqNum;
    return 0;
}

WebRtc_Word32
RTPSenderH264::SetH264RelayCompleteLayer(const bool complete)
{
    _h264SVCRelayLayerComplete = complete;
    return 0;
}

/*
    12  Filler data

        The only restriction of filler data NAL units within an
        access unit is that they shall not precede the first VCL
        NAL unit with the same access unit.
*/
WebRtc_Word32
RTPSenderH264::SendH264FillerData(const WebRtcRTPHeader* rtpHeader,
                                  const WebRtc_UWord16 bytesToSend,
                                  const WebRtc_UWord32 ssrc)
{
    WebRtc_UWord16 fillerLength = bytesToSend - 12 - 1;

    if (fillerLength > WEBRTC_IP_PACKET_SIZE - 12 - 1)
    {
        return 0;
    }

    if (fillerLength == 0)
    {
        // do not send an empty packet, will not reach JB
        fillerLength = 1;
    }

    // send codec valid data, H.264 has defined data which is binary 1111111
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];

    dataBuffer[0] = static_cast<WebRtc_UWord8>(0x80);            // version 2
    dataBuffer[1] = rtpHeader->header.payloadType;
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, _rtpSender.IncrementSequenceNumber()); // get the current SequenceNumber and add by 1 after returning
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+4, rtpHeader->header.timestamp);
    ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, rtpHeader->header.ssrc);

    // set filler NALU type
    dataBuffer[12] = 12;        // NRI field = 0, type 12

    // fill with 0xff
    memset(dataBuffer + 12 + 1, 0xff, fillerLength);

    return _rtpSender.SendToNetwork(dataBuffer,
                        fillerLength,
                        12 + 1);
}

WebRtc_Word32
RTPSenderH264::SendH264FillerData(const WebRtc_UWord32 captureTimestamp,
                                  const WebRtc_UWord8 payloadType,
                                  const WebRtc_UWord32 bytes
                                  )
{

    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();
    WebRtc_UWord16 maxLength = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - _rtpSender.RTPHeaderLength();

    WebRtc_Word32 bytesToSend=bytes;
    WebRtc_UWord16 fillerLength=0;

    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];

    while(bytesToSend>0)
    {
        fillerLength=maxLength;
        if(fillerLength<maxLength)
        {
            fillerLength = (WebRtc_UWord16) bytesToSend;
        }

        bytesToSend-=fillerLength;

        if (fillerLength > WEBRTC_IP_PACKET_SIZE - 12 - 1)
        {
            return 0;
        }

        if (fillerLength == 0)
        {
            // do not send an empty packet, will not reach JB
            fillerLength = 1;
        }

        // send paded data
        // correct seq num, time stamp and payloadtype
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, false,captureTimestamp, true, true);

        // set filler NALU type
        dataBuffer[12] = 12;        // NRI field = 0, type 12

        // send codec valid data, H.264 has defined data which is binary 1111111
        // fill with 0xff
        memset(dataBuffer + 12 + 1, 0xff, fillerLength-1);

        if( _rtpSender.SendToNetwork(dataBuffer,
                            fillerLength,
                            12)<0)
        {

            return -1;;
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderH264::SendH264SVCRelayPacket(const WebRtcRTPHeader* rtpHeader,
                                      const WebRtc_UWord8* incomingRTPPacket,
                                      const WebRtc_UWord16 incomingRTPPacketSize,
                                      const WebRtc_UWord32 ssrc,
                                      const bool higestLayer)
{
    if (rtpHeader->header.sequenceNumber != (WebRtc_UWord16)(_h264SVCRelaySequenceNumber + 1))
    {
         // not continous, signal loss
         _rtpSender.IncrementSequenceNumber();
    }
    _h264SVCRelaySequenceNumber = rtpHeader->header.sequenceNumber;


    if (rtpHeader->header.timestamp != _h264SVCRelayTimeStamp)
    {
        // new frame
        _h264SVCRelayLayerComplete = false;
    }

    if (rtpHeader->header.timestamp == _h264SVCRelayTimeStamp &&
        _h264SVCRelayLayerComplete)
    {
        // sanity, end of layer already sent
        // Could happened for fragmented packet with missing PACSI info (PACSI packet reorded and received after packet it belongs to)
        // fragmented packet has no layer info set (default info 0)
        return 0;
    }
    _h264SVCRelayTimeStamp = rtpHeader->header.timestamp;

    // re-packetize H.264-SVC packets
    // we keep the timestap unchanged
    // make a copy and only change the SSRC and seqNum

    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];
    memcpy(dataBuffer, incomingRTPPacket, incomingRTPPacketSize);

    // _sequenceNumber initiated in Init()
    // _ssrc initiated in constructor

    // re-write payload type
    if(_h264SVCPayloadType != -1)
    {
        dataBuffer[1] &= kRtpMarkerBitMask;
        dataBuffer[1] += _h264SVCPayloadType;
    }

    // _sequenceNumber will not work for re-ordering by NACK from original sender
    // engine responsible for this
    ModuleRTPUtility::AssignUWord16ToBuffer(dataBuffer+2, _rtpSender.IncrementSequenceNumber()); // get the current SequenceNumber and add by 1 after returning
    //ModuleRTPUtility::AssignUWord32ToBuffer(dataBuffer+8, ssrc);

    // how do we know it's the last relayed packet in a frame?
    // 1) packets arrive in order, the engine manages that
    // 2) highest layer that we relay
    // 3) the end bit is set for the highest layer

    if(higestLayer && rtpHeader->type.Video.codecHeader.H264.relayE)
    {
        // set marker bit
        dataBuffer[1] |= kRtpMarkerBitMask;

        // set relayed layer as complete
        _h264SVCRelayLayerComplete = true;
    }
    return _rtpSender.SendToNetwork(dataBuffer,
                         incomingRTPPacketSize - rtpHeader->header.headerLength,
                         rtpHeader->header.headerLength);
}

WebRtc_Word32
RTPSenderH264::SendH264_STAP_A(const FrameType frameType,
                                const H264Info* ptrH264Info,
                                WebRtc_UWord16 &idxNALU,
                                const WebRtc_Word8 payloadType,
                                const WebRtc_UWord32 captureTimeStamp,
                                bool& switchToFUA,
                                WebRtc_Word32 &payloadBytesToSend,
                                const WebRtc_UWord8*& data,
                                const WebRtc_UWord16 rtpHeaderLength)
{
    const WebRtc_Word32 H264_NALU_LENGTH = 2;

    WebRtc_UWord16 h264HeaderLength = 1; // normal header length
    WebRtc_UWord16 maxPayloadLengthSTAP_A = _rtpSender.MaxPayloadLength() -
                                          FECPacketOverhead() - rtpHeaderLength -
                                          h264HeaderLength - H264_NALU_LENGTH;

    WebRtc_Word32 dataOffset = rtpHeaderLength + h264HeaderLength;
    WebRtc_UWord8 NRI = 0;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];

    if (ptrH264Info->payloadSize[idxNALU] > maxPayloadLengthSTAP_A)
    {
        // we need to fragment NAL switch to mode FU-A
        switchToFUA = true;
    } else
    {
        // combine as many NAL units in every IP packet
        do
        {
            if(!_h264SendPPS_SPS)
            {
                // don't send NALU of type 7 and 8 SPS and PPS
                if(ptrH264Info->type[idxNALU] == 7 || ptrH264Info->type[idxNALU] == 8)
                {
                    payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                    data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                    idxNALU++;
                    continue;
                }
            }
            if(ptrH264Info->payloadSize[idxNALU] + payloadBytesInPacket <= maxPayloadLengthSTAP_A)
            {
                if(ptrH264Info->NRI[idxNALU] > NRI)
                {
                    NRI = ptrH264Info->NRI[idxNALU];
                }
                // put NAL size into packet
                dataBuffer[dataOffset] = (WebRtc_UWord8)(ptrH264Info->payloadSize[idxNALU] >> 8);
                dataOffset++;
                dataBuffer[dataOffset] = (WebRtc_UWord8)(ptrH264Info->payloadSize[idxNALU] & 0xff);
                dataOffset++;
                // Put payload in packet
                memcpy(&dataBuffer[dataOffset], &data[ptrH264Info->startCodeSize[idxNALU]], ptrH264Info->payloadSize[idxNALU]);
                dataOffset += ptrH264Info->payloadSize[idxNALU];
                data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                payloadBytesInPacket += (WebRtc_UWord16)(ptrH264Info->payloadSize[idxNALU] + H264_NALU_LENGTH);
                payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
            } else
            {
                // we don't fitt the next NALU in this packet
                break;
            }
            idxNALU++;
        }while(payloadBytesToSend);
    }

    // sanity
    // don't send empty packets
    if (payloadBytesInPacket)
    {
        // add RTP header
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, (payloadBytesToSend==0)?true:false, captureTimeStamp);
        dataBuffer[rtpHeaderLength] = 24 + NRI; // STAP-A == 24
        WebRtc_UWord16 payloadLength = payloadBytesInPacket + h264HeaderLength;

        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadLength, rtpHeaderLength))
        {
            return -1;
        }
    }
    return 0;
} // end STAP-A

// STAP-A for H.264 SVC
WebRtc_Word32
RTPSenderH264::SendH264_STAP_A_PACSI(const FrameType frameType,
                                      const H264Info* ptrH264Info,
                                      WebRtc_UWord16 &idxNALU,
                                      const WebRtc_Word8 payloadType,
                                      const WebRtc_UWord32 captureTimeStamp,
                                      bool& switchToFUA,
                                      WebRtc_Word32 &payloadBytesToSend,
                                      const WebRtc_UWord8*& data,
                                      const WebRtc_UWord16 rtpHeaderLength,
                                      WebRtc_UWord16& decodingOrderNumber)
{
    const WebRtc_Word32 H264_NALU_LENGTH = 2;

    WebRtc_UWord16 h264HeaderLength = 1; // normal header length
    WebRtc_UWord16 maxPayloadLengthSTAP_A = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength - h264HeaderLength - H264_NALU_LENGTH;
    WebRtc_Word32 dataOffset = rtpHeaderLength + h264HeaderLength;
    WebRtc_UWord8 NRI = 0;
    WebRtc_UWord16 payloadBytesInPacket = 0;
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];
    bool firstNALUNotIDR = true; //delta

    // Put PACSI NAL unit into packet
    WebRtc_Word32 lengthPACSI = 0;
    WebRtc_UWord32 PACSI_NALlength = ptrH264Info->PACSI[idxNALU].NALlength;
    if (PACSI_NALlength > maxPayloadLengthSTAP_A)
    {
        return -1;
    }
    dataBuffer[dataOffset++] = (WebRtc_UWord8)(PACSI_NALlength >> 8);
    dataBuffer[dataOffset++] = (WebRtc_UWord8)(PACSI_NALlength & 0xff);

    // end bit will be updated later, since another NALU in this packet might be the last
    WebRtc_Word32 lengthPASCINALU = AddH264PACSINALU(true,
                                                   false,
                                                   ptrH264Info->PACSI[idxNALU],
                                                   ptrH264Info->SVCheader[idxNALU],
                           decodingOrderNumber,
                           dataBuffer,
                                                   dataOffset);
    if (lengthPASCINALU <= 0)
    {
        return -1;
    }
    decodingOrderNumber++;

    lengthPACSI = H264_NALU_LENGTH + lengthPASCINALU;
    maxPayloadLengthSTAP_A -= (WebRtc_UWord16)lengthPACSI;
    if (ptrH264Info->payloadSize[idxNALU] > maxPayloadLengthSTAP_A)
    {
        // we need to fragment NAL switch to mode FU-A
        switchToFUA = true;
        return 0;
    }
    if(!ptrH264Info->SVCheader[idxNALU].idr)
    {
        firstNALUNotIDR = true;
    }

    WebRtc_UWord32 layer = (ptrH264Info->SVCheader[idxNALU].dependencyID << 16)+
                         (ptrH264Info->SVCheader[idxNALU].qualityID << 8) +
                          ptrH264Info->SVCheader[idxNALU].temporalID;

    {
        // combine as many NAL units in every IP packet, with the same priorityID
        // Improvement we could allow several very small MGS NALU from different layers to be sent in one packet

        do
        {
            if(!_h264SendPPS_SPS)
            {
                // Don't send NALU of type 7 and 8 SPS and PPS,
                // they could be signaled outofband
                if(ptrH264Info->type[idxNALU] == 7 || ptrH264Info->type[idxNALU] == 8)
                {
                    payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                    data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                    idxNALU++;
                    continue;
                }
            }
            //    don't send NALU type 6 (SEI message) not allowed when we send it in PACSI
            if(ptrH264Info->type[idxNALU] == 6)
            {
                // SEI NALU Don't send, not allowed when we send it in PACSI
                payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                idxNALU++;
                continue;
            }

            const WebRtc_UWord32 layerNALU = (ptrH264Info->SVCheader[idxNALU].dependencyID << 16)+
                                           (ptrH264Info->SVCheader[idxNALU].qualityID << 8) +
                                            ptrH264Info->SVCheader[idxNALU].temporalID;

            // we need to break on a new layer
            if( ptrH264Info->payloadSize[idxNALU] + payloadBytesInPacket <= maxPayloadLengthSTAP_A &&
                layerNALU == layer)
            {
                if(ptrH264Info->NRI[idxNALU] > NRI)
                {
                    NRI = ptrH264Info->NRI[idxNALU];
                }
                // put NAL size into packet
                dataBuffer[dataOffset] = (WebRtc_UWord8)(ptrH264Info->payloadSize[idxNALU] >> 8);
                dataOffset++;
                dataBuffer[dataOffset] = (WebRtc_UWord8)(ptrH264Info->payloadSize[idxNALU] & 0xff);
                dataOffset++;
                // Put payload in packet
                memcpy(&dataBuffer[dataOffset], &data[ptrH264Info->startCodeSize[idxNALU]], ptrH264Info->payloadSize[idxNALU]);
                dataOffset += ptrH264Info->payloadSize[idxNALU];
                data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
                payloadBytesInPacket += (WebRtc_UWord16)(ptrH264Info->payloadSize[idxNALU] + H264_NALU_LENGTH);
                payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
            } else
            {
                // we don't fitt the next NALU in this packet or,
                // it's the next layer

                // check if we should send this NALU
                // based on the layer

                if(_useHighestSendLayer && layerNALU != layer)
                {
                    // we don't send this NALU due to it's a new layer
                    // check if we should send the next or if this is the last
                    const WebRtc_UWord8 dependencyQualityID = (ptrH264Info->SVCheader[idxNALU].dependencyID << 4) + ptrH264Info->SVCheader[idxNALU].qualityID;

                    bool highestLayer;
                    if(SendH264SVCLayer(frameType,
                                        ptrH264Info->SVCheader[idxNALU].temporalID,
                                        dependencyQualityID,
                                        highestLayer) == false)
                    {
                        // will trigger markerbit and stop sending this frame
                        payloadBytesToSend = 0;
                    }
                }
                break;
            }
            idxNALU++;

        }while(payloadBytesToSend);
    }

    // sanity, don't send empty packets
    if (payloadBytesInPacket)
    {
        // add RTP header
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, (payloadBytesToSend==0)?true:false, captureTimeStamp);

        dataBuffer[rtpHeaderLength] = 24 + NRI; // STAP-A == 24

        // NRI for PACSI
        dataBuffer[rtpHeaderLength + H264_NALU_LENGTH + 1] &= 0x1f;   // zero out NRI field
        dataBuffer[rtpHeaderLength + H264_NALU_LENGTH + 1] |= NRI;

        if(ptrH264Info->PACSI[idxNALU-1].E)
        {
            // update end bit
            dataBuffer[rtpHeaderLength + H264_NALU_LENGTH + 5] |= 0x01;
        }
        if(firstNALUNotIDR)
        {
            // we have to check if any of the NALU in this packet is an IDR NALU
            bool setIBit = false;
            for(int i = 0; i < idxNALU; i++)
            {
                if(ptrH264Info->SVCheader[i].idr)
                {
                    setIBit = true;
                    break;
                }
            }
            if(setIBit)
            {
                // update I bit
                dataBuffer[rtpHeaderLength + H264_NALU_LENGTH + 2] |= 0x40;
            }
        }
        const WebRtc_UWord16 payloadLength = payloadBytesInPacket + h264HeaderLength + (WebRtc_UWord16)lengthPACSI;
        if(-1 == SendVideoPacket(frameType,
                                 dataBuffer,
                                 payloadLength,
                                 rtpHeaderLength,
                                 layer==0))
        {
            return -1;
        }
    }
    return 0;
} // end STAP-A

WebRtc_Word32
RTPSenderH264::SendH264_FU_A(const FrameType frameType,
                              const H264Info* ptrH264Info,
                              WebRtc_UWord16 &idxNALU,
                              const WebRtc_Word8 payloadType,
                              const WebRtc_UWord32 captureTimeStamp,
                              WebRtc_Word32 &payloadBytesToSend,
                              const WebRtc_UWord8*& data,
                              const WebRtc_UWord16 rtpHeaderLength,
                              WebRtc_UWord16& decodingOrderNumber,
                              const bool sendSVCPACSI)
{

    // FUA for the rest of the frame
    WebRtc_UWord16 maxPayloadLength = _rtpSender.MaxPayloadLength() - FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];
    WebRtc_UWord32 payloadBytesRemainingInNALU = ptrH264Info->payloadSize[idxNALU];

    bool isBaseLayer=false;

    if(payloadBytesRemainingInNALU > maxPayloadLength)
    {
        // we need to fragment NALU
        const WebRtc_UWord16 H264_FUA_LENGTH = 2; // FU-a H.264 header is 2 bytes

        if(sendSVCPACSI)
        {
            SendH264_SinglePACSI(frameType,
                                 ptrH264Info,
                                 idxNALU,
                                 payloadType,
                                 captureTimeStamp,
                                 true,
                                 false);

            WebRtc_UWord32 layer = (ptrH264Info->SVCheader[idxNALU].dependencyID << 16)+
                                 (ptrH264Info->SVCheader[idxNALU].qualityID << 8) +
                                  ptrH264Info->SVCheader[idxNALU].temporalID;
            isBaseLayer=(layer==0);
        }

        // First packet
        _rtpSender.BuildRTPheader(dataBuffer,payloadType, false, captureTimeStamp);

        WebRtc_UWord16 maxPayloadLengthFU_A = maxPayloadLength - H264_FUA_LENGTH ;
        WebRtc_UWord8 fuaIndc = 28 + ptrH264Info->NRI[idxNALU];
        dataBuffer[rtpHeaderLength] = fuaIndc;                                                     // FU-A indicator
        dataBuffer[rtpHeaderLength+1] = (WebRtc_UWord8)(ptrH264Info->type[idxNALU] + 0x80)/*start*/; // FU-A header

        memcpy(&dataBuffer[rtpHeaderLength + H264_FUA_LENGTH], &data[ptrH264Info->startCodeSize[idxNALU]+1], maxPayloadLengthFU_A);
        WebRtc_UWord16 payloadLength = maxPayloadLengthFU_A + H264_FUA_LENGTH;
        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadLength, rtpHeaderLength, isBaseLayer))
        {
            return -1;
        }

        //+1 is from the type that is coded into the FU-a header
        data += maxPayloadLengthFU_A + 1 + ptrH264Info->startCodeSize[idxNALU];             // inc data ptr
        payloadBytesToSend -= maxPayloadLengthFU_A+1+ptrH264Info->startCodeSize[idxNALU];
        payloadBytesRemainingInNALU -= maxPayloadLengthFU_A+1;

        // all non first/last packets
        while(payloadBytesRemainingInNALU  > maxPayloadLengthFU_A)
        {
            if(sendSVCPACSI)
            {
                SendH264_SinglePACSI(frameType,
                                     ptrH264Info,
                                     idxNALU,
                                     payloadType,
                                     captureTimeStamp,
                                     false,
                                     false);
            }

            // prepare next header
            _rtpSender.BuildRTPheader(dataBuffer, payloadType, false, captureTimeStamp);

            dataBuffer[rtpHeaderLength] = (WebRtc_UWord8)fuaIndc;           // FU-A indicator
            dataBuffer[rtpHeaderLength+1] = ptrH264Info->type[idxNALU];   // FU-A header

            memcpy(&dataBuffer[rtpHeaderLength+H264_FUA_LENGTH], data, maxPayloadLengthFU_A);
            payloadLength = maxPayloadLengthFU_A + H264_FUA_LENGTH;

            if(-1 == SendVideoPacket(frameType, dataBuffer, payloadLength, rtpHeaderLength,isBaseLayer))
            {
                return -1;
            }
            data += maxPayloadLengthFU_A; // inc data ptr
            payloadBytesToSend -= maxPayloadLengthFU_A;
            payloadBytesRemainingInNALU -= maxPayloadLengthFU_A;
            dataBuffer[rtpHeaderLength] = fuaIndc;                         // FU-A indicator
            dataBuffer[rtpHeaderLength+1] = ptrH264Info->type[idxNALU];    // FU-A header
        }
        if(sendSVCPACSI)
        {
            SendH264_SinglePACSI(frameType,
                                 ptrH264Info,
                                 idxNALU,
                                 payloadType,
                                 captureTimeStamp,
                                 false,
                                 true); // last packet in NALU

            if(_useHighestSendLayer && idxNALU+1 < ptrH264Info->numNALUs)
            {
                // not last NALU in frame
                // check if it's the the next layer should not be sent

                // check if we should send the next or if this is the last
                const WebRtc_UWord8 dependencyQualityID = (ptrH264Info->SVCheader[idxNALU+1].dependencyID << 4) +
                                                         ptrH264Info->SVCheader[idxNALU+1].qualityID;

                bool highestLayer;
                if(SendH264SVCLayer(frameType,
                                    ptrH264Info->SVCheader[idxNALU+1].temporalID,
                                    dependencyQualityID,
                                    highestLayer) == false)
                {
                    // will trigger markerbit and stop sending this frame
                    payloadBytesToSend = payloadBytesRemainingInNALU;
                }
            }
        }
        // last packet in NALU
        _rtpSender.BuildRTPheader(dataBuffer, payloadType,(payloadBytesToSend == (WebRtc_Word32)payloadBytesRemainingInNALU)?true:false, captureTimeStamp);
        dataBuffer[rtpHeaderLength+1] = ptrH264Info->type[idxNALU] + 0x40/*stop*/; // FU-A header

        memcpy(&dataBuffer[rtpHeaderLength+H264_FUA_LENGTH], data, payloadBytesRemainingInNALU);
        payloadLength = (WebRtc_UWord16)payloadBytesRemainingInNALU + H264_FUA_LENGTH;
        payloadBytesToSend -= payloadBytesRemainingInNALU;
        if(payloadBytesToSend != 0)
        {
            data += payloadBytesRemainingInNALU; // inc data ptr
        }
        idxNALU++;
        if(-1 == SendVideoPacket(frameType, dataBuffer, payloadLength, rtpHeaderLength,isBaseLayer))
        {
            return -1;
        }
    } else
    {
        // send NAL unit in singel mode
        return SendH264_SingleMode(frameType,
                                   ptrH264Info,
                                   idxNALU,
                                   payloadType,
                                   captureTimeStamp,
                                   payloadBytesToSend,
                                   data,
                                   rtpHeaderLength,
                                   sendSVCPACSI);
    }
    // end FU-a
    return 0;
}

WebRtc_Word32
RTPSenderH264::SendH264_SingleMode(const FrameType frameType,
                                    const H264Info* ptrH264Info,
                                    WebRtc_UWord16 &idxNALU,
                                    const WebRtc_Word8 payloadType,
                                    const WebRtc_UWord32 captureTimeStamp,
                                    WebRtc_Word32 &payloadBytesToSend,
                                    const WebRtc_UWord8*& data,
                                    const WebRtc_UWord16 rtpHeaderLength,
                                    WebRtc_UWord16& decodingOrderNumber,
                                    const bool sendSVCPACSI)
{
    // no H.264 header lenght in single mode
    // we use WEBRTC_IP_PACKET_SIZE instead of the configured MTU since it's better to send fragmented UDP than not to send
    const WebRtc_UWord16 maxPayloadLength = WEBRTC_IP_PACKET_SIZE - _rtpSender.PacketOverHead() - FECPacketOverhead() - rtpHeaderLength;
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];
    bool isBaseLayer=false;

    if(ptrH264Info->payloadSize[idxNALU] > maxPayloadLength)
    {
        return -3;
    }
    if(!_h264SendPPS_SPS)
    {
        // don't send NALU of type 7 and 8 SPS and PPS
        if(ptrH264Info->type[idxNALU] == 7 || ptrH264Info->type[idxNALU] == 8)
        {
            payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
            data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
            idxNALU++;
            return 0;
        }
    }
    if(sendSVCPACSI)
    {
        SendH264_SinglePACSI(frameType,
                             ptrH264Info,
                             idxNALU,
                             payloadType,
                             captureTimeStamp,
                             true,
                             true);

        WebRtc_UWord32 layer = (ptrH264Info->SVCheader[idxNALU].dependencyID << 16)+
                             (ptrH264Info->SVCheader[idxNALU].qualityID << 8) +
                              ptrH264Info->SVCheader[idxNALU].temporalID;
        isBaseLayer=(layer==0);
    }

    // Put payload in packet
    memcpy(&dataBuffer[rtpHeaderLength], &data[ptrH264Info->startCodeSize[idxNALU]], ptrH264Info->payloadSize[idxNALU]);

    WebRtc_UWord16 payloadBytesInPacket = (WebRtc_UWord16)ptrH264Info->payloadSize[idxNALU];
    payloadBytesToSend -= ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU]; // left to send

    //
    _rtpSender.BuildRTPheader(dataBuffer,payloadType,(payloadBytesToSend ==0)?true:false, captureTimeStamp);

    dataBuffer[rtpHeaderLength] &= 0x1f; // zero out NRI field
    dataBuffer[rtpHeaderLength] |= ptrH264Info->NRI[idxNALU]; // nri
    if(payloadBytesToSend > 0)
    {
        data += ptrH264Info->payloadSize[idxNALU] + ptrH264Info->startCodeSize[idxNALU];
    }
    idxNALU++;
    if(-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket, rtpHeaderLength,isBaseLayer))
    {
        return -1;
    }
    return 0;
}

WebRtc_Word32
RTPSenderH264::SendH264_SinglePACSI(const FrameType frameType,
                                    const H264Info* ptrH264Info,
                                     const WebRtc_UWord16 idxNALU,
                                     const WebRtc_Word8 payloadType,
                                     const WebRtc_UWord32 captureTimeStamp,
                                     const bool firstPacketInNALU,
                                     const bool lastPacketInNALU);
{
    // Send PACSI in single mode
    WebRtc_UWord8 dataBuffer[WEBRTC_IP_PACKET_SIZE];
    WebRtc_UWord16 rtpHeaderLength = (WebRtc_UWord16)_rtpSender.BuildRTPheader(dataBuffer, payloadType,false, captureTimeStamp);
    WebRtc_Word32 dataOffset = rtpHeaderLength;

    WebRtc_Word32 lengthPASCINALU = AddH264PACSINALU(firstPacketInNALU,
                                                   lastPacketInNALU,
                                                   ptrH264Info->PACSI[idxNALU],
                                                   ptrH264Info->SVCheader[idxNALU],
                                                   decodingOrderNumber,
                                                   dataBuffer,
                                                   dataOffset);

    if (lengthPASCINALU <= 0)
    {
        return -1;
    }
    decodingOrderNumber++;

    WebRtc_UWord16 payloadBytesInPacket = (WebRtc_UWord16)lengthPASCINALU;

    // Set payload header (first payload byte co-serves as the payload header)
    dataBuffer[rtpHeaderLength] &= 0x1f;        // zero out NRI field
    dataBuffer[rtpHeaderLength] |= ptrH264Info->NRI[idxNALU]; // nri

    const WebRtc_UWord32 layer = (ptrH264Info->SVCheader[idxNALU].dependencyID << 16)+
                               (ptrH264Info->SVCheader[idxNALU].qualityID << 8) +
                                ptrH264Info->SVCheader[idxNALU].temporalID;

    if (-1 == SendVideoPacket(frameType, dataBuffer, payloadBytesInPacket, rtpHeaderLength,layer==0))
    {
        return -1;
    }
    return 0;
}




WebRtc_Word32
RTPSenderH264::SendH264SVC(const FrameType frameType,
                            const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize,
                            H264Information& h264Information,
                            WebRtc_UWord16& decodingOrderNumber)
{
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    const H264Info* ptrH264Info = NULL;
    if (h264Information.GetInfo(payloadData,payloadSize, ptrH264Info) == -1)
    {
        return -1;
    }
    if(_useHighestSendLayer)
    {
        // we need to check if we should drop the frame
        // it could be a temporal layer (aka a temporal frame)
        const WebRtc_UWord8 dependencyQualityID = (ptrH264Info->SVCheader[0].dependencyID << 4) + ptrH264Info->SVCheader[0].qualityID;

        bool dummyHighestLayer;
        if(SendH264SVCLayer(frameType,
                            ptrH264Info->SVCheader[0].temporalID,
                            dependencyQualityID,
                            dummyHighestLayer) == false)
        {
            // skip send this frame
            return 0;
        }
    }

    WebRtc_UWord16 idxNALU = 0;
    while (payloadBytesToSend > 0)
    {
        bool switchToFUA = false;
        if (SendH264_STAP_A_PACSI(frameType,
                                  ptrH264Info,
                                  idxNALU,
                                  payloadType,
                                  captureTimeStamp,
                                  switchToFUA,
                                  payloadBytesToSend,
                                  payloadData,
                                  rtpHeaderLength,
                                  decodingOrderNumber) != 0)
        {
            return -1;
        }
        if(switchToFUA)
        {
            // FU_A for this NALU
            if (SendH264_FU_A(frameType,
                              ptrH264Info,
                              idxNALU,
                              payloadType,
                              captureTimeStamp,
                              payloadBytesToSend,
                              payloadData,
                              rtpHeaderLength,
                              true) != 0)
            {
                return -1;
            }
        }
    }
    return 0;
}

WebRtc_Word32
RTPSenderH264::SetH264PacketizationMode(const H264PacketizationMode mode)
{
    _h264Mode = mode;
    return 0;
}

WebRtc_Word32
RTPSenderH264::SetH264SendModeNALU_PPS_SPS(const bool dontSend)
{
    _h264SendPPS_SPS = !dontSend;
    return 0;
}

bool
RTPSenderH264::SendH264SVCLayer(const FrameType frameType,
                                  const WebRtc_UWord8 temporalID,
                                  const WebRtc_UWord8 dependencyQualityID,
                                  bool& higestLayer)
{
    WebRtc_UWord8 dependencyID  = dependencyQualityID >> 4;

    // keyframe required to switch between dependency layers not quality and temporal
    if( _highestDependencyLayer != _highestDependencyLayerOld)
    {
        // we want to switch dependency layer
        if(frameType == kVideoFrameKey)
        {
            // key frame we can change layer if it's correct layer
            if(_highestDependencyLayer > _highestDependencyLayerOld)
            {
                // we want to switch up
                // does this packet belong to a new layer?

                if( dependencyID > _highestDependencyLayerOld &&
                    dependencyID <= _highestDependencyLayer)
                {
                    _highestDependencyLayerOld = dependencyID;
                    _highestDependencyQualityIDOld = _highestDependencyQualityID;

                    if( dependencyID == _highestDependencyLayer &&
                        dependencyQualityID == _highestDependencyQualityID)
                    {
                        higestLayer = true;
                    }
                    // relay
                    return true;
                }
            }
            if(_highestDependencyLayer < _highestDependencyLayerOld)
            {
                // we want to switch down
                // does this packet belong to a low layer?
                if( dependencyID <= _highestDependencyLayer)
                {
                    _highestDependencyLayerOld = dependencyID;
                    _highestDependencyQualityIDOld = _highestDependencyQualityID;
                    if( dependencyID == _highestDependencyLayer &&
                        dependencyQualityID == _highestDependencyQualityID)
                    {
                        higestLayer = true;
                    }
                    // relay
                    return true;
                }
            }
        } else
        {
            // Delta frame and we are waiting to switch dependency layer
            if(_highestDependencyLayer > _highestDependencyLayerOld)
            {
                // we want to switch up to a higher dependency layer
                // use old setting until we get a key-frame

                // filter based on old dependency
                // we could have allowed to add a MGS layer lower than the dependency ID
                // but then we can't know the highest layer relayed we assume that the user
                // will add one layer at a time
                if( _highestTemporalLayer < temporalID ||
                    _highestDependencyLayerOld < dependencyID ||
                    _highestDependencyQualityIDOld < dependencyQualityID)
                {
                    // drop
                    return false;
                }
                // highest layer based on old
                if( dependencyID == _highestDependencyLayerOld &&
                    dependencyQualityID == _highestDependencyQualityIDOld)
                {
                    higestLayer = true;
                }
            } else
            {
                // we want to switch down to a lower dependency layer,
                // use old setting, done bellow
                // drop all temporal layers while waiting for the key-frame
                if(temporalID > 0)
                {
                    // drop
                    return false;
                }
                // we can't drop a lower MGS layer since this might depend on it
                // however we can drop MGS layers larger than dependecyQualityId
                // with dependency from old and quality 0
                if( _highestDependencyLayerOld < dependencyID ||
                    (_highestDependencyQualityIDOld & 0xf0) < dependencyQualityID)
                {
                    // drop
                    return false;
                }
                if( dependencyID == _highestDependencyLayerOld &&
                    dependencyQualityID == (_highestDependencyQualityIDOld & 0xf0))
                {
                    higestLayer = true;
                }
            }
        }
    } else
    {
        // filter based on current state
        if( _highestTemporalLayer < temporalID ||
            _highestDependencyLayer < dependencyID ||
            _highestDependencyQualityID < dependencyQualityID)
        {
            // drop
            return false;
        }
        if( dependencyID == _highestDependencyLayer &&
            dependencyQualityID == _highestDependencyQualityID)
        {
            higestLayer = true;
        }
    }
    return true;
}

WebRtc_Word32
RTPSenderH264::SetHighestSendLayer(const WebRtc_UWord8 dependencyQualityLayer,
                                   const WebRtc_UWord8 temporalLayer)
{
    const WebRtc_UWord8 dependencyLayer = (dependencyQualityLayer >> 4);

    if(_highestDependencyLayerOld != _highestDependencyLayer)
    {
        // we have not switched to the new dependency yet
    } else
    {
        if(_highestDependencyLayer == dependencyLayer)
        {
            // no change of dependency
            // switch now _highestDependencyQualityIDOld
            _highestDependencyQualityIDOld = dependencyQualityLayer;
        }else
        {
            // change of dependency, update _highestDependencyQualityIDOld store as old
            _highestDependencyQualityIDOld = _highestDependencyQualityID;
        }
    }
    _useHighestSendLayer = true;
    _highestDependencyLayer = dependencyLayer;
    _highestDependencyQualityID = dependencyQualityLayer;
    _highestTemporalLayer = temporalLayer;
    return 0;
}

WebRtc_Word32
RTPSenderH264::HighestSendLayer(WebRtc_UWord8& dependencyQualityLayer,
                                WebRtc_UWord8& temporalLayer)
{
    if (!_useHighestSendLayer)
    {
        // No information set
        return -1;
    }
    dependencyQualityLayer = _highestDependencyQualityID;
    temporalLayer = _highestTemporalLayer;
    return 0;
}
/*
*   H.264
*/
WebRtc_Word32
RTPSenderH264::SendH264(const FrameType frameType,
                        const WebRtc_Word8 payloadType,
                        const WebRtc_UWord32 captureTimeStamp,
                        const WebRtc_UWord8* payloadData,
                        const WebRtc_UWord32 payloadSize,
                        H264Information& h264Information)
{
    WebRtc_Word32 payloadBytesToSend = payloadSize;
    const WebRtc_UWord8* data = payloadData;
    bool switchToFUA = false;
    const WebRtc_UWord16 rtpHeaderLength = _rtpSender.RTPHeaderLength();

    const H264Info* ptrH264Info = NULL;
    if (h264Information.GetInfo(payloadData,payloadSize, ptrH264Info) == -1)
    {
        return -1;
    }
    WebRtc_UWord16 idxNALU = 0;
    WebRtc_UWord16 DONCdummy = 0;

    while (payloadBytesToSend > 0)
    {
        switch(_h264Mode)
        {
        case H264_NON_INTERLEAVED_MODE:

            if(!switchToFUA)
            {
                if(SendH264_STAP_A(frameType,
                                   ptrH264Info,
                                   idxNALU,
                                   payloadType,
                                   captureTimeStamp,
                                   switchToFUA,
                                   payloadBytesToSend,
                                   data,
                                   rtpHeaderLength) != 0)
                {
                    return -1;
                }
            }
            else
            {
                // FUA for the rest of the frame
                if(SendH264_FU_A(frameType,
                                 ptrH264Info,
                                 idxNALU,
                                 payloadType,
                                 captureTimeStamp,
                                 payloadBytesToSend,
                                 data,
                                 rtpHeaderLength,
                                 DONCdummy) != 0)
                {
                    return -1;
                }
                // try to go back to STAP_A
                switchToFUA = false;
            }
            break;
        case H264_SINGLE_NAL_MODE:
            {
                // modeSingleU
                if(SendH264_SingleMode(frameType,
                                       ptrH264Info,
                                       idxNALU,
                                       payloadType,
                                       captureTimeStamp,
                                       payloadBytesToSend,
                                       data,
                                       rtpHeaderLength,
                                       DONCdummy) != 0)
                {
                    return -1;
                }
                break;
            }
        case H264_INTERLEAVED_MODE:
            // not supported
            assert(false);
            return -1;
        }
    }
    return 0;
}
} // namespace webrtc
