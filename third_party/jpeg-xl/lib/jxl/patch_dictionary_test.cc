// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>

#include <cstdint>
#include <vector>

#include "lib/extras/codec.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using test::ReadTestData;
using test::Roundtrip;

TEST(PatchDictionaryTest, GrayscaleModular) {
  const std::vector<uint8_t> orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));

  CompressParams cparams;
  cparams.SetLossless();
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2;
  // Without patches: ~25k
  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _, &compressed_size));
  EXPECT_LE(compressed_size, 8000u);
  JXL_ASSERT_OK(VerifyRelativeError(*io.Main().color(), *io2.Main().color(),
                                    1e-7f, 0, _));
}

TEST(PatchDictionaryTest, GrayscaleVarDCT) {
  const std::vector<uint8_t> orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));

  CompressParams cparams;
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2;
  // Without patches: ~47k
  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _, &compressed_size));
  EXPECT_LE(compressed_size, 14000u);
  // Without patches: ~1.2
  EXPECT_LE(ButteraugliDistance(io.frames, io2.frames, ButteraugliParams(),
                                *JxlGetDefaultCms(),
                                /*distmap=*/nullptr),
            1.1);
}

}  // namespace
}  // namespace jxl
