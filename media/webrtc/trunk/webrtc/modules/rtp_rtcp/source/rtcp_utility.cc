/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

#include <cassert>
#include <cmath>   // ceil
#include <cstring> // memcpy

namespace webrtc {
// RTCPParserV2 : currently read only

RTCPUtility::RTCPParserV2::RTCPParserV2(const uint8_t* rtcpData,
                                        size_t rtcpDataLength,
                                        bool rtcpReducedSizeEnable)
    : _ptrRTCPDataBegin(rtcpData),
      _RTCPReducedSizeEnable(rtcpReducedSizeEnable),
      _ptrRTCPDataEnd(rtcpData + rtcpDataLength),
      _validPacket(false),
      _ptrRTCPData(rtcpData),
      _ptrRTCPBlockEnd(NULL),
      _state(State_TopLevel),
      _numberOfBlocks(0),
      _packetType(kRtcpNotValidCode) {
  Validate();
}

RTCPUtility::RTCPParserV2::~RTCPParserV2() {
}

ptrdiff_t
RTCPUtility::RTCPParserV2::LengthLeft() const
{
    return (_ptrRTCPDataEnd- _ptrRTCPData);
}

RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::PacketType() const
{
    return _packetType;
}

const RTCPUtility::RTCPPacket&
RTCPUtility::RTCPParserV2::Packet() const
{
    return _packet;
}

RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::Begin()
{
    _ptrRTCPData = _ptrRTCPDataBegin;

    return Iterate();
}

RTCPUtility::RTCPPacketTypes
RTCPUtility::RTCPParserV2::Iterate()
{
    // Reset packet type
    _packetType = kRtcpNotValidCode;

    if (IsValid())
    {
        switch (_state)
        {
        case State_TopLevel:
            IterateTopLevel();
            break;
        case State_ReportBlockItem:
            IterateReportBlockItem();
            break;
        case State_SDESChunk:
            IterateSDESChunk();
            break;
        case State_BYEItem:
            IterateBYEItem();
            break;
        case State_ExtendedJitterItem:
            IterateExtendedJitterItem();
            break;
        case State_RTPFB_NACKItem:
            IterateNACKItem();
            break;
        case State_RTPFB_TMMBRItem:
            IterateTMMBRItem();
            break;
        case State_RTPFB_TMMBNItem:
            IterateTMMBNItem();
            break;
        case State_PSFB_SLIItem:
            IterateSLIItem();
            break;
        case State_PSFB_RPSIItem:
            IterateRPSIItem();
            break;
        case State_PSFB_FIRItem:
            IterateFIRItem();
            break;
        case State_PSFB_AppItem:
            IteratePsfbAppItem();
            break;
        case State_PSFB_REMBItem:
            IteratePsfbREMBItem();
            break;
        case State_AppItem:
            IterateAppItem();
            break;
        default:
            assert(false); // Invalid state!
            break;
        }
    }
    return _packetType;
}

void
RTCPUtility::RTCPParserV2::IterateTopLevel()
{
    for (;;)
    {
        RTCPCommonHeader header;

        const bool success = RTCPParseCommonHeader(_ptrRTCPData,
                                                    _ptrRTCPDataEnd,
                                                    header);

        if (!success)
        {
            return;
        }
        _ptrRTCPBlockEnd = _ptrRTCPData + header.LengthInOctets;
        if (_ptrRTCPBlockEnd > _ptrRTCPDataEnd)
        {
            // Bad block!
            return;
        }

        switch (header.PT)
        {
        case PT_SR:
        {
            // number of Report blocks
            _numberOfBlocks = header.IC;
            ParseSR();
            return;
        }
        case PT_RR:
        {
            // number of Report blocks
            _numberOfBlocks = header.IC;
            ParseRR();
            return;
        }
        case PT_SDES:
        {
            // number of SDES blocks
            _numberOfBlocks = header.IC;
            const bool ok = ParseSDES();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_BYE:
        {
            _numberOfBlocks = header.IC;
            const bool ok = ParseBYE();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_IJ:
        {
            // number of Report blocks
            _numberOfBlocks = header.IC;
            ParseIJ();
            return;
        }
        case PT_RTPFB: // Fall through!
        case PT_PSFB:
        {
            const bool ok = ParseFBCommon(header);
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }

            return;
        }
        case PT_APP:
        {
            const bool ok = ParseAPP(header);
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        case PT_XR:
        {
            const bool ok = ParseXR();
            if (!ok)
            {
                // Nothing supported found, continue to next block!
                break;
            }
            return;
        }
        default:
            // Not supported! Skip!
            EndCurrentBlock();
            break;
        }
    }
}

void
RTCPUtility::RTCPParserV2::IterateReportBlockItem()
{
    const bool success = ParseReportBlockItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateSDESChunk()
{
    const bool success = ParseSDESChunk();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateBYEItem()
{
    const bool success = ParseBYEItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateExtendedJitterItem()
{
    const bool success = ParseIJItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateNACKItem()
{
    const bool success = ParseNACKItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateTMMBRItem()
{
    const bool success = ParseTMMBRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateTMMBNItem()
{
    const bool success = ParseTMMBNItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateSLIItem()
{
    const bool success = ParseSLIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateRPSIItem()
{
    const bool success = ParseRPSIItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateFIRItem()
{
    const bool success = ParseFIRItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IteratePsfbAppItem()
{
    const bool success = ParsePsfbAppItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IteratePsfbREMBItem()
{
    const bool success = ParsePsfbREMBItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::IterateAppItem()
{
    const bool success = ParseAPPItem();
    if (!success)
    {
        Iterate();
    }
}

void
RTCPUtility::RTCPParserV2::Validate()
{
    if (_ptrRTCPData == NULL)
    {
        return; // NOT VALID
    }

    RTCPCommonHeader header;
    const bool success = RTCPParseCommonHeader(_ptrRTCPDataBegin,
                                               _ptrRTCPDataEnd,
                                               header);

    if (!success)
    {
        return; // NOT VALID!
    }

    // * if (!reducedSize) : first packet must be RR or SR.
    //
    // * The padding bit (P) should be zero for the first packet of a
    //   compound RTCP packet because padding should only be applied,
    //   if it is needed, to the last packet. (NOT CHECKED!)
    //
    // * The length fields of the individual RTCP packets must add up
    //   to the overall length of the compound RTCP packet as
    //   received.  This is a fairly strong check. (NOT CHECKED!)

    if (!_RTCPReducedSizeEnable)
    {
        if ((header.PT != PT_SR) && (header.PT != PT_RR))
        {
            return; // NOT VALID
        }
    }

    _validPacket = true;
}

bool
RTCPUtility::RTCPParserV2::IsValid() const
{
    return _validPacket;
}

void
RTCPUtility::RTCPParserV2::EndCurrentBlock()
{
    _ptrRTCPData = _ptrRTCPBlockEnd;
}

bool
RTCPUtility::RTCPParseCommonHeader( const uint8_t* ptrDataBegin,
                                    const uint8_t* ptrDataEnd,
                                    RTCPCommonHeader& parsedHeader)
{
    if (!ptrDataBegin || !ptrDataEnd)
    {
        return false;
    }

    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |V=2|P|    IC   |      PT       |             length            |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //
    // Common header for all RTCP packets, 4 octets.

    if ((ptrDataEnd - ptrDataBegin) < 4)
    {
        return false;
    }

    parsedHeader.V              = ptrDataBegin[0] >> 6;
    parsedHeader.P              = ((ptrDataBegin[0] & 0x20) == 0) ? false : true;
    parsedHeader.IC             = ptrDataBegin[0] & 0x1f;
    parsedHeader.PT             = ptrDataBegin[1];

    parsedHeader.LengthInOctets = (ptrDataBegin[2] << 8) + ptrDataBegin[3] + 1;
    parsedHeader.LengthInOctets *= 4;

    if(parsedHeader.LengthInOctets == 0)
    {
        return false;
    }
    // Check if RTP version field == 2
    if (parsedHeader.V != 2)
    {
        return false;
    }

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseRR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        return false;
    }


    _ptrRTCPData += 4; // Skip header

    _packetType = kRtcpRrCode;

    _packet.RR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.RR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.RR.SenderSSRC += *_ptrRTCPData++;

    _packet.RR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    _state = State_ReportBlockItem;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 28)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = kRtcpSrCode;

    _packet.SR.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.SR.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.SR.SenderSSRC += *_ptrRTCPData++;

    _packet.SR.NTPMostSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPMostSignificant += *_ptrRTCPData++;

    _packet.SR.NTPLeastSignificant = *_ptrRTCPData++ << 24;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 16;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++ << 8;
    _packet.SR.NTPLeastSignificant += *_ptrRTCPData++;

    _packet.SR.RTPTimestamp = *_ptrRTCPData++ << 24;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 16;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++ << 8;
    _packet.SR.RTPTimestamp += *_ptrRTCPData++;

    _packet.SR.SenderPacketCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderPacketCount += *_ptrRTCPData++;

    _packet.SR.SenderOctetCount = *_ptrRTCPData++ << 24;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 16;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++ << 8;
    _packet.SR.SenderOctetCount += *_ptrRTCPData++;

    _packet.SR.NumberOfReportBlocks = _numberOfBlocks;

    // State transition
    if(_numberOfBlocks != 0)
    {
        _state = State_ReportBlockItem;
    }else
    {
        // don't go to state report block item if 0 report blocks
        _state = State_TopLevel;
        EndCurrentBlock();
    }
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseReportBlockItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 24 || _numberOfBlocks <= 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packet.ReportBlockItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.SSRC += *_ptrRTCPData++;

    _packet.ReportBlockItem.FractionLost = *_ptrRTCPData++;

    _packet.ReportBlockItem.CumulativeNumOfPacketsLost = *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.CumulativeNumOfPacketsLost += *_ptrRTCPData++;

    _packet.ReportBlockItem.ExtendedHighestSequenceNumber = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.ExtendedHighestSequenceNumber += *_ptrRTCPData++;

    _packet.ReportBlockItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.Jitter += *_ptrRTCPData++;

    _packet.ReportBlockItem.LastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.LastSR += *_ptrRTCPData++;

    _packet.ReportBlockItem.DelayLastSR = *_ptrRTCPData++ << 24;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 16;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++ << 8;
    _packet.ReportBlockItem.DelayLastSR += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = kRtcpReportBlockItemCode;
    return true;
}

/* From RFC 5450: Transmission Time Offsets in RTP Streams.
      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                      inter-arrival jitter                     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     .                                                               .
     .                                                               .
     .                                                               .
     |                      inter-arrival jitter                     |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool
RTCPUtility::RTCPParserV2::ParseIJ()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packetType = kRtcpExtendedIjCode;

    // State transition
    _state = State_ExtendedJitterItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseIJItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4 || _numberOfBlocks <= 0)
    {
        _state = State_TopLevel;
        EndCurrentBlock();
        return false;
    }

    _packet.ExtendedJitterReportItem.Jitter = *_ptrRTCPData++ << 24;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 16;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++ << 8;
    _packet.ExtendedJitterReportItem.Jitter += *_ptrRTCPData++;

    _numberOfBlocks--;
    _packetType = kRtcpExtendedIjItemCode;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSDES()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _ptrRTCPData += 4; // Skip header

    _state = State_SDESChunk;
    _packetType = kRtcpSdesCode;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSDESChunk()
{
    if(_numberOfBlocks <= 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _numberOfBlocks--;

    // Find CName item in a SDES chunk.
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const ptrdiff_t dataLen = _ptrRTCPBlockEnd - _ptrRTCPData;
        if (dataLen < 4)
        {
            _state = State_TopLevel;

            EndCurrentBlock();
            return false;
        }

        uint32_t SSRC = *_ptrRTCPData++ << 24;
        SSRC += *_ptrRTCPData++ << 16;
        SSRC += *_ptrRTCPData++ << 8;
        SSRC += *_ptrRTCPData++;

        const bool foundCname = ParseSDESItem();
        if (foundCname)
        {
            _packet.CName.SenderSSRC = SSRC; // Add SSRC
            return true;
        }
    }
    _state = State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPUtility::RTCPParserV2::ParseSDESItem()
{
    // Find CName
    // Only the CNAME item is mandatory. RFC 3550 page 46
    bool foundCName = false;

    size_t itemOctetsRead = 0;
    while (_ptrRTCPData < _ptrRTCPBlockEnd)
    {
        const uint8_t tag = *_ptrRTCPData++;
        ++itemOctetsRead;

        if (tag == 0)
        {
            // End tag! 4 oct aligned
            while ((itemOctetsRead++ % 4) != 0)
            {
                ++_ptrRTCPData;
            }
            return foundCName;
        }

        if (_ptrRTCPData < _ptrRTCPBlockEnd)
        {
            const uint8_t len = *_ptrRTCPData++;
            ++itemOctetsRead;

            if (tag == 1)
            {
                // CNAME

                // Sanity
                if ((_ptrRTCPData + len) >= _ptrRTCPBlockEnd)
                {
                    _state = State_TopLevel;

                    EndCurrentBlock();
                    return false;
                }
                uint8_t i = 0;
                for (; i < len; ++i)
                {
                    const uint8_t c = _ptrRTCPData[i];
                    if ((c < ' ') || (c > '{') || (c == '%') || (c == '\\'))
                    {
                        // Illegal char
                        _state = State_TopLevel;

                        EndCurrentBlock();
                        return false;
                    }
                    _packet.CName.CName[i] = c;
                }
                // Make sure we are null terminated.
                _packet.CName.CName[i] = 0;
                _packetType = kRtcpSdesChunkCode;

                foundCName = true;
            }
            _ptrRTCPData += len;
            itemOctetsRead += len;
        }
    }

    // No end tag found!
    _state = State_TopLevel;

    EndCurrentBlock();
    return false;
}

bool
RTCPUtility::RTCPParserV2::ParseBYE()
{
    _ptrRTCPData += 4; // Skip header

    _state = State_BYEItem;

    return ParseBYEItem();
}

bool
RTCPUtility::RTCPParserV2::ParseBYEItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4 || _numberOfBlocks == 0)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpByeCode;

    _packet.BYE.SenderSSRC = *_ptrRTCPData++ << 24;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 16;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++ << 8;
    _packet.BYE.SenderSSRC += *_ptrRTCPData++;

    // we can have several CSRCs attached

    // sanity
    if(length >= 4*_numberOfBlocks)
    {
        _ptrRTCPData += (_numberOfBlocks -1)*4;
    }
    _numberOfBlocks = 0;

    return true;
}
/*
  0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|reserved |   PT=XR=207   |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                         report blocks                         :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool RTCPUtility::RTCPParserV2::ParseXR()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip header

    _packet.XR.OriginatorSSRC = *_ptrRTCPData++ << 24;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 16;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++ << 8;
    _packet.XR.OriginatorSSRC += *_ptrRTCPData++;

    return ParseXRItem();
}
/*
    0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      BT       | type-specific |         block length          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    :             type-specific block contents                      :
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool
RTCPUtility::RTCPParserV2::ParseXRItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4) //
    {
        EndCurrentBlock();
        return false;
    }

    uint8_t blockType = *_ptrRTCPData++;
    uint8_t typeSpecific = *_ptrRTCPData++;

    uint16_t blockLength = *_ptrRTCPData++ << 8;
    blockLength = *_ptrRTCPData++;

    if(blockType == 7 && typeSpecific == 0)
    {
        if(blockLength != 8)
        {
            EndCurrentBlock();
            return false;
        }
        return ParseXRVOIPMetricItem();
    }else
    {
        EndCurrentBlock();
        return false;
    }
}
/*
 0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=7      |   reserved    |       block length = 8        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        SSRC of source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   loss rate   | discard rate  | burst density |  gap density  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |       burst duration          |         gap duration          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     round trip delay          |       end system delay        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | signal level  |  noise level  |     RERL      |     Gmin      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   R factor    | ext. R factor |    MOS-LQ     |    MOS-CQ     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   RX config   |   reserved    |          JB nominal           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          JB maximum           |          JB abs max           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
bool
RTCPUtility::RTCPParserV2::ParseXRVOIPMetricItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 28)
    {
        EndCurrentBlock();
        return false;
    }
    _packetType = kRtcpXrVoipMetricCode;

    _packet.XRVOIPMetricItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.SSRC += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.lossRate = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.discardRate = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.burstDensity = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.gapDensity = *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.burstDuration = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.burstDuration += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.gapDuration = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.gapDuration += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.roundTripDelay = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.roundTripDelay += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.endSystemDelay = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.endSystemDelay += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.signalLevel = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.noiseLevel = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.RERL = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.Gmin = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.Rfactor = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.extRfactor = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.MOSLQ = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.MOSCQ = *_ptrRTCPData++;
    _packet.XRVOIPMetricItem.RXconfig = *_ptrRTCPData++;
    _ptrRTCPData++; // skip reserved

    _packet.XRVOIPMetricItem.JBnominal = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBnominal += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.JBmax = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBmax += *_ptrRTCPData++;

    _packet.XRVOIPMetricItem.JBabsMax = *_ptrRTCPData++ << 8;
    _packet.XRVOIPMetricItem.JBabsMax += *_ptrRTCPData++;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseFBCommon(const RTCPCommonHeader& header)
{
    assert((header.PT == PT_RTPFB) || (header.PT == PT_PSFB)); // Parser logic check

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 12) // 4 * 3, RFC4585 section 6.1
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = *_ptrRTCPData++ << 24;
    senderSSRC += *_ptrRTCPData++ << 16;
    senderSSRC += *_ptrRTCPData++ << 8;
    senderSSRC += *_ptrRTCPData++;

    uint32_t mediaSSRC = *_ptrRTCPData++ << 24;
    mediaSSRC += *_ptrRTCPData++ << 16;
    mediaSSRC += *_ptrRTCPData++ << 8;
    mediaSSRC += *_ptrRTCPData++;

    if (header.PT == PT_RTPFB)
    {
        // Transport layer feedback

        switch (header.IC)
        {
        case 1:
        {
            // NACK
            _packetType             = kRtcpRtpfbNackCode;
            _packet.NACK.SenderSSRC = senderSSRC;
            _packet.NACK.MediaSSRC  = mediaSSRC;

            _state = State_RTPFB_NACKItem;

            return true;
        }
        case 2:
        {
            // used to be ACK is this code point, which is removed
            // conficts with http://tools.ietf.org/html/draft-levin-avt-rtcp-burst-00
            break;
        }
        case 3:
        {
            // TMMBR
            _packetType              = kRtcpRtpfbTmmbrCode;
            _packet.TMMBR.SenderSSRC = senderSSRC;
            _packet.TMMBR.MediaSSRC  = mediaSSRC;

            _state = State_RTPFB_TMMBRItem;

            return true;
        }
        case 4:
        {
            // TMMBN
            _packetType              = kRtcpRtpfbTmmbnCode;
            _packet.TMMBN.SenderSSRC = senderSSRC;
            _packet.TMMBN.MediaSSRC  = mediaSSRC;

            _state = State_RTPFB_TMMBNItem;

            return true;
        }
        case 5:
         {
            // RTCP-SR-REQ Rapid Synchronisation of RTP Flows
            // draft-perkins-avt-rapid-rtp-sync-03.txt
            // trigger a new RTCP SR
            _packetType = kRtcpRtpfbSrReqCode;

            // Note: No state transition, SR REQ is empty!
            return true;
        }
        default:
            break;
        }
        EndCurrentBlock();
        return false;
    }
    else if (header.PT == PT_PSFB)
    {
        // Payload specific feedback
        switch (header.IC)
        {
        case 1:
            // PLI
            _packetType            = kRtcpPsfbPliCode;
            _packet.PLI.SenderSSRC = senderSSRC;
            _packet.PLI.MediaSSRC  = mediaSSRC;

            // Note: No state transition, PLI FCI is empty!
            return true;
        case 2:
            // SLI
            _packetType            = kRtcpPsfbSliCode;
            _packet.SLI.SenderSSRC = senderSSRC;
            _packet.SLI.MediaSSRC  = mediaSSRC;

            _state = State_PSFB_SLIItem;

            return true;
        case 3:
            _packetType             = kRtcpPsfbRpsiCode;
            _packet.RPSI.SenderSSRC = senderSSRC;
            _packet.RPSI.MediaSSRC  = mediaSSRC;

            _state = State_PSFB_RPSIItem;
            return true;
        case 4:
            // FIR
            _packetType            = kRtcpPsfbFirCode;
            _packet.FIR.SenderSSRC = senderSSRC;
            _packet.FIR.MediaSSRC  = mediaSSRC;

            _state = State_PSFB_FIRItem;
            return true;
        case 15:
            _packetType                = kRtcpPsfbAppCode;
            _packet.PSFBAPP.SenderSSRC = senderSSRC;
            _packet.PSFBAPP.MediaSSRC  = mediaSSRC;

            _state = State_PSFB_AppItem;
            return true;
        default:
            break;
        }

        EndCurrentBlock();
        return false;
    }
    else
    {
        assert(false);

        EndCurrentBlock();
        return false;
    }
}

bool
RTCPUtility::RTCPParserV2::ParseRPSIItem()
{
    // RFC 4585 6.3.3.  Reference Picture Selection Indication (RPSI)
    /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      PB       |0| Payload Type|    Native RPSI bit string     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |   defined per codec          ...                | Padding (0) |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(length > 2+RTCP_RPSI_DATA_SIZE)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpPsfbRpsiCode;

    uint8_t paddingBits = *_ptrRTCPData++;
    _packet.RPSI.PayloadType = *_ptrRTCPData++;

    memcpy(_packet.RPSI.NativeBitString, _ptrRTCPData, length-2);

    _packet.RPSI.NumberOfValidBits = uint16_t(length-2)*8 - paddingBits;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseNACKItem()
{
    // RFC 4585 6.2.1. Generic NACK

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpRtpfbNackItemCode;

    _packet.NACKItem.PacketID = *_ptrRTCPData++ << 8;
    _packet.NACKItem.PacketID += *_ptrRTCPData++;

    _packet.NACKItem.BitMask = *_ptrRTCPData++ << 8;
    _packet.NACKItem.BitMask += *_ptrRTCPData++;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParsePsfbAppItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'R')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'E')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'M')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    if(*_ptrRTCPData++ != 'B')
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kRtcpPsfbRembCode;
    _state = State_PSFB_REMBItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParsePsfbREMBItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packet.REMBItem.NumberOfSSRCs = *_ptrRTCPData++;
    const uint8_t brExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t brMantissa = (_ptrRTCPData[0] & 0x03) << 16;
    brMantissa += (_ptrRTCPData[1] << 8);
    brMantissa += (_ptrRTCPData[2]);

    _ptrRTCPData += 3; // Fwd read data
    _packet.REMBItem.BitRate = (brMantissa << brExp);

    const ptrdiff_t length_ssrcs = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length_ssrcs < 4 * _packet.REMBItem.NumberOfSSRCs)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpPsfbRembItemCode;

    for (int i = 0; i < _packet.REMBItem.NumberOfSSRCs; i++)
    {
        _packet.REMBItem.SSRCs[i] = *_ptrRTCPData++ << 24;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 16;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++ << 8;
        _packet.REMBItem.SSRCs[i] += *_ptrRTCPData++;
    }
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseTMMBRItem()
{
    // RFC 5104 4.2.1. Temporary Maximum Media Stream Bit Rate Request (TMMBR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpRtpfbTmmbrItemCode;

    _packet.TMMBRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBRItem.SSRC += *_ptrRTCPData++;

    uint8_t mxtbrExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t mxtbrMantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mxtbrMantissa += (_ptrRTCPData[1] << 7);
    mxtbrMantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    _packet.TMMBRItem.MaxTotalMediaBitRate = ((mxtbrMantissa << mxtbrExp) / 1000);
    _packet.TMMBRItem.MeasuredOverhead     = measuredOH;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseTMMBNItem()
{
    // RFC 5104 4.2.2. Temporary Maximum Media Stream Bit Rate Notification (TMMBN)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpRtpfbTmmbnItemCode;

    _packet.TMMBNItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.TMMBNItem.SSRC += *_ptrRTCPData++;

    uint8_t mxtbrExp = (_ptrRTCPData[0] >> 2) & 0x3F;

    uint32_t mxtbrMantissa = (_ptrRTCPData[0] & 0x03) << 15;
    mxtbrMantissa += (_ptrRTCPData[1] << 7);
    mxtbrMantissa += (_ptrRTCPData[2] >> 1) & 0x7F;

    uint32_t measuredOH = (_ptrRTCPData[2] & 0x01) << 8;
    measuredOH += _ptrRTCPData[3];

    _ptrRTCPData += 4; // Fwd read data

    _packet.TMMBNItem.MaxTotalMediaBitRate = ((mxtbrMantissa << mxtbrExp) / 1000);
    _packet.TMMBNItem.MeasuredOverhead     = measuredOH;

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseSLIItem()
{
    // RFC 5104 6.3.2.  Slice Loss Indication (SLI)
    /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |            First        |        Number           | PictureID |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kRtcpPsfbSliItemCode;

    uint32_t buffer;
    buffer = *_ptrRTCPData++ << 24;
    buffer += *_ptrRTCPData++ << 16;
    buffer += *_ptrRTCPData++ << 8;
    buffer += *_ptrRTCPData++;

    _packet.SLIItem.FirstMB = uint16_t((buffer>>19) & 0x1fff);
    _packet.SLIItem.NumberOfMB = uint16_t((buffer>>6) & 0x1fff);
    _packet.SLIItem.PictureId = uint8_t(buffer & 0x3f);

    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseFIRItem()
{
    // RFC 5104 4.3.1. Full Intra Request (FIR)

    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 8)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }

    _packetType = kRtcpPsfbFirItemCode;

    _packet.FIRItem.SSRC = *_ptrRTCPData++ << 24;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 16;
    _packet.FIRItem.SSRC += *_ptrRTCPData++ << 8;
    _packet.FIRItem.SSRC += *_ptrRTCPData++;

    _packet.FIRItem.CommandSequenceNumber = *_ptrRTCPData++;
    _ptrRTCPData += 3; // Skip "Reserved" bytes.
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseAPP( const RTCPCommonHeader& header)
{
    ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;

    if (length < 12) // 4 * 3, RFC 3550 6.7 APP: Application-Defined RTCP Packet
    {
        EndCurrentBlock();
        return false;
    }

    _ptrRTCPData += 4; // Skip RTCP header

    uint32_t senderSSRC = *_ptrRTCPData++ << 24;
    senderSSRC += *_ptrRTCPData++ << 16;
    senderSSRC += *_ptrRTCPData++ << 8;
    senderSSRC += *_ptrRTCPData++;

    uint32_t name = *_ptrRTCPData++ << 24;
    name += *_ptrRTCPData++ << 16;
    name += *_ptrRTCPData++ << 8;
    name += *_ptrRTCPData++;

    length  = _ptrRTCPBlockEnd - _ptrRTCPData;

    _packetType = kRtcpAppCode;

    _packet.APP.SubType = header.IC;
    _packet.APP.Name = name;

    _state = State_AppItem;
    return true;
}

bool
RTCPUtility::RTCPParserV2::ParseAPPItem()
{
    const ptrdiff_t length = _ptrRTCPBlockEnd - _ptrRTCPData;
    if (length < 4)
    {
        _state = State_TopLevel;

        EndCurrentBlock();
        return false;
    }
    _packetType = kRtcpAppItemCode;

    if(length > kRtcpAppCode_DATA_SIZE)
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, kRtcpAppCode_DATA_SIZE);
        _packet.APP.Size = kRtcpAppCode_DATA_SIZE;
        _ptrRTCPData += kRtcpAppCode_DATA_SIZE;
    }else
    {
        memcpy(_packet.APP.Data, _ptrRTCPData, length);
        _packet.APP.Size = (uint16_t)length;
        _ptrRTCPData += length;
    }
    return true;
}

RTCPUtility::RTCPPacketIterator::RTCPPacketIterator(uint8_t* rtcpData,
                                                    size_t rtcpDataLength)
    : _ptrBegin(rtcpData),
      _ptrEnd(rtcpData + rtcpDataLength),
      _ptrBlock(NULL) {
  memset(&_header, 0, sizeof(_header));
}

RTCPUtility::RTCPPacketIterator::~RTCPPacketIterator() {
}

const RTCPUtility::RTCPCommonHeader*
RTCPUtility::RTCPPacketIterator::Begin()
{
    _ptrBlock = _ptrBegin;

    return Iterate();
}

const RTCPUtility::RTCPCommonHeader*
RTCPUtility::RTCPPacketIterator::Iterate()
{
    const bool success = RTCPParseCommonHeader(_ptrBlock, _ptrEnd, _header);
    if (!success)
    {
        _ptrBlock = NULL;
        return NULL;
    }
    _ptrBlock += _header.LengthInOctets;

    if (_ptrBlock > _ptrEnd)
    {
        _ptrBlock = NULL;
        return  NULL;
    }

    return &_header;
}

const RTCPUtility::RTCPCommonHeader*
RTCPUtility::RTCPPacketIterator::Current()
{
    if (!_ptrBlock)
    {
        return NULL;
    }

    return &_header;
}
} // namespace webrtc
