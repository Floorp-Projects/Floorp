// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

using ::jxl::test::Roundtrip;

TEST(PatchDictionaryTest, GrayscaleModular) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  CompressParams cparams;
  cparams.SetLossless();
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2;
  // Without patches: ~25k
  EXPECT_LE(Roundtrip(&io, cparams, {}, pool, &io2), 8000u);
  VerifyRelativeError(*io.Main().color(), *io2.Main().color(), 1e-7f, 0);
}

TEST(PatchDictionaryTest, GrayscaleVarDCT) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  CompressParams cparams;
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2;
  // Without patches: ~47k
  EXPECT_LE(Roundtrip(&io, cparams, {}, pool, &io2), 14000u);
  // Without patches: ~1.2
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params, GetJxlCms(),
                                /*distmap=*/nullptr, pool),
            1.1);
}

}  // namespace
}  // namespace jxl
