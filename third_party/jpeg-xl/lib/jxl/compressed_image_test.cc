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

#include <algorithm>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/chroma_from_luma.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/color_management.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_adaptive_quantization.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_xyb.h"
#include "lib/jxl/frame_header.h"
#include "lib/jxl/gaborish.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/loop_filter.h"
#include "lib/jxl/passes_state.h"
#include "lib/jxl/quant_weights.h"
#include "lib/jxl/quantizer.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

// Verifies ReconOpsinImage reconstructs with low butteraugli distance.
void RunRGBRoundTrip(float distance, bool fast) {
  ThreadPoolInternal pool(4);

  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  JXL_CHECK(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));
  // This test can only handle a single group.
  io.ShrinkTo(std::min(io.xsize(), kGroupDim), std::min(io.ysize(), kGroupDim));

  Image3F opsin(io.xsize(), io.ysize());
  (void)ToXYB(io.Main(), &pool, &opsin);
  opsin = PadImageToMultiple(opsin, kBlockDim);
  GaborishInverse(&opsin, 1.0f, &pool);

  CompressParams cparams;
  cparams.butteraugli_distance = distance;
  if (fast) {
    cparams.speed_tier = SpeedTier::kWombat;
  }

  JXL_CHECK(io.metadata.size.Set(opsin.xsize(), opsin.ysize()));
  FrameHeader frame_header(&io.metadata);
  frame_header.color_transform = ColorTransform::kXYB;
  frame_header.loop_filter.epf_iters = 0;

  // Use custom weights for Gaborish.
  frame_header.loop_filter.gab_custom = true;
  frame_header.loop_filter.gab_x_weight1 = 0.11501538179658321f;
  frame_header.loop_filter.gab_x_weight2 = 0.089979079587015454f;
  frame_header.loop_filter.gab_y_weight1 = 0.11501538179658321f;
  frame_header.loop_filter.gab_y_weight2 = 0.089979079587015454f;
  frame_header.loop_filter.gab_b_weight1 = 0.11501538179658321f;
  frame_header.loop_filter.gab_b_weight2 = 0.089979079587015454f;

  PassesEncoderState enc_state;
  JXL_CHECK(InitializePassesSharedState(frame_header, &enc_state.shared));

  enc_state.shared.quantizer.SetQuant(4.0f, 4.0f,
                                      &enc_state.shared.raw_quant_field);
  enc_state.shared.ac_strategy.FillDCT8();
  enc_state.cparams = cparams;
  ZeroFillImage(&enc_state.shared.epf_sharpness);
  CodecInOut io1;
  io1.Main() = RoundtripImage(opsin, &enc_state, &pool);
  io1.metadata.m.color_encoding = io1.Main().c_current();

  EXPECT_LE(ButteraugliDistance(io, io1, cparams.ba_params,
                                /*distmap=*/nullptr, &pool),
            1.2);
}

TEST(CompressedImageTest, RGBRoundTrip_1) { RunRGBRoundTrip(1.0, false); }

TEST(CompressedImageTest, RGBRoundTrip_1_fast) { RunRGBRoundTrip(1.0, true); }

TEST(CompressedImageTest, RGBRoundTrip_2) { RunRGBRoundTrip(2.0, false); }

TEST(CompressedImageTest, RGBRoundTrip_2_fast) { RunRGBRoundTrip(2.0, true); }

}  // namespace
}  // namespace jxl
