/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_ENCODED_FRAME_H_
#define WEBRTC_MODULES_VIDEO_CODING_ENCODED_FRAME_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"

namespace webrtc
{

class VCMEncodedFrame : protected EncodedImage
{
public:
    VCMEncodedFrame();
    VCMEncodedFrame(const webrtc::EncodedImage& rhs);
    VCMEncodedFrame(const VCMEncodedFrame& rhs);

    ~VCMEncodedFrame();
    /**
    *   Delete VideoFrame and resets members to zero
    */
    void Free();
    /**
    *   Set render time in milliseconds
    */
    void SetRenderTime(const int64_t renderTimeMs) {_renderTimeMs = renderTimeMs;}

    /**
    *   Set the encoded frame size
    */
    void SetEncodedSize(uint32_t width, uint32_t height)
                       { _encodedWidth  = width; _encodedHeight = height; }
    /**
    *   Get the encoded image
    */
    const webrtc::EncodedImage& EncodedImage() const
                       { return static_cast<const webrtc::EncodedImage&>(*this); }
    /**
    *   Get pointer to frame buffer
    */
    const uint8_t* Buffer() const {return _buffer;}
    /**
    *   Get frame length
    */
    uint32_t Length() const {return _length;}
    /**
    *   Get frame timestamp (90kHz)
    */
    uint32_t TimeStamp() const {return _timeStamp;}
    /**
    *   Get render time in milliseconds
    */
    int64_t RenderTimeMs() const {return _renderTimeMs;}
    /**
    *   Get frame type
    */
    webrtc::FrameType FrameType() const {return ConvertFrameType(_frameType);}
    /**
    *   True if this frame is complete, false otherwise
    */
    bool Complete() const { return _completeFrame; }
    /**
    *   True if there's a frame missing before this frame
    */
    bool MissingFrame() const { return _missingFrame; }
    /**
    *   Payload type of the encoded payload
    */
    uint8_t PayloadType() const { return _payloadType; }
    /**
    *   Get codec specific info.
    *   The returned pointer is only valid as long as the VCMEncodedFrame
    *   is valid. Also, VCMEncodedFrame owns the pointer and will delete
    *   the object.
    */
    const CodecSpecificInfo* CodecSpecific() const {return &_codecSpecificInfo;}

    const RTPFragmentationHeader* FragmentationHeader() const;

    static webrtc::FrameType ConvertFrameType(VideoFrameType frameType);
    static VideoFrameType ConvertFrameType(webrtc::FrameType frameType);
    static void ConvertFrameTypes(
        const std::vector<webrtc::FrameType>& frame_types,
        std::vector<VideoFrameType>* video_frame_types);

protected:
    /**
    * Verifies that current allocated buffer size is larger than or equal to the input size.
    * If the current buffer size is smaller, a new allocation is made and the old buffer data
    * is copied to the new buffer.
    * Buffer size is updated to minimumSize.
    */
    void VerifyAndAllocate(const uint32_t minimumSize);

    void Reset();

    void CopyCodecSpecific(const RTPVideoHeader* header);

    int64_t                 _renderTimeMs;
    uint8_t                 _payloadType;
    bool                          _missingFrame;
    CodecSpecificInfo             _codecSpecificInfo;
    webrtc::VideoCodecType        _codec;
    RTPFragmentationHeader        _fragmentation;
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_ENCODED_FRAME_H_
