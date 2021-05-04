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

#include <stddef.h>

#include <string>

#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {
using test::Roundtrip;

TEST(PreviewTest, RoundtripGivenPreview) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));
  io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);
  // Same as main image
  io.preview_frame = io.Main().Copy();
  const size_t preview_xsize = 15;
  const size_t preview_ysize = 27;
  io.preview_frame.ShrinkTo(preview_xsize, preview_ysize);
  io.metadata.m.have_preview = true;
  ASSERT_TRUE(io.metadata.m.preview_size.Set(io.preview_frame.xsize(),
                                             io.preview_frame.ysize()));

  CompressParams cparams;
  cparams.butteraugli_distance = 2.0;
  cparams.speed_tier = SpeedTier::kSquirrel;
  DecompressParams dparams;

  dparams.preview = Override::kOff;

  CodecInOut io2;
  Roundtrip(&io, cparams, dparams, pool, &io2);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.5);
  EXPECT_EQ(0, io2.preview_frame.xsize());

  dparams.preview = Override::kOn;

  CodecInOut io3;
  Roundtrip(&io, cparams, dparams, pool, &io3);
  EXPECT_EQ(preview_xsize, io3.metadata.m.preview_size.xsize());
  EXPECT_EQ(preview_ysize, io3.metadata.m.preview_size.ysize());
  EXPECT_EQ(preview_xsize, io3.preview_frame.xsize());
  EXPECT_EQ(preview_ysize, io3.preview_frame.ysize());

  EXPECT_LE(ButteraugliDistance(io.preview_frame, io3.preview_frame,
                                cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.5);
  EXPECT_LE(ButteraugliDistance(io.Main(), io3.Main(), cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2.5);
}

}  // namespace
}  // namespace jxl
