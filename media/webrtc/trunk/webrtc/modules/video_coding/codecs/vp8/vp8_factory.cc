/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/vp8/vp8_factory.h"

#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"
#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"

namespace webrtc {

bool VP8EncoderFactoryConfig::use_simulcast_adapter_ = false;

class VP8EncoderImplFactory : public VideoEncoderFactory {
 public:
  VideoEncoder* Create() override { return new VP8EncoderImpl(); }

  void Destroy(VideoEncoder* encoder) override { delete encoder; }

  virtual ~VP8EncoderImplFactory() {}
};

VP8Encoder* VP8Encoder::Create() {
  if (VP8EncoderFactoryConfig::use_simulcast_adapter()) {
    return new SimulcastEncoderAdapter(new VP8EncoderImplFactory());
  } else {
    return new VP8EncoderImpl();
  }
}

VP8Decoder* VP8Decoder::Create() {
  return new VP8DecoderImpl();
}

}  // namespace webrtc
