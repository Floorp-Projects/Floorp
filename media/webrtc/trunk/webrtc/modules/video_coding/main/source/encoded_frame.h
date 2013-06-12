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

#include "common_types.h"
#include "common_video/interface/video_image.h"
#include "modules/interface/module_common_types.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"

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
    void SetRenderTime(const WebRtc_Word64 renderTimeMs) {_renderTimeMs = renderTimeMs;}

    /**
    *   Set the encoded frame size
    */
    void SetEncodedSize(WebRtc_UWord32 width, WebRtc_UWord32 height)
                       { _encodedWidth  = width; _encodedHeight = height; }
    /**
    *   Get the encoded image
    */
    const webrtc::EncodedImage& EncodedImage() const
                       { return static_cast<const webrtc::EncodedImage&>(*this); }
    /**
    *   Get pointer to frame buffer
    */
    const WebRtc_UWord8* Buffer() const {return _buffer;}
    /**
    *   Get frame length
    */
    WebRtc_UWord32 Length() const {return _length;}
    /**
    *   Get frame timestamp (90kHz)
    */
    WebRtc_UWord32 TimeStamp() const {return _timeStamp;}
    /**
    *   Get render time in milliseconds
    */
    WebRtc_Word64 RenderTimeMs() const {return _renderTimeMs;}
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
    WebRtc_UWord8 PayloadType() const { return _payloadType; }
    /**
    *   Get codec specific info.
    *   The returned pointer is only valid as long as the VCMEncodedFrame
    *   is valid. Also, VCMEncodedFrame owns the pointer and will delete
    *   the object.
    */
    const CodecSpecificInfo* CodecSpecific() const {return &_codecSpecificInfo;}

    const RTPFragmentationHeader* FragmentationHeader() const;

    WebRtc_Word32 Store(VCMFrameStorageCallback& storeCallback) const;

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
    WebRtc_Word32 VerifyAndAllocate(const WebRtc_UWord32 minimumSize);

    void Reset();

    void CopyCodecSpecific(const RTPVideoHeader* header);

    WebRtc_Word64                 _renderTimeMs;
    WebRtc_UWord8                 _payloadType;
    bool                          _missingFrame;
    CodecSpecificInfo             _codecSpecificInfo;
    webrtc::VideoCodecType        _codec;
    RTPFragmentationHeader        _fragmentation;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_ENCODED_FRAME_H_
