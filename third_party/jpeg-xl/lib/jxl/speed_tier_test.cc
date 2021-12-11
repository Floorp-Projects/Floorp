// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <string>

#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/jxl/aux_out.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/dec_params.h"
#include "lib/jxl/enc_butteraugli_comparator.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

struct SpeedTierTestParams {
  explicit SpeedTierTestParams(const SpeedTier speed_tier,
                               const bool shrink8 = false)
      : speed_tier(speed_tier), shrink8(shrink8) {}
  SpeedTier speed_tier;
  bool shrink8;
};

std::ostream& operator<<(std::ostream& os, SpeedTierTestParams params) {
  auto previous_flags = os.flags();
  os << std::boolalpha;
  os << "SpeedTierTestParams{" << SpeedTierName(params.speed_tier)
     << ", /*shrink8=*/" << params.shrink8 << "}";
  os.flags(previous_flags);
  return os;
}

class SpeedTierTest : public testing::TestWithParam<SpeedTierTestParams> {};

JXL_GTEST_INSTANTIATE_TEST_SUITE_P(
    SpeedTierTestInstantiation, SpeedTierTest,
    testing::Values(SpeedTierTestParams{SpeedTier::kCheetah,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kCheetah,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kThunder,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kThunder,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kLightning,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kLightning,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kFalcon,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kFalcon,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kHare,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kHare,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kWombat,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kWombat,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kSquirrel,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kSquirrel,
                                        /*shrink8=*/false},
                    SpeedTierTestParams{SpeedTier::kKitten,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kKitten,
                                        /*shrink8=*/false},
                    // Only downscaled image for Tortoise mode.
                    SpeedTierTestParams{SpeedTier::kTortoise,
                                        /*shrink8=*/true}));

TEST_P(SpeedTierTest, Roundtrip) {
  const PaddedBytes orig =
      ReadTestData("wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  CodecInOut io;
  ThreadPoolInternal pool(8);
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(orig), &io, &pool));

  const SpeedTierTestParams& params = GetParam();

  if (params.shrink8) {
    io.ShrinkTo(io.xsize() / 8, io.ysize() / 8);
  }

  CompressParams cparams;
  cparams.speed_tier = params.speed_tier;
  DecompressParams dparams;

  CodecInOut io2;
  test::Roundtrip(&io, cparams, dparams, nullptr, &io2);

  // Can be 2.2 in non-hare mode.
  EXPECT_LE(ButteraugliDistance(io, io2, cparams.ba_params,
                                /*distmap=*/nullptr, /*pool=*/nullptr),
            2.8);
}
}  // namespace
}  // namespace jxl
