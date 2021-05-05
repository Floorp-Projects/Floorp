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

#include "gtest/gtest.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/dec_xyb.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_test_utils.h"

namespace jxl {
namespace {

TEST(OpsinInverseTest, LinearInverseInverts) {
  Image3F linear(128, 128);
  RandomFillImage(&linear, 1.0f);

  CodecInOut io;
  io.metadata.m.SetFloat32Samples();
  io.metadata.m.color_encoding = ColorEncoding::LinearSRGB();
  io.SetFromImage(CopyImage(linear), io.metadata.m.color_encoding);
  ThreadPool* null_pool = nullptr;
  Image3F opsin(io.xsize(), io.ysize());
  (void)ToXYB(io.Main(), null_pool, &opsin);

  OpsinParams opsin_params;
  opsin_params.Init(/*intensity_target=*/255.0f);
  OpsinToLinearInplace(&opsin, /*pool=*/nullptr, opsin_params);

  VerifyRelativeError(linear, opsin, 3E-3, 2E-4);
}

TEST(OpsinInverseTest, YcbCrInverts) {
  Image3F rgb(128, 128);
  RandomFillImage(&rgb, 1.0f);

  ThreadPool* null_pool = nullptr;
  Image3F ycbcr(rgb.xsize(), rgb.ysize());
  RgbToYcbcr(rgb.Plane(0), rgb.Plane(1), rgb.Plane(2), &ycbcr.Plane(1),
             &ycbcr.Plane(0), &ycbcr.Plane(2), null_pool);

  Image3F rgb2(rgb.xsize(), rgb.ysize());
  YcbcrToRgb(ycbcr, &rgb2, Rect(rgb));

  VerifyRelativeError(rgb, rgb2, 4E-5, 4E-7);
}

}  // namespace
}  // namespace jxl
