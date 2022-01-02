/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/codecs/av1/create_scalability_structure.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l1t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t1h.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l2t2_key_shift.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t1.h"
#include "modules/video_coding/codecs/av1/scalability_structure_l3t3.h"
#include "modules/video_coding/codecs/av1/scalability_structure_s2t1.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller.h"
#include "modules/video_coding/codecs/av1/scalable_video_controller_no_layering.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

struct NamedStructureFactory {
  absl::string_view name;
  // Use function pointer to make NamedStructureFactory trivally destructable.
  std::unique_ptr<ScalableVideoController> (*factory)();
};

// Wrap std::make_unique function to have correct return type.
template <typename T>
std::unique_ptr<ScalableVideoController> Create() {
  return std::make_unique<T>();
}

constexpr NamedStructureFactory kFactories[] = {
    {"NONE", Create<ScalableVideoControllerNoLayering>},
    {"L1T2", Create<ScalabilityStructureL1T2>},
    {"L1T3", Create<ScalabilityStructureL1T3>},
    {"L2T1", Create<ScalabilityStructureL2T1>},
    {"L2T1h", Create<ScalabilityStructureL2T1h>},
    {"L2T1_KEY", Create<ScalabilityStructureL2T1Key>},
    {"L2T2", Create<ScalabilityStructureL2T2>},
    {"L2T2_KEY", Create<ScalabilityStructureL2T2Key>},
    {"L2T2_KEY_SHIFT", Create<ScalabilityStructureL2T2KeyShift>},
    {"L3T1", Create<ScalabilityStructureL3T1>},
    {"L3T3", Create<ScalabilityStructureL3T3>},
    {"S2T1", Create<ScalabilityStructureS2T1>},
};

}  // namespace

std::unique_ptr<ScalableVideoController> CreateScalabilityStructure(
    absl::string_view name) {
  RTC_DCHECK(!name.empty());
  for (const auto& entry : kFactories) {
    if (entry.name == name) {
      return entry.factory();
    }
  }
  return nullptr;
}

}  // namespace webrtc
