// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/cms.h>
#include <jxl/memory_manager.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "lib/extras/codec.h"
#include "lib/jxl/base/override.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using test::ButteraugliDistance;
using test::ReadTestData;
using test::Roundtrip;

TEST(PatchDictionaryTest, GrayscaleModular) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));

  CompressParams cparams;
  cparams.SetLossless();
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2{memory_manager};
  // Without patches: ~25k
  size_t compressed_size;
  JXL_EXPECT_OK(Roundtrip(&io, cparams, {}, &io2, _, &compressed_size));
  EXPECT_LE(compressed_size, 8000u);
  JXL_ASSERT_OK(VerifyRelativeError(*io.Main().color(), *io2.Main().color(),
                                    1e-7f, 0, _));
}

TEST(PatchDictionaryTest, GrayscaleVarDCT) {
  JxlMemoryManager* memory_manager = jxl::test::MemoryManager();
  const std::vector<uint8_t> orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io{memory_manager};
  ASSERT_TRUE(SetFromBytes(Bytes(orig), &io));

  CompressParams cparams;
  cparams.patches = jxl::Override::kOn;

  CodecInOut io2{memory_manager};
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
