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

#include "video_codec_interface.h"

#include <stdio.h>

namespace webrtc
{

class VCMMediaOptimization;

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
    WebRtc_Word32 Encoded(
        EncodedImage& encodedImage,
        const CodecSpecificInfo* codecSpecificInfo = NULL,
        const RTPFragmentationHeader* fragmentationHeader = NULL);
    /*
    * Get number of encoded bytes
    */
    WebRtc_UWord32 EncodedBytes();
    /*
    * Callback implementation - generic encoder encode complete
    */
    WebRtc_Word32 SetTransportCallback(VCMPacketizationCallback* transport);
    /**
    * Set media Optimization
    */
    void SetMediaOpt (VCMMediaOptimization* mediaOpt);

    void SetPayloadType(WebRtc_UWord8 payloadType) { _payloadType = payloadType; };
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
    VCMMediaOptimization*     _mediaOpt;
    WebRtc_UWord32            _encodedBytes;
    WebRtc_UWord8             _payloadType;
    VideoCodecType            _codecType;
    bool                      _internalSource;
#ifdef DEBUG_ENCODER_BIT_STREAM
    FILE*                     _bitStreamAfterEncoder;
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
    *	Free encoder memory
    */
    WebRtc_Word32 Release();
    /**
    *	Initialize the encoder with the information from the VideoCodec
    */
    WebRtc_Word32 InitEncode(const VideoCodec* settings,
                             WebRtc_Word32 numberOfCores,
                             WebRtc_UWord32 maxPayloadSize);
    /**
    *	Encode raw image
    *	inputFrame        : Frame containing raw image
    *	codecSpecificInfo : Specific codec data
    *	cameraFrameRate	  :	request or information from the remote side
    *	frameType         : The requested frame type to encode
    */
    WebRtc_Word32 Encode(const I420VideoFrame& inputFrame,
                         const CodecSpecificInfo* codecSpecificInfo,
                         const std::vector<FrameType>& frameTypes);
    /**
    *	Set new target bit rate and frame rate
    * Return Value: new bit rate if OK, otherwise <0s
    */
    WebRtc_Word32 SetRates(WebRtc_UWord32 newBitRate, WebRtc_UWord32 frameRate);
    /**
    * Set a new packet loss rate and a new round-trip time in milliseconds.
    */
    WebRtc_Word32 SetChannelParameters(WebRtc_Word32 packetLoss, int rtt);
    WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* buffer, WebRtc_Word32 size);
    /**
    * Register a transport callback which will be called to deliver the encoded buffers
    */
    WebRtc_Word32 RegisterEncodeCallback(VCMEncodedFrameCallback* VCMencodedFrameCallback);
    /**
    * Get encoder bit rate
    */
    WebRtc_UWord32 BitRate() const;
     /**
    * Get encoder frame rate
    */
    WebRtc_UWord32 FrameRate() const;

    WebRtc_Word32 SetPeriodicKeyFrames(bool enable);

    WebRtc_Word32 RequestFrame(const std::vector<FrameType>& frame_types);

    bool InternalSource() const;

private:
    VideoEncoder&               _encoder;
    VideoCodecType              _codecType;
    VCMEncodedFrameCallback*    _VCMencodedFrameCallback;
    WebRtc_UWord32              _bitRate;
    WebRtc_UWord32              _frameRate;
    bool                        _internalSource;
}; // end of VCMGenericEncoder class

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
