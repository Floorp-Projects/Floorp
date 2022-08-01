// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/render_hdr.h"

#include "lib/extras/hlg.h"
#include "lib/extras/tone_mapping.h"
#include "lib/jxl/enc_color_management.h"

namespace jxl {

Status RenderHDR(CodecInOut* io, float display_nits, ThreadPool* pool) {
  const ColorEncoding& original_color_encoding = io->metadata.m.color_encoding;
  if (!(original_color_encoding.tf.IsPQ() ||
        original_color_encoding.tf.IsHLG())) {
    // Nothing to do.
    return true;
  }

  if (original_color_encoding.tf.IsPQ()) {
    JXL_RETURN_IF_ERROR(ToneMapTo({0, display_nits}, io, pool));
    JXL_RETURN_IF_ERROR(GamutMap(io, /*preserve_saturation=*/0.1, pool));
  } else {
    const float intensity_target = io->metadata.m.IntensityTarget();
    const float gamma_hlg_to_display = GetHlgGamma(display_nits);
    // If the image is already in display space, we need to account for the
    // already-applied OOTF.
    const float gamma_display_to_display =
        gamma_hlg_to_display / GetHlgGamma(intensity_target);
    // Ensures that conversions to linear in HlgOOTF below will not themselves
    // include the OOTF.
    io->metadata.m.SetIntensityTarget(300);

    bool need_gamut_mapping = false;
    for (ImageBundle& ib : io->frames) {
      const float gamma = ib.c_current().tf.IsHLG() ? gamma_hlg_to_display
                                                    : gamma_display_to_display;
      if (gamma < 1) need_gamut_mapping = true;
      JXL_RETURN_IF_ERROR(HlgOOTF(&ib, gamma, pool));
    }
    io->metadata.m.SetIntensityTarget(display_nits);

    if (need_gamut_mapping) {
      JXL_RETURN_IF_ERROR(GamutMap(io, /*preserve_saturation=*/0.1, pool));
    }
  }

  ColorEncoding rec2020_pq;
  rec2020_pq.SetColorSpace(ColorSpace::kRGB);
  rec2020_pq.white_point = WhitePoint::kD65;
  rec2020_pq.primaries = Primaries::k2100;
  rec2020_pq.tf.SetTransferFunction(TransferFunction::kPQ);
  JXL_RETURN_IF_ERROR(rec2020_pq.CreateICC());
  io->metadata.m.color_encoding = rec2020_pq;
  return io->TransformTo(rec2020_pq, GetJxlCms(), pool);
}

}  // namespace jxl
