/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_L2T1_KEY_H_
#define MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_L2T1_KEY_H_

#include <vector>

#include "api/transport/rtp/dependency_descriptor.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/video_coding/svc/scalable_video_controller.h"

namespace webrtc {

// S1  0--0--0-
//     |       ...
// S0  0--0--0-
class ScalabilityStructureL2T1Key : public ScalableVideoController {
 public:
  ~ScalabilityStructureL2T1Key() override;

  StreamLayersConfig StreamConfig() const override;
  FrameDependencyStructure DependencyStructure() const override;

  std::vector<LayerFrameConfig> NextFrameConfig(bool restart) override;
  GenericFrameInfo OnEncodeDone(const LayerFrameConfig& config) override;

 private:
  LayerFrameConfig KeyFrameConfig() const;

  bool keyframe_ = true;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_SVC_SCALABILITY_STRUCTURE_L2T1_KEY_H_
