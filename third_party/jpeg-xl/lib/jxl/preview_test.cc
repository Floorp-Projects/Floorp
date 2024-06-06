// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/memory_manager.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lib/extras/codec.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/common.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/headers.h"
#include "lib/jxl/image_bundle.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {
using test::ButteraugliDistance;
using test::ReadTestData;
using test::Roundtrip;

TEST(PreviewTest, RoundtripGivenPreview) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig =
      ReadTestData("external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));
  io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);
  // Same as main image
  JXL_ASSIGN_OR_DIE(io.preview_frame, io.Main().Copy());
  const size_t preview_xsize = 15;
  const size_t preview_ysize = 27;
  io.preview_frame.ShrinkTo(preview_xsize, preview_ysize);
  io.metadata.m.have_preview = true;
  ASSERT_TRUE(io.metadata.m.preview_size.Set(io.preview_frame.xsize(),
                                             io.preview_frame.ysize()));

  CompressParams cparams;
  cparams.butteraugli_distance = 2.0;
  cparams.speed_tier = SpeedTier::kSquirrel;
  cparams.SetCms(*JxlGetDefaultCms());

  CodecInOut io2{memory_manager};
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _));
  EXPECT_EQ(preview_xsize, io2.metadata.m.preview_size.xsize());
  EXPECT_EQ(preview_ysize, io2.metadata.m.preview_size.ysize());
  EXPECT_EQ(preview_xsize, io2.preview_frame.xsize());
  EXPECT_EQ(preview_ysize, io2.preview_frame.ysize());

  EXPECT_LE(ButteraugliDistance(io.preview_frame, io2.preview_frame,
                                ButteraugliParams(), *JxlGetDefaultCms(),
                                /*distmap=*/nullptr),
            2.5);
  EXPECT_LE(ButteraugliDistance(io.Main(), io2.Main(), ButteraugliParams(),
                                *JxlGetDefaultCms(),
                                /*distmap=*/nullptr),
            2.5);
}

}  // namespace
}  // namespace jxl
