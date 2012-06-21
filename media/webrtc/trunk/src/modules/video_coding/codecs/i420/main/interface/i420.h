/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_I420_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_I420_H_

#include "video_codec_interface.h"
#include "typedefs.h"

namespace webrtc
{

class I420Encoder : public VideoEncoder
{
public:

    I420Encoder();

    virtual ~I420Encoder();

// Initialize the encoder with the information from the VideoCodec
//
// Input:
//          - codecSettings     : Codec settings
//          - numberOfCores     : Number of cores available for the encoder
//          - maxPayloadSize    : The maximum size each payload is allowed
//                                to have. Usually MTU - overhead.
//
// Return value                 : WEBRTC_VIDEO_CODEC_OK if OK
//                                <0 - Error
    virtual WebRtc_Word32 InitEncode(const VideoCodec* codecSettings, WebRtc_Word32 /*numberOfCores*/, WebRtc_UWord32 /*maxPayloadSize*/);

// "Encode" an I420 image (as a part of a video stream). The encoded image
// will be returned to the user via the encode complete callback.
//
// Input:
//          - inputImage        : Image to be encoded
//          - codecSpecificInfo : Pointer to codec specific data
//          - frameType         : Frame type to be sent (Key /Delta) .
//
// Return value                 : WEBRTC_VIDEO_CODEC_OK if OK
//                                <0 - Error
    virtual WebRtc_Word32
        Encode(const RawImage& inputImage,
               const CodecSpecificInfo* /*codecSpecificInfo*/,
               const VideoFrameType /*frameTypes*/);

// Register an encode complete callback object.
//
// Input:
//          - callback         : Callback object which handles encoded images.
//
// Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 RegisterEncodeCompleteCallback(EncodedImageCallback* callback);

// Free encoder memory.
//
// Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 Release();

    virtual WebRtc_Word32 SetRates(WebRtc_UWord32 /*newBitRate*/,
                                   WebRtc_UWord32 /*frameRate*/)
    {return WEBRTC_VIDEO_CODEC_OK;}

    virtual WebRtc_Word32 SetChannelParameters(WebRtc_UWord32 /*packetLoss*/,
                                               int /*rtt*/)
    {return WEBRTC_VIDEO_CODEC_OK;}

    virtual WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* /*buffer*/,
                                                WebRtc_Word32 /*size*/)
    {return WEBRTC_VIDEO_CODEC_OK;}

private:
    bool                     _inited;
    EncodedImage             _encodedImage;
    EncodedImageCallback*    _encodedCompleteCallback;

}; // end of WebRtcI420DEncoder class

class I420Decoder : public VideoDecoder
{
public:

    I420Decoder();

    virtual ~I420Decoder();

// Initialize the decoder.
// The user must notify the codec of width and height values.
//
// Return value         :  WEBRTC_VIDEO_CODEC_OK.
//                        <0 - Errors
    virtual WebRtc_Word32 InitDecode(const VideoCodec* codecSettings, WebRtc_Word32 /*numberOfCores*/);

    virtual WebRtc_Word32 SetCodecConfigParameters(const WebRtc_UWord8* /*buffer*/, WebRtc_Word32 /*size*/){return WEBRTC_VIDEO_CODEC_OK;};

// Decode encoded image (as a part of a video stream). The decoded image
// will be returned to the user through the decode complete callback.
//
// Input:
//          - inputImage        : Encoded image to be decoded
//          - missingFrames     : True if one or more frames have been lost
//                                since the previous decode call.
//          - codecSpecificInfo : pointer to specific codec data
//          - renderTimeMs      : Render time in Ms
//
// Return value                 : WEBRTC_VIDEO_CODEC_OK if OK
//                                 <0 - Error
    virtual WebRtc_Word32 Decode(
        const EncodedImage& inputImage,
        bool missingFrames,
        const RTPFragmentationHeader* /*fragmentation*/,
        const CodecSpecificInfo* /*codecSpecificInfo*/,
        WebRtc_Word64 /*renderTimeMs*/);

// Register a decode complete callback object.
//
// Input:
//          - callback         : Callback object which handles decoded images.
//
// Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 RegisterDecodeCompleteCallback(DecodedImageCallback* callback);

// Free decoder memory.
//
// Return value                : WEBRTC_VIDEO_CODEC_OK if OK
//                                  <0 - Error
    virtual WebRtc_Word32 Release();

// Reset decoder state and prepare for a new call.
//
// Return value         :  WEBRTC_VIDEO_CODEC_OK.
//                          <0 - Error
    virtual WebRtc_Word32 Reset();

private:

    RawImage                    _decodedImage;
    WebRtc_Word32               _width;
    WebRtc_Word32               _height;
    bool                        _inited;
    DecodedImageCallback*       _decodeCompleteCallback;


}; // end of WebRtcI420Decoder class

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_I420_H_
