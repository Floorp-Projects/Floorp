// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/modular/transform/transform.h"

#include "lib/jxl/fields.h"
#include "lib/jxl/modular/modular_image.h"
#include "lib/jxl/modular/transform/near-lossless.h"
#include "lib/jxl/modular/transform/palette.h"
#include "lib/jxl/modular/transform/squeeze.h"
#include "lib/jxl/modular/transform/subtractgreen.h"

namespace jxl {

namespace {
const char *transform_name[static_cast<uint32_t>(TransformId::kNumTransforms)] =
    {"RCT", "Palette", "Squeeze", "Invalid", "Near-Lossless"};
}  // namespace

SqueezeParams::SqueezeParams() { Bundle::Init(this); }
Transform::Transform(TransformId id) {
  Bundle::Init(this);
  this->id = id;
}

const char *Transform::TransformName() const {
  return transform_name[static_cast<uint32_t>(id)];
}

Status Transform::Forward(Image &input, const weighted::Header &wp_header,
                          ThreadPool *pool) {
  switch (id) {
    case TransformId::kRCT:
      return FwdSubtractGreen(input, begin_c, rct_type);
    case TransformId::kSqueeze:
      return FwdSqueeze(input, squeezes, pool);
    case TransformId::kPalette:
      return FwdPalette(input, begin_c, begin_c + num_c - 1, nb_colors,
                        ordered_palette, lossy_palette, predictor, wp_header);
    case TransformId::kNearLossless:
      return FwdNearLossless(input, begin_c, begin_c + num_c - 1,
                             max_delta_error, predictor);
    default:
      return JXL_FAILURE("Unknown transformation (ID=%u)",
                         static_cast<unsigned int>(id));
  }
}

Status Transform::Inverse(Image &input, const weighted::Header &wp_header,
                          ThreadPool *pool) {
  switch (id) {
    case TransformId::kRCT:
      return InvSubtractGreen(input, begin_c, rct_type);
    case TransformId::kSqueeze:
      return InvSqueeze(input, squeezes, pool);
    case TransformId::kPalette:
      return InvPalette(input, begin_c, nb_colors, nb_deltas, predictor,
                        wp_header, pool);
    default:
      return JXL_FAILURE("Unknown transformation (ID=%u)",
                         static_cast<unsigned int>(id));
  }
}

Status Transform::MetaApply(Image &input) {
  switch (id) {
    case TransformId::kRCT:
      JXL_DEBUG_V(2, "Transform: kRCT, rct_type=%" PRIu32, rct_type);
      return true;
    case TransformId::kSqueeze:
      JXL_DEBUG_V(2, "Transform: kSqueeze:");
#if JXL_DEBUG_V_LEVEL >= 2
      for (const auto &params : squeezes) {
        JXL_DEBUG_V(
            2,
            "  squeeze params: horizontal=%d, in_place=%d, begin_c=%" PRIu32
            ", num_c=%" PRIu32,
            params.horizontal, params.in_place, params.begin_c, params.num_c);
      }
#endif
      return MetaSqueeze(input, &squeezes);
    case TransformId::kPalette:
      JXL_DEBUG_V(2,
                  "Transform: kPalette, begin_c=%" PRIu32 ", num_c=%" PRIu32
                  ", nb_colors=%" PRIu32 ", nb_deltas=%" PRIu32,
                  begin_c, num_c, nb_colors, nb_deltas);
      return MetaPalette(input, begin_c, begin_c + num_c - 1, nb_colors,
                         nb_deltas, lossy_palette);
    default:
      return JXL_FAILURE("Unknown transformation (ID=%u)",
                         static_cast<unsigned int>(id));
  }
}

}  // namespace jxl
