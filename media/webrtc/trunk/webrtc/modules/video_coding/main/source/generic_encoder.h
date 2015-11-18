/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"

#include <stdio.h>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {
class CriticalSectionWrapper;

namespace media_optimization {
class MediaOptimization;
}  // namespace media_optimization

/*************************************/
/* VCMEncodeFrameCallback class     */
/***********************************/
class VCMEncodedFrameCallback : public EncodedImageCallback
{
public:
    VCMEncodedFrameCallback(EncodedImageCallback* post_encode_callback);
    virtual ~VCMEncodedFrameCallback();

    void SetCritSect(CriticalSectionWrapper* critSect);

    /*
    * Callback implementation - codec encode complete
    */
    int32_t Encoded(
        const EncodedImage& encodedImage,
        const CodecSpecificInfo* codecSpecificInfo = NULL,
        const RTPFragmentationHeader* fragmentationHeader = NULL);
    /*
    * Callback implementation - generic encoder encode complete
    */
    int32_t SetTransportCallback(VCMPacketizationCallback* transport);
    /**
    * Set media Optimization
    */
    void SetMediaOpt (media_optimization::MediaOptimization* mediaOpt);

    void SetPayloadType(uint8_t payloadType) { _payloadType = payloadType; };
    void SetInternalSource(bool internalSource) { _internalSource = internalSource; };

    void SetRotation(VideoRotation rotation) { _rotation = rotation; }

private:
    VCMPacketizationCallback* _sendCallback;
    CriticalSectionWrapper* _critSect;
    media_optimization::MediaOptimization* _mediaOpt;
    uint8_t _payloadType;
    bool _internalSource;
    VideoRotation _rotation;

    EncodedImageCallback* post_encode_callback_;

#ifdef DEBUG_ENCODER_BIT_STREAM
    FILE* _bitStreamAfterEncoder;
#endif
};// end of VCMEncodeFrameCallback class


/******************************/
/* VCMGenericEncoder class    */
/******************************/
class VCMGenericEncoder
{
    friend class VCMCodecDataBase;
public:
    VCMGenericEncoder(VideoEncoder* encoder,
                      VideoEncoderRateObserver* rate_observer,
                      bool internalSource);
    ~VCMGenericEncoder();
    /**
    * Free encoder memory
    */
    int32_t Release();
    /**
    * Initialize the encoder with the information from the VideoCodec
    */
    int32_t InitEncode(const VideoCodec* settings,
                       int32_t numberOfCores,
                       size_t maxPayloadSize);
    /**
    * Encode raw image
    * inputFrame        : Frame containing raw image
    * codecSpecificInfo : Specific codec data
    * cameraFrameRate   : Request or information from the remote side
    * frameType         : The requested frame type to encode
    */
    int32_t Encode(const I420VideoFrame& inputFrame,
                   const CodecSpecificInfo* codecSpecificInfo,
                   const std::vector<FrameType>& frameTypes);
    /**
    * Set new target bitrate (bits/s) and framerate.
    * Return Value: new bit rate if OK, otherwise <0s.
    */
    // TODO(tommi): We could replace BitRate and FrameRate below with a GetRates
    // method that matches SetRates. For fetching current rates, we'd then only
    // grab the lock once instead of twice.
    int32_t SetRates(uint32_t target_bitrate, uint32_t frameRate);
    /**
    * Set a new packet loss rate and a new round-trip time in milliseconds.
    */
    int32_t SetChannelParameters(int32_t packetLoss, int64_t rtt);
    int32_t CodecConfigParameters(uint8_t* buffer, int32_t size);
    /**
    * Register a transport callback which will be called to deliver the encoded
    * buffers
    */
    int32_t RegisterEncodeCallback(
        VCMEncodedFrameCallback* VCMencodedFrameCallback);
    /**
    * Get encoder bit rate
    */
    uint32_t BitRate() const;
     /**
    * Get encoder frame rate
    */
    uint32_t FrameRate() const;

    int32_t SetPeriodicKeyFrames(bool enable);

    int32_t RequestFrame(const std::vector<FrameType>& frame_types);

    bool InternalSource() const;

private:
    VideoEncoder* const encoder_;
    VideoEncoderRateObserver* const rate_observer_;
    VCMEncodedFrameCallback*  vcm_encoded_frame_callback_;
    uint32_t bit_rate_;
    uint32_t frame_rate_;
    const bool internal_source_;
    mutable rtc::CriticalSection rates_lock_;
    VideoRotation rotation_;
}; // end of VCMGenericEncoder class

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
