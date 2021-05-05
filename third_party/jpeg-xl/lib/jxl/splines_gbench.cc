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
#include "lib/jxl/splines.h"

namespace jxl {
namespace {

constexpr int kQuantizationAdjustment = 0;
const ColorCorrelationMap* const cmap = new ColorCorrelationMap;
const float kYToX = cmap->YtoXRatio(0);
const float kYToB = cmap->YtoBRatio(0);

void BM_Splines(benchmark::State& state) {
  const size_t n = state.range();

  std::vector<Spline> spline_data = {
      {/*control_points=*/{
           {9, 54}, {118, 159}, {97, 3}, {10, 40}, {150, 25}, {120, 300}},
       /*color_dct=*/
       {{0.03125f, 0.00625f, 0.003125f}, {1.f, 0.321875f}, {1.f, 0.24375f}},
       /*sigma_dct=*/{0.3125f, 0.f, 0.f, 0.0625f}}};
  std::vector<QuantizedSpline> quantized_splines;
  std::vector<Spline::Point> starting_points;
  for (const Spline& spline : spline_data) {
    quantized_splines.emplace_back(spline, kQuantizationAdjustment, kYToX,
                                   kYToB);
    starting_points.push_back(spline.control_points.front());
  }
  Splines splines(kQuantizationAdjustment, std::move(quantized_splines),
                  std::move(starting_points));

  Image3F drawing_area(320, 320);
  ZeroFillImage(&drawing_area);
  for (auto _ : state) {
    for (size_t i = 0; i < n; ++i) {
      JXL_CHECK(splines.AddTo(&drawing_area, Rect(drawing_area),
                              Rect(drawing_area), *cmap));
    }
  }

  state.SetItemsProcessed(n * state.iterations());
}

BENCHMARK(BM_Splines)->Range(1, 1 << 10);

}  // namespace
}  // namespace jxl
