/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/i420/main/interface/i420.h"

#include <string.h>

#include "common_video/libyuv/include/webrtc_libyuv.h"


namespace webrtc
{

I420Encoder::I420Encoder():
_inited(false),
_encodedImage(),
_encodedCompleteCallback(NULL)
{}

I420Encoder::~I420Encoder() {
  _inited = false;
  if (_encodedImage._buffer != NULL) {
    delete [] _encodedImage._buffer;
    _encodedImage._buffer = NULL;
  }
}

int I420Encoder::Release() {
  // Should allocate an encoded frame and then release it here, for that we
  // actually need an init flag.
  if (_encodedImage._buffer != NULL) {
    delete [] _encodedImage._buffer;
    _encodedImage._buffer = NULL;
  }
  _inited = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

int I420Encoder::InitEncode(const VideoCodec* codecSettings,
                            int /*numberOfCores*/,
                            uint32_t /*maxPayloadSize */) {
  if (codecSettings == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (codecSettings->width < 1 || codecSettings->height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Allocating encoded memory.
  if (_encodedImage._buffer != NULL) {
    delete [] _encodedImage._buffer;
    _encodedImage._buffer = NULL;
    _encodedImage._size = 0;
  }
  const uint32_t newSize = CalcBufferSize(kI420,
                                          codecSettings->width,
                                          codecSettings->height);
  uint8_t* newBuffer = new uint8_t[newSize];
  if (newBuffer == NULL) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }
  _encodedImage._size = newSize;
  _encodedImage._buffer = newBuffer;

  // If no memory allocation, no point to init.
  _inited = true;
  return WEBRTC_VIDEO_CODEC_OK;
}



int I420Encoder::Encode(const VideoFrame& inputImage,
                    const CodecSpecificInfo* /*codecSpecificInfo*/,
                    const VideoFrameType /*frameType*/) {
  if (!_inited) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (_encodedCompleteCallback == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  _encodedImage._frameType = kKeyFrame; // No coding.
  _encodedImage._timeStamp = inputImage.TimeStamp();
  _encodedImage._encodedHeight = inputImage.Height();
  _encodedImage._encodedWidth = inputImage.Width();
  if (inputImage.Length() > _encodedImage._size) {

    // Allocating encoded memory.
    if (_encodedImage._buffer != NULL) {
      delete [] _encodedImage._buffer;
      _encodedImage._buffer = NULL;
      _encodedImage._size = 0;
    }
    const uint32_t newSize = CalcBufferSize(kI420,
                                            _encodedImage._encodedWidth,
                                            _encodedImage._encodedHeight);
    uint8_t* newBuffer = new uint8_t[newSize];
    if (newBuffer == NULL) {
      return WEBRTC_VIDEO_CODEC_MEMORY;
    }
    _encodedImage._size = newSize;
    _encodedImage._buffer = newBuffer;
  }
  memcpy(_encodedImage._buffer, inputImage.Buffer(), inputImage.Length());
  _encodedImage._length = inputImage.Length();
  _encodedCompleteCallback->Encoded(_encodedImage);
  return WEBRTC_VIDEO_CODEC_OK;
}


int
I420Encoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback) {
  _encodedCompleteCallback = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}


I420Decoder::I420Decoder():
_decodedImage(),
_width(0),
_height(0),
_inited(false),
_decodeCompleteCallback(NULL)
{}

I420Decoder::~I420Decoder() {
  Release();
}

int
I420Decoder::Reset() {
  return WEBRTC_VIDEO_CODEC_OK;
}


int
I420Decoder::InitDecode(const VideoCodec* codecSettings,
                        int /*numberOfCores */) {
  if (codecSettings == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  } else if (codecSettings->width < 1 || codecSettings->height < 1) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  _width = codecSettings->width;
  _height = codecSettings->height;
  _inited = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

int
I420Decoder::Decode(const EncodedImage& inputImage,
                    bool /*missingFrames*/,
                    const RTPFragmentationHeader* /*fragmentation*/,
                    const CodecSpecificInfo* /*codecSpecificInfo*/,
                    int64_t /*renderTimeMs*/) {
  if (inputImage._buffer == NULL) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (_decodeCompleteCallback == NULL) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (inputImage._length <= 0) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }
  if (!_inited) {
   return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  // Set decoded image parameters.
  if (_decodedImage.CopyFrame(inputImage._length, inputImage._buffer) < 0) {
    return WEBRTC_VIDEO_CODEC_MEMORY;
  }
  _decodedImage.SetHeight(_height);
  _decodedImage.SetWidth(_width);
  _decodedImage.SetTimeStamp(inputImage._timeStamp);

  _decodeCompleteCallback->Decoded(_decodedImage);
  return WEBRTC_VIDEO_CODEC_OK;
}

int
I420Decoder::RegisterDecodeCompleteCallback(DecodedImageCallback* callback) {
  _decodeCompleteCallback = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int
I420Decoder::Release() {
  _decodedImage.Free();
  _inited = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

}
