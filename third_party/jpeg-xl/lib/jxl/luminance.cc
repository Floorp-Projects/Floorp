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

#include "lib/jxl/luminance.h"

#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {

void SetIntensityTarget(CodecInOut* io) {
  if (io->target_nits != 0) {
    io->metadata.m.SetIntensityTarget(io->target_nits);
    return;
  }
  if (io->metadata.m.color_encoding.tf.IsPQ()) {
    // Peak luminance of PQ as defined by SMPTE ST 2084:2014.
    io->metadata.m.SetIntensityTarget(10000);
  } else if (io->metadata.m.color_encoding.tf.IsHLG()) {
    // Nominal display peak luminance used as a reference by
    // Rec. ITU-R BT.2100-2.
    io->metadata.m.SetIntensityTarget(1000);
  } else {
    // SDR
    io->metadata.m.SetIntensityTarget(kDefaultIntensityTarget);
  }
}

}  // namespace jxl
