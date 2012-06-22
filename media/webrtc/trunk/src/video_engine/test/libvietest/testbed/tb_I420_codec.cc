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
  return 0;
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
    const webrtc::RawImage& inputImage,
    const webrtc::CodecSpecificInfo* /*codecSpecificInfo*/,
    const webrtc::VideoFrameType /*frameType*/)
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
    _encodedImage._timeStamp = inputImage._timeStamp;
    _encodedImage._encodedHeight = inputImage._height;
    _encodedImage._encodedWidth = inputImage._width;
    if (inputImage._length > _encodedImage._size)
    {

        // allocating encoded memory
        if (_encodedImage._buffer != NULL)
        {
            delete[] _encodedImage._buffer;
            _encodedImage._buffer = NULL;
            _encodedImage._size = 0;
        }
        const WebRtc_UWord32 newSize = (3 * _encodedImage._encodedWidth
            * _encodedImage._encodedHeight) >> 1;
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
        if (newBuffer == NULL)
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        _encodedImage._size = newSize;
        _encodedImage._buffer = newBuffer;
    }
    assert(_encodedImage._size >= inputImage._length);
    memcpy(_encodedImage._buffer, inputImage._buffer, inputImage._length);
    _encodedImage._length = inputImage._length;
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

WebRtc_Word32 TbI420Encoder::SetPacketLoss(WebRtc_UWord32 packetLoss)
{
    _functionCalls.SetPacketLoss++;
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

    // Allocate memory for decoded image

    if (_decodedImage._buffer != NULL)
    {
        delete[] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
        _decodedImage._size = 0;
    }
    if (_decodedImage._buffer == NULL)
    {
        const WebRtc_UWord32 newSize = (3 * _width * _height) >> 1;
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
        if (newBuffer == NULL)
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        _decodedImage._size = newSize;
        _decodedImage._buffer = newBuffer;
    }

    // Set decoded image parameters
    _decodedImage._height = _height;
    _decodedImage._width = _width;
    _decodedImage._timeStamp = inputImage._timeStamp;
    assert(_decodedImage._size >= inputImage._length);
    memcpy(_decodedImage._buffer, inputImage._buffer, inputImage._length);
    _decodedImage._length = inputImage._length;
    //_decodedImage._buffer = inputImage._buffer;

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
    if (_decodedImage._buffer != NULL)
    {
        delete[] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
    }
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

TbI420Decoder::FunctionCalls TbI420Decoder::GetFunctionCalls()
{
    return _functionCalls;
}
