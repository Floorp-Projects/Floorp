/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

#include <stdio.h>

namespace webrtc
{

namespace media_optimization {
class MediaOptimization;
}  // namespace media_optimization

/*************************************/
/* VCMEncodeFrameCallback class     */
/***********************************/
class VCMEncodedFrameCallback : public EncodedImageCallback
{
public:
    VCMEncodedFrameCallback();
    virtual ~VCMEncodedFrameCallback();

    /*
    * Callback implementation - codec encode complete
    */
    int32_t Encoded(
        EncodedImage& encodedImage,
        const CodecSpecificInfo* codecSpecificInfo = NULL,
        const RTPFragmentationHeader* fragmentationHeader = NULL);
    /*
    * Get number of encoded bytes
    */
    uint32_t EncodedBytes();
    /*
    * Callback implementation - generic encoder encode complete
    */
    int32_t SetTransportCallback(VCMPacketizationCallback* transport);
    /**
    * Set media Optimization
    */
    void SetMediaOpt (media_optimization::MediaOptimization* mediaOpt);

    void SetPayloadType(uint8_t payloadType) { _payloadType = payloadType; };
    void SetCodecType(VideoCodecType codecType) {_codecType = codecType;};
    void SetInternalSource(bool internalSource) { _internalSource = internalSource; };

private:
    /*
     * Map information from info into rtp. If no relevant information is found
     * in info, rtp is set to NULL.
     */
    static void CopyCodecSpecific(const CodecSpecificInfo& info,
                                  RTPVideoHeader** rtp);

    VCMPacketizationCallback* _sendCallback;
    media_optimization::MediaOptimization* _mediaOpt;
    uint32_t _encodedBytes;
    uint8_t _payloadType;
    VideoCodecType _codecType;
    bool _internalSource;
#ifdef DEBUG_ENCODER_BIT_STREAM
    FILE* _bitStreamAfterEncoder;
#endif
};// end of VCMEncodeFrameCallback class


/******************************/
/* VCMGenericEncoder class    */
/******************************/
class VCMGenericEncoder
{
    friend class VCMCodecDataBase;
public:
    VCMGenericEncoder(VideoEncoder& encoder, bool internalSource = false);
    ~VCMGenericEncoder();
    /**
    * Free encoder memory
    */
    int32_t Release();
    /**
    * Initialize the encoder with the information from the VideoCodec
    */
    int32_t InitEncode(const VideoCodec* settings,
                       int32_t numberOfCores,
                       uint32_t maxPayloadSize);
    /**
    * Encode raw image
    * inputFrame        : Frame containing raw image
    * codecSpecificInfo : Specific codec data
    * cameraFrameRate   : Request or information from the remote side
    * frameType         : The requested frame type to encode
    */
    int32_t Encode(const I420VideoFrame& inputFrame,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<FrameType>& frameTypes);
    /**
    * Set new target bitrate (bits/s) and framerate.
    * Return Value: new bit rate if OK, otherwise <0s.
    */
    int32_t SetRates(uint32_t target_bitrate, uint32_t frameRate);
    /**
    * Set a new packet loss rate and a new round-trip time in milliseconds.
    */
    int32_t SetChannelParameters(int32_t packetLoss, int rtt);
    int32_t CodecConfigParameters(uint8_t* buffer, int32_t size);
    /**
    * Register a transport callback which will be called to deliver the encoded
    * buffers
    */
    int32_t RegisterEncodeCallback(
        VCMEncodedFrameCallback* VCMencodedFrameCallback);
    /**
    * Get encoder bit rate
    */
    uint32_t BitRate() const;
     /**
    * Get encoder frame rate
    */
    uint32_t FrameRate() const;

    int32_t SetPeriodicKeyFrames(bool enable);

    int32_t RequestFrame(const std::vector<FrameType>& frame_types);

    bool InternalSource() const;

private:
    VideoEncoder&               _encoder;
    VideoCodecType              _codecType;
    VCMEncodedFrameCallback*    _VCMencodedFrameCallback;
    uint32_t                    _bitRate;
    uint32_t                    _frameRate;
    bool                        _internalSource;
}; // end of VCMGenericEncoder class

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
