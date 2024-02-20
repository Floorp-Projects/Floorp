// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/encode.h>
#include <jxl/types.h>

#include <cstddef>
#include <cstdint>
#include <ios>
#include <ostream>
#include <string>
#include <vector>

#include "lib/extras/dec/jxl.h"
#include "lib/extras/enc/jxl.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

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
  os << "SpeedTierTestParams{" << static_cast<size_t>(params.speed_tier)
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
                                        /*shrink8=*/false},
                    // Only downscaled image for Tortoise mode.
                    SpeedTierTestParams{SpeedTier::kTortoise,
                                        /*shrink8=*/true},
                    SpeedTierTestParams{SpeedTier::kGlacier,
                                        /*shrink8=*/true}));

TEST_P(SpeedTierTest, Roundtrip) {
  const SpeedTierTestParams& params = GetParam();
  test::ThreadPoolForTests pool(8);
  const std::vector<uint8_t> orig = jxl::test::ReadTestData(
      "external/wesaturate/500px/u76c0g_bliznaca_srgb8.png");
  test::TestImage t;
  t.DecodeFromBytes(orig).ClearMetadata();
  if (params.speed_tier == SpeedTier::kGlacier) {
    // just a few pixels will already take enough time at this setting
    t.SetDimensions(8, 8);
  } else if (params.shrink8) {
    t.SetDimensions(t.ppf().xsize() / 8, t.ppf().ysize() / 8);
  }

  extras::JXLCompressParams cparams;
  cparams.distance = 1.0f;
  cparams.allow_expert_options = true;
  cparams.AddOption(JXL_ENC_FRAME_SETTING_EFFORT,
                    10 - static_cast<int>(params.speed_tier));
  extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {{3, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0}};

  {
    extras::PackedPixelFile ppf_out;
    test::Roundtrip(t.ppf(), cparams, dparams, nullptr, &ppf_out);
    EXPECT_LE(test::ButteraugliDistance(t.ppf(), ppf_out), 2.0);
  }
  if (params.shrink8) {
    cparams.distance = 0.0f;
    extras::PackedPixelFile ppf_out;
    test::Roundtrip(t.ppf(), cparams, dparams, nullptr, &ppf_out);
    EXPECT_EQ(0.0f, test::ComputeDistance2(t.ppf(), ppf_out));
  }
}
}  // namespace
}  // namespace jxl
