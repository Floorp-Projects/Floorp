/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_ANDROID_JNI_ANDROID_MEDIA_CODEC_DECODER_H_
#define WEBRTC_VIDEO_ENGINE_TEST_ANDROID_JNI_ANDROID_MEDIA_CODEC_DECODER_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {

class AndroidMediaCodecDecoder : public VideoDecoder {
 public:
  AndroidMediaCodecDecoder(JavaVM* vm, jobject surface, jclass decoderClass);
  virtual ~AndroidMediaCodecDecoder() { }

  // Initialize the decoder with the information from the VideoCodec.
  //
  // Input:
  //          - inst              : Codec settings
  //          - numberOfCores     : Number of cores available for the decoder
  //
  // Return value                 : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t InitDecode(
      const VideoCodec* codecSettings, int32_t numberOfCores);

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
  virtual int32_t
  Decode(const EncodedImage& inputImage,
         bool missingFrames,
         const RTPFragmentationHeader* fragmentation,
         const CodecSpecificInfo* codecSpecificInfo = NULL,
         int64_t renderTimeMs = -1);

  // Register an decode complete callback object.
  //
  // Input:
  //          - callback         : Callback object which handles decoded images.
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback);

  // Free decoder memory.
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t Release();

  // Reset decoder state and prepare for a new call.
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t Reset();

  // Codec configuration data sent out-of-band, i.e. in SIP call setup
  //
  // Input/Output:
  //          - buffer           : Buffer pointer to the configuration data
  //          - size             : The size of the configuration data in
  //                               bytes
  //
  // Return value                : WEBRTC_VIDEO_CODEC_OK if OK, < 0 otherwise.
  virtual int32_t SetCodecConfigParameters(
      const uint8_t* /*buffer*/, int32_t /*size*/) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Create a copy of the codec and its internal state.
  //
  // Return value                : A copy of the instance if OK, NULL otherwise.
  virtual VideoDecoder* Copy() { return NULL; }

 private:
  DecodedImageCallback* decode_complete_callback_;
  JavaVM* vm_;
  jobject surface_;
  jobject mediaCodecDecoder_;
  jclass decoderClass_;
  JNIEnv* env_;
  jmethodID setEncodedImageID_;
  bool vm_attached_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_ANDROID_JNI_ANDROID_MEDIA_CODEC_DECODER_H_
