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
#include "lib/extras/codec.h"
#include "lib/jxl/dec_params.h"
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
  cparams.color_transform = jxl::ColorTransform::kNone;
  cparams.modular_mode = true;
  cparams.patches = jxl::Override::kOn;
  DecompressParams dparams;

  CodecInOut io2;
  Roundtrip(&io, cparams, dparams, pool, &io2);
  VerifyEqual(*io.Main().color(), *io2.Main().color());
}

TEST(PatchDictionaryTest, GrayscaleVarDCT) {
  ThreadPool* pool = nullptr;
  const PaddedBytes orig = ReadTestData("jxl/grayscale_patches.png");
  CodecInOut io;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, pool));

  CompressParams cparams;
  cparams.patches = jxl::Override::kOn;
  DecompressParams dparams;

  CodecInOut io2;
  Roundtrip(&io, cparams, dparams, pool, &io2);
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, pool),
            2);
}

}  // namespace
}  // namespace jxl
