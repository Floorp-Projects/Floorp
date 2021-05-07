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
#include "lib/jxl/dec_external_image.h"
#include "lib/jxl/image_ops.h"

namespace jxl {
namespace {

// Decoder case, interleaves an internal float image.
void BM_DecExternalImage_ConvertImageRGBA(benchmark::State& state) {
  const size_t kNumIter = 5;
  size_t xsize = state.range();
  size_t ysize = state.range();
  size_t num_channels = 4;

  ImageMetadata im;
  im.SetAlphaBits(8);
  ImageBundle ib(&im);
  Image3F color(xsize, ysize);
  ZeroFillImage(&color);
  ib.SetFromImage(std::move(color), ColorEncoding::SRGB());
  ImageF alpha(xsize, ysize);
  ZeroFillImage(&alpha);
  ib.SetAlpha(std::move(alpha), /*alpha_is_premultiplied=*/false);

  const size_t bytes_per_row = xsize * num_channels;
  std::vector<uint8_t> interleaved(bytes_per_row * ysize);

  for (auto _ : state) {
    for (size_t i = 0; i < kNumIter; ++i) {
      JXL_CHECK(ConvertToExternal(
          ib,
          /*bits_per_sample=*/8,
          /*float_out=*/false, num_channels, JXL_NATIVE_ENDIAN,
          /*stride*/ bytes_per_row,
          /*thread_pool=*/nullptr, interleaved.data(), interleaved.size(),
          /*out_callback=*/nullptr, /*out_opaque=*/nullptr,
          /*undo_orientation=*/jxl::Orientation::kIdentity));
    }
  }

  // Pixels per second.
  state.SetItemsProcessed(kNumIter * state.iterations() * xsize * ysize);
  state.SetBytesProcessed(kNumIter * state.iterations() * interleaved.size());
}

BENCHMARK(BM_DecExternalImage_ConvertImageRGBA)
    ->RangeMultiplier(2)
    ->Range(256, 2048);

}  // namespace
}  // namespace jxl
