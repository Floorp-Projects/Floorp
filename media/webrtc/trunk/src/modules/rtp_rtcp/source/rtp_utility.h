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

#include <cstddef> // size_t, ptrdiff_t

#include "typedefs.h"
#include "rtp_header_extension.h"
#include "rtp_rtcp_config.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {
enum RtpVideoCodecTypes
{
    kRtpNoVideo       = 0,
    kRtpFecVideo      = 10,
    kRtpVp8Video      = 11
};

const WebRtc_UWord8 kRtpMarkerBitMask = 0x80;

namespace ModuleRTPUtility
{
    // January 1970, in NTP seconds.
    const uint32_t NTP_JAN_1970 = 2208988800UL;

    // Magic NTP fractional unit.
    const double NTP_FRAC = 4.294967296E+9;

    struct AudioPayload
    {
        WebRtc_UWord32    frequency;
        WebRtc_UWord8     channels;
        WebRtc_UWord32    rate;
    };
    struct VideoPayload
    {
        RtpVideoCodecTypes   videoCodecType;
        WebRtc_UWord32       maxRate;
    };
    union PayloadUnion
    {
        AudioPayload Audio;
        VideoPayload Video;
    };
    struct Payload
    {
        char name[RTP_PAYLOAD_NAME_SIZE];
        bool audio;
        PayloadUnion typeSpecific;
    };

    // Return a clock that reads the time as reported by the operating
    // system. The returned instances are guaranteed to read the same
    // times; in particular, they return relative times relative to
    // the same base.
    // Note that even though the instances returned by this function
    // read the same times a new object is created every time this
    // API is called. The ownership of this object belongs to the
    // caller.
    RtpRtcpClock* GetSystemClock();

    // Return the current RTP timestamp from the NTP timestamp
    // returned by the specified clock.
    WebRtc_UWord32 GetCurrentRTP(RtpRtcpClock* clock, WebRtc_UWord32 freq);

    // Return the current RTP absolute timestamp.
    WebRtc_UWord32 ConvertNTPTimeToRTP(WebRtc_UWord32 NTPsec,
                                       WebRtc_UWord32 NTPfrac,
                                       WebRtc_UWord32 freq);

    // Return the time in milliseconds corresponding to the specified
    // NTP timestamp.
    WebRtc_UWord32 ConvertNTPTimeToMS(WebRtc_UWord32 NTPsec,
                                      WebRtc_UWord32 NTPfrac);

    WebRtc_UWord32 pow2(WebRtc_UWord8 exp);

    // Returns true if |newTimestamp| is older than |existingTimestamp|.
    // |wrapped| will be set to true if there has been a wraparound between the
    // two timestamps.
    bool OldTimestamp(uint32_t newTimestamp,
                      uint32_t existingTimestamp,
                      bool* wrapped);

    bool StringCompare(const char* str1,
                       const char* str2,
                       const WebRtc_UWord32 length);

    void AssignUWord32ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value);
    void AssignUWord24ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value);
    void AssignUWord16ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord16 value);

    /**
     * Converts a network-ordered two-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered two-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord16 BufferToUWord16(const WebRtc_UWord8* dataBuffer);

    /**
     * Converts a network-ordered three-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered three-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord32 BufferToUWord24(const WebRtc_UWord8* dataBuffer);

    /**
     * Converts a network-ordered four-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered four-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord32 BufferToUWord32(const WebRtc_UWord8* dataBuffer);

    class RTPHeaderParser
    {
    public:
        RTPHeaderParser(const WebRtc_UWord8* rtpData,
                        const WebRtc_UWord32 rtpDataLength);
        ~RTPHeaderParser();

        bool RTCP() const;
        bool Parse(WebRtcRTPHeader& parsedPacket,
                   RtpHeaderExtensionMap* ptrExtensionMap = NULL) const;

    private:
        void ParseOneByteExtensionHeader(
            WebRtcRTPHeader& parsedPacket,
            const RtpHeaderExtensionMap* ptrExtensionMap,
            const WebRtc_UWord8* ptrRTPDataExtensionEnd,
            const WebRtc_UWord8* ptr) const;

        WebRtc_UWord8 ParsePaddingBytes(
            const WebRtc_UWord8* ptrRTPDataExtensionEnd,
            const WebRtc_UWord8* ptr) const;

        const WebRtc_UWord8* const _ptrRTPDataBegin;
        const WebRtc_UWord8* const _ptrRTPDataEnd;
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

        const WebRtc_UWord8*   data; 
        WebRtc_UWord16         dataLength;
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
                         const WebRtc_UWord8* payloadData,
                         const WebRtc_UWord16 payloadDataLength, // Length w/o padding.
                         const WebRtc_Word32 id);

        ~RTPPayloadParser();

        bool Parse(RTPPayload& parsedPacket) const;

    private:
        bool ParseGeneric(RTPPayload& parsedPacket) const;

        bool ParseVP8(RTPPayload& parsedPacket) const;

        int ParseVP8Extension(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 *dataPtr,
                              int dataLength) const;

        int ParseVP8PictureID(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8Tl0PicIdx(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8TIDAndKeyIdx(RTPPayloadVP8 *vp8,
                                 const WebRtc_UWord8 **dataPtr,
                                 int *dataLength,
                                 int *parsedBytes) const;

        int ParseVP8FrameSize(RTPPayload& parsedPacket,
                              const WebRtc_UWord8 *dataPtr,
                              int dataLength) const;

    private:
        WebRtc_Word32               _id;
        const WebRtc_UWord8*        _dataPtr;
        const WebRtc_UWord16        _dataLength;
        const RtpVideoCodecTypes    _videoType;
    };

}  // namespace ModuleRTPUtility

}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
