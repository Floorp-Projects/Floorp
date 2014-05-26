/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_

#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {

class H264Encoder : public VideoEncoder {
 public:
  static H264Encoder* Create();

  virtual ~H264Encoder() {}
};  // H264Encoder

class H264Decoder : public VideoDecoder {
 public:
  static H264Decoder* Create();

  virtual ~H264Decoder() {}
};  // H264Decoder

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_H_
