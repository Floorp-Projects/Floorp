/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/libaom_av1_encoder_supported.h"

#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"  // nogncheck
#endif

namespace webrtc {
#if defined(RTC_USE_LIBAOM_AV1_ENCODER)
ABSL_CONST_INIT const bool kIsLibaomAv1EncoderSupported = true;
std::unique_ptr<VideoEncoder> CreateLibaomAv1EncoderIfSupported() {
  return CreateLibaomAv1Encoder();
}
#else
ABSL_CONST_INIT const bool kIsLibaomAv1EncoderSupported = false;
std::unique_ptr<VideoEncoder> CreateLibaomAv1EncoderIfSupported() {
  return nullptr;
}
#endif

}  // namespace webrtc
