/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H

#include "common_types.h"
#include "modules/interface/module_common_types.h"
#include "modules/video_coding/codecs/interface/video_error_codes.h"
#include "common_video/interface/video_image.h"
#include "typedefs.h"

namespace webrtc
{

class RTPFragmentationHeader; // forward declaration

// Note: if any pointers are added to this struct, it must be fitted
// with a copy-constructor. See below.
struct CodecSpecificInfoVP8
{
    bool             hasReceivedSLI;
    WebRtc_UWord8    pictureIdSLI;
    bool             hasReceivedRPSI;
    WebRtc_UWord64   pictureIdRPSI;
    WebRtc_Word16    pictureId;         // negative value to skip pictureId
    bool             nonReference;
    WebRtc_UWord8    simulcastIdx;
    WebRtc_UWord8    temporalIdx;
    bool             layerSync;
    int              tl0PicIdx;         // Negative value to skip tl0PicIdx
    WebRtc_Word8     keyIdx;            // negative value to skip keyIdx
};

union CodecSpecificInfoUnion
{
    CodecSpecificInfoVP8       VP8;
};

// Note: if any pointers are added to this struct or its sub-structs, it
// must be fitted with a copy-constructor. This is because it is copied
// in the copy-constructor of VCMEncodedFrame.
struct CodecSpecificInfo
{
    VideoCodecType   codecType;
    CodecSpecificInfoUnion codecSpecific;
};

class EncodedImageCallback
{
public:
    virtual ~EncodedImageCallback() {};

    // Callback function which is called when an image has been encoded.
    //
    // Input:
    //          - encodedImage         : The encoded image
    //
    // Return value                    : > 0,   signals to the caller that one or more future frames
    //                                          should be dropped to keep bit rate or frame rate.
    //                                   = 0,   if OK.
    //                                   < 0,   on error.
    virtual WebRtc_Word32
    Encoded(EncodedImage& encodedImage,
            const CodecSpecificInfo* codecSpecificInfo = NULL,
            const RTPFragmentationHeader* fragmentation = NULL) = 0;
};

class VideoEncoder
{
public:
    virtual ~VideoEncoder() {};

    // Initialize the encoder with the information from the VideoCodec.
    //
    // Input:
    //          - codecSettings     : Codec settings
    //          - numberOfCores     : Number of cores available for the encoder
    //          - maxPayloadSize    : The maximum size each payload is allowed
    //                                to have. Usually MTU - overhead.
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 InitEncode(const VideoCodec* codecSettings, WebRtc_Word32 numberOfCores, WebRtc_UWord32 maxPayloadSize) = 0;

    // Encode an I420 image (as a part of a video stream). The encoded image
    // will be returned to the user through the encode complete callback.
    //
    // Input:
    //          - inputImage        : Image to be encoded
    //          - codecSpecificInfo : Pointer to codec specific data
    //          - frameType         : The frame type to encode
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 Encode(const VideoFrame& inputImage,
                                 const CodecSpecificInfo* codecSpecificInfo,
                                 const VideoFrameType frameType) = 0;

    // Register an encode complete callback object.
    //
    // Input:
    //          - callback         : Callback object which handles encoded images.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 RegisterEncodeCompleteCallback(EncodedImageCallback* callback) = 0;

    // Free encoder memory.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 Release() = 0;

    // Inform the encoder about the packet loss and round trip time on the
    // network used to decide the best pattern and signaling.
    //
    //          - packetLoss       : Fraction lost (loss rate in percent =
    //                               100 * packetLoss / 255)
    //          - rtt              : Round-trip time in milliseconds
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 SetChannelParameters(WebRtc_UWord32 packetLoss,
                                               int rtt) = 0;

    // Inform the encoder about the new target bit rate.
    //
    //          - newBitRate       : New target bit rate
    //          - frameRate        : The target frame rate
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 SetRates(WebRtc_UWord32 newBitRate, WebRtc_UWord32 frameRate) = 0;

    // Use this function to enable or disable periodic key frames. Can be useful for codecs
    // which have other ways of stopping error propagation.
    //
    //          - enable           : Enable or disable periodic key frames
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 SetPeriodicKeyFrames(bool enable) { return WEBRTC_VIDEO_CODEC_ERROR; }

    // Codec configuration data to send out-of-band, i.e. in SIP call setup
    //
    //          - buffer           : Buffer pointer to where the configuration data
    //                               should be stored
    //          - size             : The size of the buffer in bytes
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 CodecConfigParameters(WebRtc_UWord8* /*buffer*/, WebRtc_Word32 /*size*/) { return WEBRTC_VIDEO_CODEC_ERROR; }
};

class DecodedImageCallback
{
public:
    virtual ~DecodedImageCallback() {};

    // Callback function which is called when an image has been decoded.
    //
    // Input:
    //          - decodedImage         : The decoded image.
    //
    // Return value                    : 0 if OK, < 0 otherwise.
    virtual WebRtc_Word32 Decoded(VideoFrame& decodedImage) = 0;

    virtual WebRtc_Word32 ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId) {return -1;}

    virtual WebRtc_Word32 ReceivedDecodedFrame(const WebRtc_UWord64 pictureId) {return -1;}
};

class VideoDecoder
{
public:
    virtual ~VideoDecoder() {};

    // Initialize the decoder with the information from the VideoCodec.
    //
    // Input:
    //          - inst              : Codec settings
    //          - numberOfCores     : Number of cores available for the decoder
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 InitDecode(const VideoCodec* codecSettings, WebRtc_Word32 numberOfCores) = 0;

    // Decode encoded image (as a part of a video stream). The decoded image
    // will be returned to the user through the decode complete callback.
    //
    // Input:
    //          - inputImage        : Encoded image to be decoded
    //          - missingFrames     : True if one or more frames have been lost
    //                                since the previous decode call.
    //          - fragmentation     : Specifies where the encoded frame can be
    //                                split into separate fragments. The meaning
    //                                of fragment is codec specific, but often
    //                                means that each fragment is decodable by
    //                                itself.
    //          - codecSpecificInfo : Pointer to codec specific data
    //          - renderTimeMs      : System time to render in milliseconds. Only
    //                                used by decoders with internal rendering.
    //
    // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32
    Decode(const EncodedImage& inputImage,
           bool missingFrames,
           const RTPFragmentationHeader* fragmentation,
           const CodecSpecificInfo* codecSpecificInfo = NULL,
           WebRtc_Word64 renderTimeMs = -1) = 0;

    // Register an decode complete callback object.
    //
    // Input:
    //          - callback         : Callback object which handles decoded images.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 RegisterDecodeCompleteCallback(DecodedImageCallback* callback) = 0;

    // Free decoder memory.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 Release() = 0;

    // Reset decoder state and prepare for a new call.
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 Reset() = 0;

    // Codec configuration data sent out-of-band, i.e. in SIP call setup
    //
    // Input/Output:
    //          - buffer           : Buffer pointer to the configuration data
    //          - size             : The size of the configuration data in
    //                               bytes
    //
    // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
    virtual WebRtc_Word32 SetCodecConfigParameters(const WebRtc_UWord8* /*buffer*/, WebRtc_Word32 /*size*/) { return WEBRTC_VIDEO_CODEC_ERROR; }

    // Create a copy of the codec and its internal state.
    //
    // Return value                : A copy of the instance if OK, NULL otherwise.
    virtual VideoDecoder* Copy() { return NULL; }
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODECS_INTERFACE_VIDEO_CODEC_INTERFACE_H
