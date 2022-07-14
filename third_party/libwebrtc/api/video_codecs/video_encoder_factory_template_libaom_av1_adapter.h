/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBAOM_AV1_ADAPTER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBAOM_AV1_ADAPTER_H_

#include <memory>
#include <vector>

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"
#include "modules/video_coding/svc/create_scalability_structure.h"
#include "modules/video_coding/svc/scalability_mode_util.h"

namespace webrtc {
struct LibaomAv1EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    return {SdpVideoFormat("AV1")};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    return CreateLibaomAv1Encoder();
  }

  static bool IsScalabilityModeSupported(absl::string_view mode_string) {
    // For libaom AV1, the scalability mode is supported if we can create the
    // scalability structure.
    absl::optional<ScalabilityMode> scalability_mode =
        ScalabilityModeFromString(mode_string);
    return scalability_mode != absl::nullopt &&
           ScalabilityStructureConfig(*scalability_mode) != absl::nullopt;
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_LIBAOM_AV1_ADAPTER_H_
