/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_VIDEO_CODER_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_VIDEO_CODER_H_

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "engine_configurations.h"
#include "video_coding.h"

namespace webrtc {
class VideoCoder : public VCMPacketizationCallback, public VCMReceiveCallback
{
public:
    VideoCoder(WebRtc_UWord32 instanceID);
    ~VideoCoder();

    WebRtc_Word32 ResetDecoder();

    WebRtc_Word32 SetEncodeCodec(VideoCodec& videoCodecInst,
                                 WebRtc_UWord32 numberOfCores,
                                 WebRtc_UWord32 maxPayloadSize);


    // Select the codec that should be used for decoding. videoCodecInst.plType
    // will be set to the codec's default payload type.
    WebRtc_Word32 SetDecodeCodec(VideoCodec& videoCodecInst,
                                 WebRtc_Word32 numberOfCores);

    WebRtc_Word32 Decode(I420VideoFrame& decodedVideo,
                         const EncodedVideoData& encodedData);

    WebRtc_Word32 Encode(const I420VideoFrame& videoFrame,
                         EncodedVideoData& videoEncodedData);

    WebRtc_Word8 DefaultPayloadType(const char* plName);

private:
    // VCMReceiveCallback function.
    // Note: called by VideoCodingModule when decoding finished.
    WebRtc_Word32 FrameToRender(I420VideoFrame& videoFrame);

    // VCMPacketizationCallback function.
    // Note: called by VideoCodingModule when encoding finished.
    WebRtc_Word32 SendData(
        FrameType /*frameType*/,
        WebRtc_UWord8 /*payloadType*/,
        WebRtc_UWord32 /*timeStamp*/,
        int64_t capture_time_ms,
        const WebRtc_UWord8* payloadData,
        WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& /* fragmentationHeader*/,
        const RTPVideoHeader* rtpTypeHdr);

    VideoCodingModule* _vcm;
    I420VideoFrame* _decodedVideo;
    EncodedVideoData* _videoEncodedData;
};
} // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
#endif // WEBRTC_MODULES_UTILITY_SOURCE_VIDEO_CODER_H_
