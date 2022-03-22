// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/luminance.h"

#include "lib/jxl/codec_in_out.h"

namespace jxl {

void SetIntensityTarget(CodecInOut* io) { SetIntensityTarget(&io->metadata.m); }

void SetIntensityTarget(ImageMetadata* m) {
  if (m->color_encoding.tf.IsPQ()) {
    // Peak luminance of PQ as defined by SMPTE ST 2084:2014.
    m->SetIntensityTarget(10000);
  } else if (m->color_encoding.tf.IsHLG()) {
    // Nominal display peak luminance used as a reference by
    // Rec. ITU-R BT.2100-2.
    m->SetIntensityTarget(1000);
  } else {
    // SDR
    m->SetIntensityTarget(kDefaultIntensityTarget);
  }
}

}  // namespace jxl
