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

#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/common.h"
#include "lib/jxl/dec_reconstruct.h"
#include "lib/jxl/epf.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/test_utils.h"

namespace jxl {
namespace {

const size_t xsize = 16;
const size_t ysize = 8;

void GenerateFlat(const float background, const float foreground,
                  std::vector<Image3F>* images) {
  for (size_t c = 0; c < Image3F::kNumPlanes; ++c) {
    Image3F in(xsize, ysize);
    // Plane c = foreground, all others = background.
    for (size_t y = 0; y < ysize; ++y) {
      float* rows[3] = {in.PlaneRow(0, y), in.PlaneRow(1, y),
                        in.PlaneRow(2, y)};
      for (size_t x = 0; x < xsize; ++x) {
        rows[0][x] = rows[1][x] = rows[2][x] = background;
        rows[c][x] = foreground;
      }
    }
    images->push_back(std::move(in));
  }
}

// Single foreground point at any position in any channel
void GeneratePoints(const float background, const float foreground,
                    std::vector<Image3F>* images) {
  for (size_t c = 0; c < Image3F::kNumPlanes; ++c) {
    for (size_t y = 0; y < ysize; ++y) {
      for (size_t x = 0; x < xsize; ++x) {
        Image3F in(xsize, ysize);
        FillImage(background, &in);
        in.PlaneRow(c, y)[x] = foreground;
        images->push_back(std::move(in));
      }
    }
  }
}

void GenerateHorzEdges(const float background, const float foreground,
                       std::vector<Image3F>* images) {
  for (size_t c = 0; c < Image3F::kNumPlanes; ++c) {
    // Begin of foreground rows
    for (size_t y = 1; y < ysize; ++y) {
      Image3F in(xsize, ysize);
      FillImage(background, &in);
      for (size_t iy = y; iy < ysize; ++iy) {
        std::fill(in.PlaneRow(c, iy), in.PlaneRow(c, iy) + xsize, foreground);
      }
      images->push_back(std::move(in));
    }
  }
}

void GenerateVertEdges(const float background, const float foreground,
                       std::vector<Image3F>* images) {
  for (size_t c = 0; c < Image3F::kNumPlanes; ++c) {
    // Begin of foreground columns
    for (size_t x = 1; x < xsize; ++x) {
      Image3F in(xsize, ysize);
      FillImage(background, &in);
      for (size_t iy = 0; iy < ysize; ++iy) {
        float* JXL_RESTRICT row = in.PlaneRow(c, iy);
        for (size_t ix = x; ix < xsize; ++ix) {
          row[ix] = foreground;
        }
      }
      images->push_back(std::move(in));
    }
  }
}

void DumpTestImage(const char* name, const Image3F& img) {
  fprintf(stderr, "Image %s:\n", name);
  for (size_t y = 0; y < img.ysize(); ++y) {
    const float* row_x = img.ConstPlaneRow(0, y);
    const float* row_y = img.ConstPlaneRow(1, y);
    const float* row_b = img.ConstPlaneRow(2, y);
    for (size_t x = 0; x < img.xsize(); ++x) {
      fprintf(stderr, "%5.1f|%5.1f|%5.1f ", row_x[x], row_y[x], row_b[x]);
    }
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}

// Ensures input remains unchanged by filter - verifies the edge-preserving
// nature of the filter because inputs are piecewise constant.
void EnsureUnchanged(const float background, const float foreground,
                     uint32_t epf_iters) {
  std::vector<Image3F> images;
  GenerateFlat(background, foreground, &images);
  GeneratePoints(background, foreground, &images);
  GenerateHorzEdges(background, foreground, &images);
  GenerateVertEdges(background, foreground, &images);

  CodecMetadata metadata;
  JXL_CHECK(metadata.size.Set(xsize, ysize));
  metadata.m.xyb_encoded = false;
  FrameHeader frame_header(&metadata);
  // Ensure no CT is applied
  frame_header.color_transform = ColorTransform::kNone;
  LoopFilter& lf = frame_header.loop_filter;
  lf.gab = false;
  lf.epf_iters = epf_iters;
  FrameDimensions frame_dim = frame_header.ToFrameDimensions();

  jxl::PassesDecoderState state;
  JXL_CHECK(
      jxl::InitializePassesSharedState(frame_header, &state.shared_storage));
  state.Init();
  state.InitForAC(/*pool=*/nullptr);

  state.filter_weights.Init(lf, frame_dim);
  FillImage(-0.5f, &state.filter_weights.sigma);

  for (size_t idx_image = 0; idx_image < images.size(); ++idx_image) {
    const Image3F& in = images[idx_image];
    state.decoded = CopyImage(in);

    ImageBundle out(&metadata.m);
    out.SetFromImage(CopyImage(in), ColorEncoding::LinearSRGB());
    FillImage(-99.f, out.color());  // Initialized with garbage.
    Image3F padded = PadImageMirror(in, 2 * kBlockDim, 0);
    // Call with `force_fir` set to true to force to apply filters to all of the
    // input image.
    JXL_CHECK(FinalizeFrameDecoding(&out, &state, /*pool=*/nullptr,
                                    /*force_fir=*/true,
                                    /*skip_blending=*/true));

#if JXL_HIGH_PRECISION
    VerifyRelativeError(in, *out.color(), 1E-3, 1E-4);
#else
    VerifyRelativeError(in, *out.color(), 1E-2, 1E-2);
#endif
    if (testing::Test::HasFatalFailure()) {
      DumpTestImage("in", in);
      DumpTestImage("out", *out.color());
    }
  }
}

}  // namespace

class AdaptiveReconstructionTest : public testing::TestWithParam<uint32_t> {};

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(EPFItersGroup, AdaptiveReconstructionTest,
                                   testing::Values(1, 2, 3),
                                   testing::PrintToStringParamName());

TEST_P(AdaptiveReconstructionTest, TestBright) {
  EnsureUnchanged(1.0f, 128.0f, GetParam());
}
TEST_P(AdaptiveReconstructionTest, TestDark) {
  EnsureUnchanged(128.0f, 1.0f, GetParam());
}

}  // namespace jxl
