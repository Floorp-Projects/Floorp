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

#include "benchmark/benchmark.h"
#include "lib/extras/codec.h"
#include "lib/extras/tone_mapping.h"
#include "lib/jxl/testdata.h"

namespace jxl {

static void BM_ToneMapping(benchmark::State& state) {
  CodecInOut image;
  const PaddedBytes image_bytes =
      ReadTestData("imagecompression.info/flower_foveon.png");
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(image_bytes), &image));

  // Convert to linear Rec. 2020 so that `ToneMapTo` doesn't have to and we
  // mainly measure the tone mapping itself.
  ColorEncoding linear_rec2020;
  linear_rec2020.SetColorSpace(ColorSpace::kRGB);
  linear_rec2020.primaries = Primaries::k2100;
  linear_rec2020.white_point = WhitePoint::kD65;
  linear_rec2020.tf.SetTransferFunction(TransferFunction::kLinear);
  JXL_CHECK(linear_rec2020.CreateICC());
  JXL_CHECK(image.TransformTo(linear_rec2020));

  for (auto _ : state) {
    state.PauseTiming();
    CodecInOut tone_mapping_input;
    tone_mapping_input.SetFromImage(CopyImage(*image.Main().color()),
                                    image.Main().c_current());
    tone_mapping_input.metadata.m.SetIntensityTarget(
        image.metadata.m.IntensityTarget());
    state.ResumeTiming();

    JXL_CHECK(ToneMapTo({0.1, 100}, &tone_mapping_input));
  }

  state.SetItemsProcessed(state.iterations() * image.xsize() * image.ysize());
}
BENCHMARK(BM_ToneMapping);

}  // namespace jxl
