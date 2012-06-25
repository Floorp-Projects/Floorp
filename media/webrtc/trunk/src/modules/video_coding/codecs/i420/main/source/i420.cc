/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "i420.h"
#include <string.h>

namespace webrtc
{

I420Encoder::I420Encoder():
_inited(false),
_encodedImage(),
_encodedCompleteCallback(NULL)
{
     //
}

I420Encoder::~I420Encoder()
{
    _inited = false;
    if (_encodedImage._buffer != NULL)
    {
        delete [] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
    }
}

WebRtc_Word32
I420Encoder::Release()
{
    // should allocate an encoded frame and then release it here, for that we actaully need an init flag
    if (_encodedImage._buffer != NULL)
    {
        delete [] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
    }
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
I420Encoder::InitEncode(const VideoCodec* codecSettings,
                              WebRtc_Word32 /*numberOfCores*/,
                              WebRtc_UWord32 /*maxPayloadSize */)
{
    if (codecSettings == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    if (codecSettings->width < 1 || codecSettings->height < 1)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }

    // allocating encoded memory

    if (_encodedImage._buffer != NULL)
    {
        delete [] _encodedImage._buffer;
        _encodedImage._buffer = NULL;
        _encodedImage._size = 0;
    }
    const WebRtc_UWord32 newSize = (3 * codecSettings->width *
                                      codecSettings->height) >> 1;
    WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
    if (newBuffer == NULL)
    {
        return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    _encodedImage._size = newSize;
    _encodedImage._buffer = newBuffer;

    // if no memory allocation, no point to init
    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}



WebRtc_Word32
I420Encoder::Encode(const RawImage& inputImage,
                    const CodecSpecificInfo* /*codecSpecificInfo*/,
                    const VideoFrameType /*frameType*/)
{
    if (!_inited)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }
    if (_encodedCompleteCallback == NULL)
    {
        return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    }

    _encodedImage._frameType = kKeyFrame; // no coding
    _encodedImage._timeStamp = inputImage._timeStamp;
    _encodedImage._encodedHeight = inputImage._height;
    _encodedImage._encodedWidth = inputImage._width;
    if (inputImage._length > _encodedImage._size)
    {

        // allocating encoded memory
        if (_encodedImage._buffer != NULL)
        {
            delete [] _encodedImage._buffer;
            _encodedImage._buffer = NULL;
            _encodedImage._size = 0;
        }
        const WebRtc_UWord32 newSize = (3 * _encodedImage._encodedWidth * _encodedImage._encodedHeight) >> 1;
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
        if (newBuffer == NULL)
        {
            return WEBRTC_VIDEO_CODEC_MEMORY;
        }
        _encodedImage._size = newSize;
        _encodedImage._buffer = newBuffer;
    }
    memcpy(_encodedImage._buffer, inputImage._buffer, inputImage._length);
    _encodedImage._length = inputImage._length;
    _encodedCompleteCallback->Encoded(_encodedImage);
    return WEBRTC_VIDEO_CODEC_OK;
}


WebRtc_Word32
I420Encoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
{
    _encodedCompleteCallback = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}


I420Decoder::I420Decoder():
_decodedImage(),
_width(0),
_height(0),
_inited(false),
_decodeCompleteCallback(NULL)
{
    //
}

I420Decoder::~I420Decoder()
{
    Release();
}

WebRtc_Word32
I420Decoder::Reset()
{
    return WEBRTC_VIDEO_CODEC_OK;
}


WebRtc_Word32
I420Decoder::InitDecode(const VideoCodec* codecSettings, WebRtc_Word32 /*numberOfCores */)
{
    if (codecSettings == NULL)
    {
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    else if (codecSettings->width < 1 || codecSettings->height < 1)
    {
         return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
    }
    _width = codecSettings->width;
    _height = codecSettings->height;
    _inited = true;
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
I420Decoder::Decode(const EncodedImage& inputImage,
                    bool /*missingFrames*/,
                    const RTPFragmentationHeader* /*fragmentation*/,
                    const CodecSpecificInfo* /*codecSpecificInfo*/,
                    WebRtc_Word64 /*renderTimeMs*/)
{
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

    //Allocate memory for decoded image

    if (_decodedImage._buffer != NULL)
    {
        delete [] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
        _decodedImage._size = 0;
    }
    if (_decodedImage._buffer == NULL)
    {
        const WebRtc_UWord32 newSize = (3*_width*_height) >> 1;
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
    memcpy(_decodedImage._buffer, inputImage._buffer, inputImage._length);
    _decodedImage._length = inputImage._length;
    //_decodedImage._buffer = inputImage._buffer;

    _decodeCompleteCallback->Decoded(_decodedImage);
    return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
I420Decoder::RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
{
    _decodeCompleteCallback = callback;
        return WEBRTC_VIDEO_CODEC_OK;
}

WebRtc_Word32
I420Decoder::Release()
{
    if (_decodedImage._buffer != NULL)
    {
        delete [] _decodedImage._buffer;
        _decodedImage._buffer = NULL;
    }
    _inited = false;
    return WEBRTC_VIDEO_CODEC_OK;
}

}
