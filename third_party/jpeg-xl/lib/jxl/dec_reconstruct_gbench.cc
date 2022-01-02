// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "benchmark/benchmark.h"
#include "lib/jxl/dec_reconstruct.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

static void BM_UndoXYB(benchmark::State& state, TransferFunction tf) {
  const size_t xsize = state.range();
  const size_t ysize = xsize;
  Image3F src(xsize, ysize);
  Image3F dst(xsize, ysize);
  FillImage(1.f, &src);

  OutputEncodingInfo output_info = {};
  CodecMetadata metadata = {};
  JXL_CHECK(output_info.Set(metadata, ColorEncoding::LinearSRGB(false)));
  output_info.inverse_gamma = 1.;
  // Set the TransferFunction since the code executed depends on this parameter.
  output_info.color_encoding.tf.SetTransferFunction(tf);

  ThreadPool* null_pool = nullptr;
  for (auto _ : state) {
    UndoXYB(src, &dst, output_info, null_pool);
  }
  state.SetItemsProcessed(xsize * ysize * state.iterations());
}

BENCHMARK_CAPTURE(BM_UndoXYB, Linear, TransferFunction::kLinear)
    ->RangeMultiplier(4)
    ->Range(512, 2048);
BENCHMARK_CAPTURE(BM_UndoXYB, SRGB, TransferFunction::kSRGB)
    ->RangeMultiplier(4)
    ->Range(512, 2048);
BENCHMARK_CAPTURE(BM_UndoXYB, PQ, TransferFunction::kPQ)
    ->RangeMultiplier(4)
    ->Range(512, 2048);
BENCHMARK_CAPTURE(BM_UndoXYB, DCI, TransferFunction::kDCI)
    ->RangeMultiplier(4)
    ->Range(512, 2048);

}  // namespace jxl
