/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/libvietest/include/tb_I420_codec.h"

#include <assert.h>
#include <stdio.h>

#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"

TbI420Encoder::TbI420Encoder() :
    _inited(false), _encodedImage(), _encodedCompleteCallback(NULL)
{
    //
    memset(&_functionCalls, 0, sizeof(_functionCalls));
}

TbI420Encoder::~TbI420Encoder()
{
    _inited = false;
    if (_encodedImage._buffer != NULL)
    {
        delete[] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
    }
}

int32_t TbI420Encoder::Release()
{
    _functionCalls.Release++;
    // should allocate an encoded frame and then release it here, for that we
    // actaully need an init flag
    if (_encodedImage._buffer != NULL)
    {
        delete[] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
    }
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::SetChannelParameters(uint32_t packetLoss, int rtt) {
  _functionCalls.SetChannelParameters++;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::InitEncode(const webrtc::VideoCodec* inst,
                                  int32_t /*numberOfCores*/,
                                  uint32_t /*maxPayloadSize */)
{
    _functionCalls.InitEncode++;
    if (inst == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (inst->width < 1 || inst->height < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    // allocating encoded memory
    if (_encodedImage._buffer != NULL)
    {
        delete[] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
        _encodedImage._size = 0;
    }
    const uint32_t newSize = (3 * inst->width * inst->height) >> 1;
    uint8_t* newBuffer = new uint8_t[newSize];
    if (newBuffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    _encodedImage._size = newSize;
    _encodedImage._buffer = newBuffer;

    // if no memeory allocation, no point to init
    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::Encode(
    const webrtc::I420VideoFrame& inputImage,
    const webrtc::CodecSpecificInfo* /*codecSpecificInfo*/,
    const std::vector<webrtc::VideoFrameType>* /*frameTypes*/)
{
    _functionCalls.Encode++;
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (_encodedCompleteCallback == NULL)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    _encodedImage._frameType = webrtc::kKeyFrame; // no coding
    _encodedImage._timeStamp = inputImage.timestamp();
    _encodedImage._encodedHeight = inputImage.height();
    _encodedImage._encodedWidth = inputImage.width();
    unsigned int reqSize = webrtc::CalcBufferSize(webrtc::kI420,
                                                  _encodedImage._encodedWidth,
                                                  _encodedImage._encodedHeight);
    if (reqSize > _encodedImage._size)
    {

        // allocating encoded memory
        if (_encodedImage._buffer != NULL)
        {
            delete[] _encodedImage._buffer;
            _encodedImage._buffer = NULL;
            _encodedImage._size = 0;
        }
        uint8_t* newBuffer = new uint8_t[reqSize];
        if (newBuffer == NULL)
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        _encodedImage._size = reqSize;
        _encodedImage._buffer = newBuffer;
    }
    if (ExtractBuffer(inputImage, _encodedImage._size,
                      _encodedImage._buffer) < 0) {
      return -1;
    }

    _encodedImage._length = reqSize;
    _encodedCompleteCallback->Encoded(_encodedImage);
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback)
{
    _functionCalls.RegisterEncodeCompleteCallback++;
    _encodedCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::SetRates(uint32_t newBitRate, uint32_t frameRate)
{
    _functionCalls.SetRates++;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Encoder::SetPeriodicKeyFrames(bool enable)
{
    _functionCalls.SetPeriodicKeyFrames++;
    return WEBRTC_VIDEO_CODEC_ERROR;
}

int32_t TbI420Encoder::CodecConfigParameters(uint8_t* /*buffer*/,
                                             int32_t /*size*/)
{
    _functionCalls.CodecConfigParameters++;
    return WEBRTC_VIDEO_CODEC_ERROR;
}
TbI420Encoder::FunctionCalls TbI420Encoder::GetFunctionCalls()
{
    return _functionCalls;
}

TbI420Decoder::TbI420Decoder():
    _decodedImage(), _width(0), _height(0), _inited(false),
        _decodeCompleteCallback(NULL)
{
    memset(&_functionCalls, 0, sizeof(_functionCalls));
}

TbI420Decoder::~TbI420Decoder()
{
    Release();
}

int32_t TbI420Decoder::Reset()
{
    _functionCalls.Reset++;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Decoder::InitDecode(const webrtc::VideoCodec* inst,
                                  int32_t /*numberOfCores */)
{
    _functionCalls.InitDecode++;
    if (inst == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    else if (inst->width < 1 || inst->height < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    _width = inst->width;
    _height = inst->height;
    int half_width = (_width + 1 ) / 2 ;
    _decodedImage.CreateEmptyFrame(_width, _height,
                                   _width, half_width, half_width);
    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Decoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool /*missingFrames*/,
    const webrtc::RTPFragmentationHeader* /*fragmentation*/,
    const webrtc::CodecSpecificInfo* /*codecSpecificInfo*/,
    int64_t /*renderTimeMs*/)
{
    _functionCalls.Decode++;
    if (inputImage._buffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (_decodeCompleteCallback == NULL)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (inputImage._length <= 0)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    // Only send complete frames.
    if (static_cast<int>(inputImage._length) !=
        webrtc::CalcBufferSize(webrtc::kI420,_width,_height)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    int ret = ConvertToI420(webrtc::kI420, inputImage._buffer, 0, 0,
                           _width, _height,
                           0, webrtc::kRotateNone, &_decodedImage);

    if (ret < 0)
      return WEBRTC_VIDEO_CODEC_ERROR;

    _decodedImage.set_timestamp(inputImage._timeStamp);

    _decodeCompleteCallback->Decoded(_decodedImage);
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Decoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback)
{
    _functionCalls.RegisterDecodeCompleteCallback++;
    _decodeCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t TbI420Decoder::Release()
{
    _functionCalls.Release++;
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

TbI420Decoder::FunctionCalls TbI420Decoder::GetFunctionCalls()
{
    return _functionCalls;
}
