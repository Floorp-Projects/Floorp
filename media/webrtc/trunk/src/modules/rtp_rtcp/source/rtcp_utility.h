/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_

#include <cstddef> // size_t, ptrdiff_t

#include "typedefs.h"
#include "rtp_rtcp_config.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {
namespace RTCPUtility {
    // CNAME
    struct RTCPCnameInformation
    {
        char name[RTCP_CNAME_SIZE];
    };
    struct RTCPPacketRR
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord8  NumberOfReportBlocks;
    };
    struct RTCPPacketSR
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord8  NumberOfReportBlocks;

        // sender info
        WebRtc_UWord32 NTPMostSignificant;
        WebRtc_UWord32 NTPLeastSignificant;
        WebRtc_UWord32 RTPTimestamp;
        WebRtc_UWord32 SenderPacketCount;
        WebRtc_UWord32 SenderOctetCount;
    };
    struct RTCPPacketReportBlockItem
    {
        // report block
        WebRtc_UWord32 SSRC;
        WebRtc_UWord8  FractionLost;
        WebRtc_UWord32 CumulativeNumOfPacketsLost;
        WebRtc_UWord32 ExtendedHighestSequenceNumber;
        WebRtc_UWord32 Jitter;
        WebRtc_UWord32 LastSR;
        WebRtc_UWord32 DelayLastSR;
    };
    struct RTCPPacketSDESCName
    {
        // RFC3550
        WebRtc_UWord32 SenderSSRC;
        char CName[RTCP_CNAME_SIZE];
    };

    struct RTCPPacketExtendedJitterReportItem
    {
        // RFC 5450
        WebRtc_UWord32 Jitter;
    };

    struct RTCPPacketBYE
    {
        WebRtc_UWord32 SenderSSRC;
    };
    struct RTCPPacketXR
    {
        // RFC 3611
        WebRtc_UWord32 OriginatorSSRC;
    };
    struct RTCPPacketXRVOIPMetricItem
    {
        // RFC 3611 4.7
        WebRtc_UWord32    SSRC;
        WebRtc_UWord8     lossRate;
        WebRtc_UWord8     discardRate;
        WebRtc_UWord8     burstDensity;
        WebRtc_UWord8     gapDensity;
        WebRtc_UWord16    burstDuration;
        WebRtc_UWord16    gapDuration;
        WebRtc_UWord16    roundTripDelay;
        WebRtc_UWord16    endSystemDelay;
        WebRtc_UWord8     signalLevel;
        WebRtc_UWord8     noiseLevel;
        WebRtc_UWord8     RERL;
        WebRtc_UWord8     Gmin;
        WebRtc_UWord8     Rfactor;
        WebRtc_UWord8     extRfactor;
        WebRtc_UWord8     MOSLQ;
        WebRtc_UWord8     MOSCQ;
        WebRtc_UWord8     RXconfig;
        WebRtc_UWord16    JBnominal;
        WebRtc_UWord16    JBmax;
        WebRtc_UWord16    JBabsMax;
    };

    struct RTCPPacketRTPFBNACK
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC;
    };
    struct RTCPPacketRTPFBNACKItem
    {
        // RFC4585
        WebRtc_UWord16 PacketID;
        WebRtc_UWord16 BitMask;
    };

    struct RTCPPacketRTPFBTMMBR
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC; // zero!
    };
    struct RTCPPacketRTPFBTMMBRItem
    {
        // RFC5104
        WebRtc_UWord32 SSRC;
        WebRtc_UWord32 MaxTotalMediaBitRate; // In Kbit/s
        WebRtc_UWord32 MeasuredOverhead;
    };

    struct RTCPPacketRTPFBTMMBN
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC; // zero!
    };
    struct RTCPPacketRTPFBTMMBNItem
    {
        // RFC5104
        WebRtc_UWord32 SSRC; // "Owner"
        WebRtc_UWord32 MaxTotalMediaBitRate;
        WebRtc_UWord32 MeasuredOverhead;
    };

    struct RTCPPacketPSFBFIR
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC; // zero!
    };
    struct RTCPPacketPSFBFIRItem
    {
        // RFC5104
        WebRtc_UWord32 SSRC;
        WebRtc_UWord8  CommandSequenceNumber;
    };

    struct RTCPPacketPSFBPLI
    {
        // RFC4585
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC;
    };

    struct RTCPPacketPSFBSLI
    {
        // RFC4585
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC;
    };
    struct RTCPPacketPSFBSLIItem
    {
        // RFC4585
        WebRtc_UWord16 FirstMB;
        WebRtc_UWord16 NumberOfMB;
        WebRtc_UWord8 PictureId;
    };
    struct RTCPPacketPSFBRPSI
    {
        // RFC4585
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC;
        WebRtc_UWord8  PayloadType;
        WebRtc_UWord16 NumberOfValidBits;
        WebRtc_UWord8  NativeBitString[RTCP_RPSI_DATA_SIZE];
    };
    struct RTCPPacketPSFBAPP
    {
        WebRtc_UWord32 SenderSSRC;
        WebRtc_UWord32 MediaSSRC;
    };
    struct RTCPPacketPSFBREMBItem
    {
        WebRtc_UWord32 BitRate;
        WebRtc_UWord8 NumberOfSSRCs;
        WebRtc_UWord32 SSRCs[MAX_NUMBER_OF_REMB_FEEDBACK_SSRCS];
    };
    // generic name APP
    struct RTCPPacketAPP
    {
        WebRtc_UWord8     SubType;
        WebRtc_UWord32    Name;
        WebRtc_UWord8     Data[kRtcpAppCode_DATA_SIZE];
        WebRtc_UWord16    Size;
    };

    union RTCPPacket
    {
        RTCPPacketRR              RR;
        RTCPPacketSR              SR;
        RTCPPacketReportBlockItem ReportBlockItem;

        RTCPPacketSDESCName       CName;
        RTCPPacketBYE             BYE;

        RTCPPacketExtendedJitterReportItem ExtendedJitterReportItem;

        RTCPPacketRTPFBNACK       NACK;
        RTCPPacketRTPFBNACKItem   NACKItem;

        RTCPPacketPSFBPLI         PLI;
        RTCPPacketPSFBSLI         SLI;
        RTCPPacketPSFBSLIItem     SLIItem;
        RTCPPacketPSFBRPSI        RPSI;
        RTCPPacketPSFBAPP         PSFBAPP;
        RTCPPacketPSFBREMBItem    REMBItem;

        RTCPPacketRTPFBTMMBR      TMMBR;
        RTCPPacketRTPFBTMMBRItem  TMMBRItem;
        RTCPPacketRTPFBTMMBN      TMMBN;
        RTCPPacketRTPFBTMMBNItem  TMMBNItem;
        RTCPPacketPSFBFIR         FIR;
        RTCPPacketPSFBFIRItem     FIRItem;

        RTCPPacketXR               XR;
        RTCPPacketXRVOIPMetricItem XRVOIPMetricItem;

        RTCPPacketAPP             APP;
    };

    enum RTCPPacketTypes
    {
        kRtcpNotValidCode,

        // RFC3550
        kRtcpRrCode,
        kRtcpSrCode,
        kRtcpReportBlockItemCode,

        kRtcpSdesCode,
        kRtcpSdesChunkCode,
        kRtcpByeCode,

        // RFC5450
        kRtcpExtendedIjCode,
        kRtcpExtendedIjItemCode,

        // RFC4585
        kRtcpRtpfbNackCode,
        kRtcpRtpfbNackItemCode,

        kRtcpPsfbPliCode,
        kRtcpPsfbRpsiCode,
        kRtcpPsfbSliCode,
        kRtcpPsfbSliItemCode,
        kRtcpPsfbAppCode,
        kRtcpPsfbRembCode,
        kRtcpPsfbRembItemCode,

        // RFC5104
        kRtcpRtpfbTmmbrCode,
        kRtcpRtpfbTmmbrItemCode,
        kRtcpRtpfbTmmbnCode,
        kRtcpRtpfbTmmbnItemCode,
        kRtcpPsfbFirCode,
        kRtcpPsfbFirItemCode,

        // draft-perkins-avt-rapid-rtp-sync
        kRtcpRtpfbSrReqCode,

        // RFC 3611
        kRtcpXrVoipMetricCode,

        kRtcpAppCode,
        kRtcpAppItemCode,
    };

    struct RTCPRawPacket
    {
        const WebRtc_UWord8* _ptrPacketBegin;
        const WebRtc_UWord8* _ptrPacketEnd;
    };

    struct RTCPModRawPacket
    {
        WebRtc_UWord8* _ptrPacketBegin;
        WebRtc_UWord8* _ptrPacketEnd;
    };

    struct RTCPCommonHeader
    {
        WebRtc_UWord8  V;  // Version
        bool           P;  // Padding
        WebRtc_UWord8  IC; // Item count/subtype
        WebRtc_UWord8  PT; // Packet Type
        WebRtc_UWord16 LengthInOctets;
    };

    enum RTCPPT
    {
        PT_IJ    = 195,
        PT_SR    = 200,
        PT_RR    = 201,
        PT_SDES  = 202,
        PT_BYE   = 203,
        PT_APP   = 204,
        PT_RTPFB = 205,
        PT_PSFB  = 206,
        PT_XR    = 207
    };

    bool RTCPParseCommonHeader( const WebRtc_UWord8* ptrDataBegin,
                                const WebRtc_UWord8* ptrDataEnd,
                                RTCPCommonHeader& parsedHeader);

    class RTCPParserV2
    {
    public:
        RTCPParserV2(const WebRtc_UWord8* rtcpData,
                     size_t rtcpDataLength,
                     bool rtcpReducedSizeEnable); // Set to true, to allow non-compound RTCP!
        ~RTCPParserV2();

        RTCPPacketTypes PacketType() const;
        const RTCPPacket& Packet() const;
        const RTCPRawPacket& RawPacket() const;
        ptrdiff_t LengthLeft() const;

        bool IsValid() const;

        RTCPPacketTypes Begin();
        RTCPPacketTypes Iterate();

    private:
        enum ParseState
        {
            State_TopLevel,        // Top level packet
            State_ReportBlockItem, // SR/RR report block
            State_SDESChunk,       // SDES chunk
            State_BYEItem,         // BYE item
            State_ExtendedJitterItem, // Extended jitter report item
            State_RTPFB_NACKItem,  // NACK FCI item
            State_RTPFB_TMMBRItem, // TMMBR FCI item
            State_RTPFB_TMMBNItem, // TMMBN FCI item
            State_PSFB_SLIItem,    // SLI FCI item
            State_PSFB_RPSIItem,   // RPSI FCI item
            State_PSFB_FIRItem,    // FIR FCI item
            State_PSFB_AppItem,    // Application specific FCI item
            State_PSFB_REMBItem,   // Application specific REMB item
            State_XRItem,
            State_AppItem
        };

    private:
        void IterateTopLevel();
        void IterateReportBlockItem();
        void IterateSDESChunk();
        void IterateBYEItem();
        void IterateExtendedJitterItem();
        void IterateNACKItem();
        void IterateTMMBRItem();
        void IterateTMMBNItem();
        void IterateSLIItem();
        void IterateRPSIItem();
        void IterateFIRItem();
        void IteratePsfbAppItem();
        void IteratePsfbREMBItem();
        void IterateAppItem();

        void Validate();
        void EndCurrentBlock();

        bool ParseRR();
        bool ParseSR();
        bool ParseReportBlockItem();

        bool ParseSDES();
        bool ParseSDESChunk();
        bool ParseSDESItem();

        bool ParseBYE();
        bool ParseBYEItem();

        bool ParseIJ();
        bool ParseIJItem();

        bool ParseXR();
        bool ParseXRItem();
        bool ParseXRVOIPMetricItem();

        bool ParseFBCommon(const RTCPCommonHeader& header);
        bool ParseNACKItem();
        bool ParseTMMBRItem();
        bool ParseTMMBNItem();
        bool ParseSLIItem();
        bool ParseRPSIItem();
        bool ParseFIRItem();
        bool ParsePsfbAppItem();
        bool ParsePsfbREMBItem();

        bool ParseAPP(const RTCPCommonHeader& header);
        bool ParseAPPItem();

    private:
        const WebRtc_UWord8* const _ptrRTCPDataBegin;
        const bool                 _RTCPReducedSizeEnable;
        const WebRtc_UWord8* const _ptrRTCPDataEnd;

        bool                     _validPacket;
        const WebRtc_UWord8*     _ptrRTCPData;
        const WebRtc_UWord8*     _ptrRTCPBlockEnd;

        ParseState               _state;
        WebRtc_UWord8            _numberOfBlocks;

        RTCPPacketTypes          _packetType;
        RTCPPacket               _packet;
    };

    class RTCPPacketIterator
    {
    public:
        RTCPPacketIterator(WebRtc_UWord8* rtcpData,
                            size_t rtcpDataLength);
        ~RTCPPacketIterator();

        const RTCPCommonHeader* Begin();
        const RTCPCommonHeader* Iterate();
        const RTCPCommonHeader* Current();

    private:
        WebRtc_UWord8* const     _ptrBegin;
        WebRtc_UWord8* const     _ptrEnd;

        WebRtc_UWord8*           _ptrBlock;

        RTCPCommonHeader         _header;
    };
} // RTCPUtility
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_UTILITY_H_
