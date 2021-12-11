// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
