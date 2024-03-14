// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "lib/extras/codec.h"
#include "lib/extras/tone_mapping.h"
#include "lib/jxl/image.h"

namespace jxl {

static void BM_ToneMapping(benchmark::State& state) {
  JXL_ASSIGN_OR_DIE(Image3F color, Image3F::Create(2268, 1512));
  FillImage(0.5f, &color);

  // Use linear Rec. 2020 so that `ToneMapTo` doesn't have to convert to it and
  // we mainly measure the tone mapping itself.
  ColorEncoding linear_rec2020;
  linear_rec2020.SetColorSpace(ColorSpace::kRGB);
  JXL_CHECK(linear_rec2020.SetPrimariesType(Primaries::k2100));
  JXL_CHECK(linear_rec2020.SetWhitePointType(WhitePoint::kD65));
  linear_rec2020.Tf().SetTransferFunction(TransferFunction::kLinear);
  JXL_CHECK(linear_rec2020.CreateICC());

  for (auto _ : state) {
    state.PauseTiming();
    CodecInOut tone_mapping_input;
    JXL_ASSIGN_OR_DIE(Image3F color2,
                      Image3F::Create(color.xsize(), color.ysize()));
    CopyImageTo(color, &color2);
    tone_mapping_input.SetFromImage(std::move(color2), linear_rec2020);
    tone_mapping_input.metadata.m.SetIntensityTarget(255);
    state.ResumeTiming();

    JXL_CHECK(ToneMapTo({0.1, 100}, &tone_mapping_input));
  }

  state.SetItemsProcessed(state.iterations() * color.xsize() * color.ysize());
}
BENCHMARK(BM_ToneMapping);

}  // namespace jxl
