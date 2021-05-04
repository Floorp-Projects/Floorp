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

#include <stdlib.h>

#include <algorithm>

#include "gtest/gtest.h"
#include "lib/jxl/enc_gamma_correct.h"

namespace jxl {
namespace {

TEST(GammaCorrectTest, TestLinearToSrgbEdgeCases) {
  EXPECT_EQ(0, LinearToSrgb8Direct(0.0));
  EXPECT_NEAR(0, LinearToSrgb8Direct(1E-6f), 2E-5);
  EXPECT_EQ(0, LinearToSrgb8Direct(-1E-6f));
  EXPECT_EQ(0, LinearToSrgb8Direct(-1E6));
  EXPECT_NEAR(1, LinearToSrgb8Direct(1 - 1E-6f), 1E-5);
  EXPECT_EQ(1, LinearToSrgb8Direct(1 + 1E-6f));
  EXPECT_EQ(1, LinearToSrgb8Direct(1E6));
}

TEST(GammaCorrectTest, TestRoundTrip) {
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter)
  for (double linear = 0.0; linear <= 1.0; linear += 1E-7) {
    const double srgb = LinearToSrgb8Direct(linear);
    const double linear2 = Srgb8ToLinearDirect(srgb);
    ASSERT_LT(std::abs(linear - linear2), 2E-13)
        << "linear = " << linear << ", linear2 = " << linear2;
  }
}

}  // namespace
}  // namespace jxl
