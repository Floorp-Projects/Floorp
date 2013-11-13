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

namespace ModuleRTPUtility
{
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

    class RTPHeaderParser
    {
    public:
        RTPHeaderParser(const uint8_t* rtpData,
                        const uint32_t rtpDataLength);
        ~RTPHeaderParser();

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

    enum FrameTypes
    {
        kIFrame,    // key frame
        kPFrame         // Delta frame
    };

    struct RTPPayloadVP8
    {
        bool                 nonReferenceFrame;
        bool                 beginningOfPartition;
        int                  partitionID;
        bool                 hasPictureID;
        bool                 hasTl0PicIdx;
        bool                 hasTID;
        bool                 hasKeyIdx;
        int                  pictureID;
        int                  tl0PicIdx;
        int                  tID;
        bool                 layerSync;
        int                  keyIdx;
        int                  frameWidth;
        int                  frameHeight;

        const uint8_t*   data;
        uint16_t         dataLength;
    };

    union RTPPayloadUnion
    {
        RTPPayloadVP8   VP8;
    };

    struct RTPPayload
    {
        void SetType(RtpVideoCodecTypes videoType);

        RtpVideoCodecTypes  type;
        FrameTypes          frameType;
        RTPPayloadUnion     info;
    };

    // RTP payload parser
    class RTPPayloadParser
    {
    public:
        RTPPayloadParser(const RtpVideoCodecTypes payloadType,
                         const uint8_t* payloadData,
                         const uint16_t payloadDataLength, // Length w/o padding.
                         const int32_t id);

        ~RTPPayloadParser();

        bool Parse(RTPPayload& parsedPacket) const;

    private:
        bool ParseGeneric(RTPPayload& parsedPacket) const;

        bool ParseVP8(RTPPayload& parsedPacket) const;

        int ParseVP8Extension(RTPPayloadVP8 *vp8,
                              const uint8_t *dataPtr,
                              int dataLength) const;

        int ParseVP8PictureID(RTPPayloadVP8 *vp8,
                              const uint8_t **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8Tl0PicIdx(RTPPayloadVP8 *vp8,
                              const uint8_t **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8TIDAndKeyIdx(RTPPayloadVP8 *vp8,
                                 const uint8_t **dataPtr,
                                 int *dataLength,
                                 int *parsedBytes) const;

        int ParseVP8FrameSize(RTPPayload& parsedPacket,
                              const uint8_t *dataPtr,
                              int dataLength) const;

    private:
        int32_t               _id;
        const uint8_t*        _dataPtr;
        const uint16_t        _dataLength;
        const RtpVideoCodecTypes    _videoType;
    };

}  // namespace ModuleRTPUtility

}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
