/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_

#include <stddef.h> // size_t, ptrdiff_t

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {

const uint8_t kRtpMarkerBitMask = 0x80;

RtpData* NullObjectRtpData();
RtpFeedback* NullObjectRtpFeedback();
RtpAudioFeedback* NullObjectRtpAudioFeedback();
ReceiveStatistics* NullObjectReceiveStatistics();

namespace RtpUtility {
    // January 1970, in NTP seconds.
    const uint32_t NTP_JAN_1970 = 2208988800UL;

    // Magic NTP fractional unit.
    const double NTP_FRAC = 4.294967296E+9;

    struct Payload
    {
        char name[RTP_PAYLOAD_NAME_SIZE];
        bool audio;
        PayloadUnion typeSpecific;
    };

    typedef std::map<int8_t, Payload*> PayloadTypeMap;

    // Return the current RTP timestamp from the NTP timestamp
    // returned by the specified clock.
    uint32_t GetCurrentRTP(Clock* clock, uint32_t freq);

    // Return the current RTP absolute timestamp.
    uint32_t ConvertNTPTimeToRTP(uint32_t NTPsec,
                                 uint32_t NTPfrac,
                                 uint32_t freq);

    uint32_t pow2(uint8_t exp);

    // Returns true if |newTimestamp| is older than |existingTimestamp|.
    // |wrapped| will be set to true if there has been a wraparound between the
    // two timestamps.
    bool OldTimestamp(uint32_t newTimestamp,
                      uint32_t existingTimestamp,
                      bool* wrapped);

    bool StringCompare(const char* str1,
                       const char* str2,
                       const uint32_t length);

    void AssignUWord32ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord24ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord16ToBuffer(uint8_t* dataBuffer, uint16_t value);

    /**
     * Converts a network-ordered two-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered two-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint16_t BufferToUWord16(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered three-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered three-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord24(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered four-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered four-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord32(const uint8_t* dataBuffer);

    class RtpHeaderParser {
    public:
     RtpHeaderParser(const uint8_t* rtpData, size_t rtpDataLength);
     ~RtpHeaderParser();

        bool RTCP() const;
        bool ParseRtcp(RTPHeader* header) const;
        bool Parse(RTPHeader& parsedPacket,
                   RtpHeaderExtensionMap* ptrExtensionMap = NULL) const;

    private:
        void ParseOneByteExtensionHeader(
            RTPHeader& parsedPacket,
            const RtpHeaderExtensionMap* ptrExtensionMap,
            const uint8_t* ptrRTPDataExtensionEnd,
            const uint8_t* ptr) const;

        uint8_t ParsePaddingBytes(
            const uint8_t* ptrRTPDataExtensionEnd,
            const uint8_t* ptr) const;

        const uint8_t* const _ptrRTPDataBegin;
        const uint8_t* const _ptrRTPDataEnd;
    };
}  // namespace RtpUtility
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
