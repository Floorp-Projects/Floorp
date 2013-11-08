/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/generic_encoder.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer_common.h"

namespace webrtc {

VCMEncodedFrame::VCMEncodedFrame()
:
webrtc::EncodedImage(),
_renderTimeMs(-1),
_payloadType(0),
_missingFrame(false),
_codec(kVideoCodecUnknown),
_fragmentation()
{
    _codecSpecificInfo.codecType = kVideoCodecUnknown;
}

VCMEncodedFrame::VCMEncodedFrame(const webrtc::EncodedImage& rhs)
:
webrtc::EncodedImage(rhs),
_renderTimeMs(-1),
_payloadType(0),
_missingFrame(false),
_codec(kVideoCodecUnknown),
_fragmentation()
{
    _codecSpecificInfo.codecType = kVideoCodecUnknown;
    _buffer = NULL;
    _size = 0;
    _length = 0;
    if (rhs._buffer != NULL)
    {
        VerifyAndAllocate(rhs._length);
        memcpy(_buffer, rhs._buffer, rhs._length);
    }
}

VCMEncodedFrame::VCMEncodedFrame(const VCMEncodedFrame& rhs)
  :
    webrtc::EncodedImage(rhs),
    _renderTimeMs(rhs._renderTimeMs),
    _payloadType(rhs._payloadType),
    _missingFrame(rhs._missingFrame),
    _codecSpecificInfo(rhs._codecSpecificInfo),
    _codec(rhs._codec),
    _fragmentation() {
  _buffer = NULL;
  _size = 0;
  _length = 0;
  if (rhs._buffer != NULL)
  {
      VerifyAndAllocate(rhs._length);
      memcpy(_buffer, rhs._buffer, rhs._length);
      _length = rhs._length;
  }
  _fragmentation.CopyFrom(rhs._fragmentation);
}

VCMEncodedFrame::~VCMEncodedFrame()
{
    Free();
}

void VCMEncodedFrame::Free()
{
    Reset();
    if (_buffer != NULL)
    {
        delete [] _buffer;
        _buffer = NULL;
    }
}

void VCMEncodedFrame::Reset()
{
    _renderTimeMs = -1;
    _timeStamp = 0;
    _payloadType = 0;
    _frameType = kDeltaFrame;
    _encodedWidth = 0;
    _encodedHeight = 0;
    _completeFrame = false;
    _missingFrame = false;
    _length = 0;
    _codecSpecificInfo.codecType = kVideoCodecUnknown;
    _codec = kVideoCodecUnknown;
}

void VCMEncodedFrame::CopyCodecSpecific(const RTPVideoHeader* header)
{
    if (header)
    {
        switch (header->codec)
        {
            case kRtpVideoVp8:
            {
                if (_codecSpecificInfo.codecType != kVideoCodecVP8)
                {
                    // This is the first packet for this frame.
                    _codecSpecificInfo.codecSpecific.VP8.pictureId = -1;
                    _codecSpecificInfo.codecSpecific.VP8.temporalIdx = 0;
                    _codecSpecificInfo.codecSpecific.VP8.layerSync = false;
                    _codecSpecificInfo.codecSpecific.VP8.keyIdx = -1;
                    _codecSpecificInfo.codecType = kVideoCodecVP8;
                }
                _codecSpecificInfo.codecSpecific.VP8.nonReference =
                    header->codecHeader.VP8.nonReference;
                if (header->codecHeader.VP8.pictureId != kNoPictureId)
                {
                    _codecSpecificInfo.codecSpecific.VP8.pictureId =
                        header->codecHeader.VP8.pictureId;
                }
                if (header->codecHeader.VP8.temporalIdx != kNoTemporalIdx)
                {
                    _codecSpecificInfo.codecSpecific.VP8.temporalIdx =
                        header->codecHeader.VP8.temporalIdx;
                    _codecSpecificInfo.codecSpecific.VP8.layerSync =
                        header->codecHeader.VP8.layerSync;
                }
                if (header->codecHeader.VP8.keyIdx != kNoKeyIdx)
                {
                    _codecSpecificInfo.codecSpecific.VP8.keyIdx =
                        header->codecHeader.VP8.keyIdx;
                }
                break;
            }
            default:
            {
                _codecSpecificInfo.codecType = kVideoCodecUnknown;
                break;
            }
        }
    }
}

const RTPFragmentationHeader* VCMEncodedFrame::FragmentationHeader() const {
  return &_fragmentation;
}

int32_t
VCMEncodedFrame::VerifyAndAllocate(const uint32_t minimumSize)
{
    if(minimumSize > _size)
    {
        // create buffer of sufficient size
        uint8_t* newBuffer = new uint8_t[minimumSize];
        if (newBuffer == NULL)
        {
            return -1;
        }
        if(_buffer)
        {
            // copy old data
            memcpy(newBuffer, _buffer, _size);
            delete [] _buffer;
        }
        _buffer = newBuffer;
        _size = minimumSize;
    }
    return 0;
}

webrtc::FrameType VCMEncodedFrame::ConvertFrameType(VideoFrameType frameType)
{
    switch(frameType)
    {
    case kKeyFrame:
        {
            return  kVideoFrameKey;
        }
    case kDeltaFrame:
        {
            return kVideoFrameDelta;
        }
    case kGoldenFrame:
        {
            return kVideoFrameGolden;
        }
    case kAltRefFrame:
        {
            return kVideoFrameAltRef;
        }
    case kSkipFrame:
        {
            return kFrameEmpty;
        }
    default:
        {
            return kVideoFrameDelta;
        }
    }
}

VideoFrameType VCMEncodedFrame::ConvertFrameType(webrtc::FrameType frame_type) {
  switch (frame_type) {
    case kVideoFrameKey:
      return kKeyFrame;
    case kVideoFrameDelta:
      return kDeltaFrame;
    case kVideoFrameGolden:
      return kGoldenFrame;
    case kVideoFrameAltRef:
      return kAltRefFrame;
    default:
      assert(false);
      return kDeltaFrame;
  }
}

void VCMEncodedFrame::ConvertFrameTypes(
    const std::vector<webrtc::FrameType>& frame_types,
    std::vector<VideoFrameType>* video_frame_types) {
  assert(video_frame_types);
  video_frame_types->reserve(frame_types.size());
  for (size_t i = 0; i < frame_types.size(); ++i) {
    (*video_frame_types)[i] = ConvertFrameType(frame_types[i]);
  }
}

}
