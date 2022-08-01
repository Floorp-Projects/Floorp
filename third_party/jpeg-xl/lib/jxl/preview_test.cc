// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
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

  CodecInOut io2;
  Roundtrip(&io, cparams, {}, pool, &io2);
  EXPECT_EQ(preview_xsize, io2.metadata.m.preview_size.xsize());
  EXPECT_EQ(preview_ysize, io2.metadata.m.preview_size.ysize());
  EXPECT_EQ(preview_xsize, io2.preview_frame.xsize());
  EXPECT_EQ(preview_ysize, io2.preview_frame.ysize());

  EXPECT_LE(ButteraugliDistance(io.preview_frame, io2.preview_frame,
                                cparams.ba_params, GetJxlCms(),
                                /*distmap=*/nullptr, pool),
            2.5);
  EXPECT_LE(
      ButteraugliDistance(io.Main(), io2.Main(), cparams.ba_params, GetJxlCms(),
                          /*distmap=*/nullptr, pool),
      2.5);
}

}  // namespace
}  // namespace jxl
