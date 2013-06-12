/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/libvietest/include/tb_I420_codec.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"

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

WebRtc_Word32 TbI420Encoder::VersionStatic(char* version,
                                           WebRtc_Word32 length)
{
    const char* str = "I420 version 1.0.0\n";
    WebRtc_Word32 verLen = (WebRtc_Word32) strlen(str);
    if (verLen > length)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    strncpy(version, str, length);
    return verLen;
}

WebRtc_Word32 TbI420Encoder::Version(char* version,
                                     WebRtc_Word32 length) const
{
    return VersionStatic(version, length);
}

WebRtc_Word32 TbI420Encoder::Release()
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

WebRtc_Word32 TbI420Encoder::Reset()
{
    _functionCalls.Reset++;
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    return WEBRTC_VIDEO_CODEC_OK;

}

WebRtc_Word32 TbI420Encoder::SetChannelParameters(WebRtc_UWord32 packetLoss,
                                                  int rtt) {
  _functionCalls.SetChannelParameters++;
  return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Encoder::InitEncode(const webrtc::VideoCodec* inst,
                                        WebRtc_Word32 /*numberOfCores*/,
                                        WebRtc_UWord32 /*maxPayloadSize */)
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
    const WebRtc_UWord32 newSize = (3 * inst->width * inst->height) >> 1;
    WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
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

WebRtc_Word32 TbI420Encoder::Encode(
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
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[reqSize];
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

WebRtc_Word32 TbI420Encoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback)
{
    _functionCalls.RegisterEncodeCompleteCallback++;
    _encodedCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Encoder::SetRates(WebRtc_UWord32 newBitRate,
                                      WebRtc_UWord32 frameRate)
{
    _functionCalls.SetRates++;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Encoder::SetPeriodicKeyFrames(bool enable)
{
    _functionCalls.SetPeriodicKeyFrames++;
    return WEBRTC_VIDEO_CODEC_ERROR;
}

WebRtc_Word32 TbI420Encoder::CodecConfigParameters(WebRtc_UWord8* /*buffer*/,
                                                   WebRtc_Word32 /*size*/)
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

WebRtc_Word32 TbI420Decoder::Reset()
{
    _functionCalls.Reset++;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Decoder::InitDecode(const webrtc::VideoCodec* inst,
                                        WebRtc_Word32 /*numberOfCores */)
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
    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Decoder::Decode(
    const webrtc::EncodedImage& inputImage,
    bool /*missingFrames*/,
    const webrtc::RTPFragmentationHeader* /*fragmentation*/,
    const webrtc::CodecSpecificInfo* /*codecSpecificInfo*/,
    WebRtc_Word64 /*renderTimeMs*/)
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

    int size_y = _width * _height;
    int size_uv = ((_width + 1 ) / 2) * ((_height + 1) / 2);
    int ret = _decodedImage.CreateFrame(size_y, inputImage._buffer,
                                        size_uv, inputImage._buffer + size_y,
                                        size_uv, inputImage._buffer + size_y +
                                        size_uv,
                                        _width, _height,
                                        _width, (_width + 1 ) / 2,
                                        (_width + 1 ) / 2);
    if (ret < 0)
      return WEBRTC_VIDEO_CODEC_ERROR;
    _decodedImage.set_timestamp(inputImage._timeStamp);

    _decodeCompleteCallback->Decoded(_decodedImage);
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Decoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback)
{
    _functionCalls.RegisterDecodeCompleteCallback++;
    _decodeCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32 TbI420Decoder::Release()
{
    _functionCalls.Release++;
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

TbI420Decoder::FunctionCalls TbI420Decoder::GetFunctionCalls()
{
    return _functionCalls;
}
