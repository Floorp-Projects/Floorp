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

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"

namespace webrtc {
class VideoCoder : public VCMPacketizationCallback, public VCMReceiveCallback
{
public:
    VideoCoder();
    ~VideoCoder();

    int32_t SetEncodeCodec(VideoCodec& videoCodecInst,
                           uint32_t numberOfCores,
                           uint32_t maxPayloadSize);


    // Select the codec that should be used for decoding. videoCodecInst.plType
    // will be set to the codec's default payload type.
    int32_t SetDecodeCodec(VideoCodec& videoCodecInst, int32_t numberOfCores);

    int32_t Decode(I420VideoFrame& decodedVideo,
                   const EncodedVideoData& encodedData);

    int32_t Encode(const I420VideoFrame& videoFrame,
                   EncodedVideoData& videoEncodedData);

    int8_t DefaultPayloadType(const char* plName);

private:
    // VCMReceiveCallback function.
    // Note: called by VideoCodingModule when decoding finished.
    virtual int32_t FrameToRender(I420VideoFrame& videoFrame) OVERRIDE;

    // VCMPacketizationCallback function.
    // Note: called by VideoCodingModule when encoding finished.
    virtual int32_t SendData(
        FrameType /*frameType*/,
        uint8_t /*payloadType*/,
        uint32_t /*timeStamp*/,
        int64_t capture_time_ms,
        const uint8_t* payloadData,
        uint32_t payloadSize,
        const RTPFragmentationHeader& /* fragmentationHeader*/,
        const RTPVideoHeader* rtpTypeHdr) OVERRIDE;

    VideoCodingModule* _vcm;
    I420VideoFrame* _decodedVideo;
    EncodedVideoData* _videoEncodedData;
};
}  // namespace webrtc
#endif // WEBRTC_MODULE_UTILITY_VIDEO
#endif // WEBRTC_MODULES_UTILITY_SOURCE_VIDEO_CODER_H_
