/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_DECODER_H_
#define WEBRTC_VIDEO_DECODER_H_

#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_frame.h"

namespace webrtc {

class RTPFragmentationHeader;
// TODO(pbos): Expose these through a public (root) header or change these APIs.
struct CodecSpecificInfo;
struct VideoCodec;

class DecodedImageCallback {
 public:
  virtual ~DecodedImageCallback() {}

  virtual int32_t Decoded(I420VideoFrame& decodedImage) = 0;
  virtual int32_t ReceivedDecodedReferenceFrame(const uint64_t pictureId) {
    return -1;
  }

  virtual int32_t ReceivedDecodedFrame(const uint64_t pictureId) { return -1; }
};

class VideoDecoder {
 public:
  enum DecoderType {
    kVp8,
    kVp9
  };

  static VideoDecoder* Create(DecoderType codec_type);

  virtual ~VideoDecoder() {}

  virtual int32_t InitDecode(const VideoCodec* codecSettings,
                             int32_t numberOfCores) = 0;

  virtual int32_t Decode(const EncodedImage& inputImage,
                         bool missingFrames,
                         const RTPFragmentationHeader* fragmentation,
                         const CodecSpecificInfo* codecSpecificInfo = NULL,
                         int64_t renderTimeMs = -1) = 0;

  virtual int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;
  virtual int32_t Reset() = 0;

  virtual int32_t SetCodecConfigParameters(const uint8_t* /*buffer*/,
                                           int32_t /*size*/) {
    return -1;
  }

  virtual VideoDecoder* Copy() { return NULL; }
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_DECODER_H_
