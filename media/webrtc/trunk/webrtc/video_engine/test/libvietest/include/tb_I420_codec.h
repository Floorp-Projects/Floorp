/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the interface to I420 "codec"
 * This is a dummy wrapper to allow VCM deal with raw I420 sequences
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

class TbI420Encoder: public webrtc::VideoEncoder
{
public:
    TbI420Encoder();
    virtual ~TbI420Encoder();

    virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                               int32_t numberOfCores,
                               uint32_t maxPayloadSize) OVERRIDE;

    virtual int32_t Encode(
        const webrtc::I420VideoFrame& inputImage,
        const webrtc::CodecSpecificInfo* codecSpecificInfo,
        const std::vector<webrtc::VideoFrameType>* frameTypes) OVERRIDE;

    virtual int32_t RegisterEncodeCompleteCallback(
        webrtc::EncodedImageCallback* callback) OVERRIDE;

    virtual int32_t Release() OVERRIDE;

    virtual int32_t SetChannelParameters(uint32_t packetLoss, int rtt) OVERRIDE;

    virtual int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) OVERRIDE;

    virtual int32_t SetPeriodicKeyFrames(bool enable) OVERRIDE;

    virtual int32_t CodecConfigParameters(uint8_t* /*buffer*/,
                                          int32_t /*size*/) OVERRIDE;

    struct FunctionCalls
    {
        int32_t InitEncode;
        int32_t Encode;
        int32_t RegisterEncodeCompleteCallback;
        int32_t Release;
        int32_t Reset;
        int32_t SetChannelParameters;
        int32_t SetRates;
        int32_t SetPeriodicKeyFrames;
        int32_t CodecConfigParameters;

    };

    FunctionCalls GetFunctionCalls();
private:
    bool _inited;
    webrtc::EncodedImage _encodedImage;
    FunctionCalls _functionCalls;
    webrtc::EncodedImageCallback* _encodedCompleteCallback;

}; // end of tbI420Encoder class


/***************************/
/* tbI420Decoder class */
/***************************/

class TbI420Decoder: public webrtc::VideoDecoder
{
public:
    TbI420Decoder();
    virtual ~TbI420Decoder();

    virtual int32_t InitDecode(const webrtc::VideoCodec* inst,
                               int32_t numberOfCores) OVERRIDE;
    virtual int32_t Decode(
        const webrtc::EncodedImage& inputImage,
        bool missingFrames,
        const webrtc::RTPFragmentationHeader* fragmentation,
        const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
        int64_t renderTimeMs = -1) OVERRIDE;

    virtual int32_t RegisterDecodeCompleteCallback(
        webrtc::DecodedImageCallback* callback) OVERRIDE;
    virtual int32_t Release() OVERRIDE;
    virtual int32_t Reset() OVERRIDE;

    struct FunctionCalls
    {
        int32_t InitDecode;
        int32_t Decode;
        int32_t RegisterDecodeCompleteCallback;
        int32_t Release;
        int32_t Reset;
    };

    FunctionCalls GetFunctionCalls();

private:

    webrtc::I420VideoFrame _decodedImage;
    int32_t _width;
    int32_t _height;
    bool _inited;
    FunctionCalls _functionCalls;
    webrtc::DecodedImageCallback* _decodeCompleteCallback;

}; // end of tbI420Decoder class

#endif // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_I420_CODEC_H_
