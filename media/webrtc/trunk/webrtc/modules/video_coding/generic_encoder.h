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

#include <stdio.h>
#include <vector>

#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"

namespace webrtc {
class CriticalSectionWrapper;

namespace media_optimization {
class MediaOptimization;
}  // namespace media_optimization

struct EncoderParameters {
  uint32_t target_bitrate;
  uint8_t loss_rate;
  int64_t rtt;
  uint32_t input_frame_rate;
};

/*************************************/
/* VCMEncodeFrameCallback class     */
/***********************************/
class VCMEncodedFrameCallback : public EncodedImageCallback {
 public:
    explicit VCMEncodedFrameCallback(
      EncodedImageCallback* post_encode_callback);
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
    void SetMediaOpt(media_optimization::MediaOptimization* mediaOpt);

    void SetPayloadType(uint8_t payloadType) {
      _payloadType = payloadType;
    }

    void SetInternalSource(bool internalSource) {
      _internalSource = internalSource;
    }

    void SetRotation(VideoRotation rotation) { _rotation = rotation; }
    void SignalLastEncoderImplementationUsed(
        const char* encoder_implementation_name);

 private:
    VCMPacketizationCallback* send_callback_;
    CriticalSectionWrapper* _critSect;
    media_optimization::MediaOptimization* _mediaOpt;
    uint8_t _payloadType;
    bool _internalSource;
    VideoRotation _rotation;

    EncodedImageCallback* post_encode_callback_;

#ifdef DEBUG_ENCODER_BIT_STREAM
    FILE* _bitStreamAfterEncoder;
#endif
};  // end of VCMEncodeFrameCallback class

/******************************/
/* VCMGenericEncoder class    */
/******************************/
class VCMGenericEncoder {
  friend class VCMCodecDataBase;

 public:
  VCMGenericEncoder(VideoEncoder* encoder,
                    VideoEncoderRateObserver* rate_observer,
                    VCMEncodedFrameCallback* encoded_frame_callback,
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
  int32_t Encode(const VideoFrame& inputFrame,
                 const CodecSpecificInfo* codecSpecificInfo,
                 const std::vector<FrameType>& frameTypes);

  void SetEncoderParameters(const EncoderParameters& params);
  EncoderParameters GetEncoderParameters() const;

  int32_t SetPeriodicKeyFrames(bool enable);

  int32_t RequestFrame(const std::vector<FrameType>& frame_types);

  bool InternalSource() const;

  void OnDroppedFrame();

  bool SupportsNativeHandle() const;

  int GetTargetFramerate();

 private:
  VideoEncoder* const encoder_;
  VideoEncoderRateObserver* const rate_observer_;
  VCMEncodedFrameCallback* const vcm_encoded_frame_callback_;
  const bool internal_source_;
  mutable rtc::CriticalSection params_lock_;
  EncoderParameters encoder_params_ GUARDED_BY(params_lock_);
  VideoRotation rotation_;
  bool is_screenshare_;
};  // end of VCMGenericEncoder class

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_GENERIC_ENCODER_H_
