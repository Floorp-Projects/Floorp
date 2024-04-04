// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/alpha.h"

#include <array>

#include "lib/jxl/base/common.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

AlphaBlendingInputLayer makeAbil(const Color& rgb, const float& a) {
  const float* data = rgb.data();
  return {data, data + 1, data + 2, &a};
}

AlphaBlendingOutput makeAbo(Color& rgb, float& a) {
  float* data = rgb.data();
  return {data, data + 1, data + 2, &a};
}

TEST(AlphaTest, BlendingWithNonPremultiplied) {
  const Color bg_rgb{100, 110, 120};
  const float bg_a = 180.f / 255;
  const Color fg_rgb{25, 21, 23};
  const float fg_a = 15420.f / 65535;
  const float fg_a2 = 2.0f;
  Color out_rgb;
  float out_a;
  PerformAlphaBlending(
      /*bg=*/makeAbil(bg_rgb, bg_a),
      /*fg=*/makeAbil(fg_rgb, fg_a),
      /*out=*/makeAbo(out_rgb, out_a), 1,
      /*alpha_is_premultiplied=*/false, /*clamp=*/false);
  EXPECT_ARRAY_NEAR(out_rgb, (Color{77.2f, 83.0f, 90.6f}), 0.05f);
  EXPECT_NEAR(out_a, 3174.f / 4095, 1e-5);
  PerformAlphaBlending(
      /*bg=*/makeAbil(bg_rgb, bg_a),
      /*fg=*/makeAbil(fg_rgb, fg_a2),
      /*out=*/makeAbo(out_rgb, out_a), 1,
      /*alpha_is_premultiplied=*/false, /*clamp=*/true);
  EXPECT_ARRAY_NEAR(out_rgb, fg_rgb, 0.05f);
  EXPECT_NEAR(out_a, 1.0f, 1e-5);
}

TEST(AlphaTest, BlendingWithPremultiplied) {
  const Color bg_rgb{100, 110, 120};
  const float bg_a = 180.f / 255;
  const Color fg_rgb{25, 21, 23};
  const float fg_a = 15420.f / 65535;
  const float fg_a2 = 2.0f;
  Color out_rgb;
  float out_a;
  PerformAlphaBlending(
      /*bg=*/makeAbil(bg_rgb, bg_a),
      /*fg=*/makeAbil(fg_rgb, fg_a),
      /*out=*/makeAbo(out_rgb, out_a), 1,
      /*alpha_is_premultiplied=*/true, /*clamp=*/false);
  EXPECT_ARRAY_NEAR(out_rgb, (Color{101.5f, 105.1f, 114.8f}), 0.05f);
  EXPECT_NEAR(out_a, 3174.f / 4095, 1e-5);
  PerformAlphaBlending(
      /*bg=*/makeAbil(bg_rgb, bg_a),
      /*fg=*/makeAbil(fg_rgb, fg_a2),
      /*out=*/makeAbo(out_rgb, out_a), 1,
      /*alpha_is_premultiplied=*/true, /*clamp=*/true);
  EXPECT_ARRAY_NEAR(out_rgb, fg_rgb, 0.05f);
  EXPECT_NEAR(out_a, 1.0f, 1e-5);
}

TEST(AlphaTest, Mul) {
  const float bg = 100;
  const float fg = 25;
  float out;
  PerformMulBlending(&bg, &fg, &out, 1, /*clamp=*/false);
  EXPECT_NEAR(out, fg * bg, .05f);
  PerformMulBlending(&bg, &fg, &out, 1, /*clamp=*/true);
  EXPECT_NEAR(out, bg, .05f);
}

TEST(AlphaTest, PremultiplyAndUnpremultiply) {
  using F4 = std::array<float, 4>;
  const F4 alpha{0.f, 63.f / 255, 127.f / 255, 1.f};
  F4 r{120, 130, 140, 150};
  F4 g{124, 134, 144, 154};
  F4 b{127, 137, 147, 157};

  PremultiplyAlpha(r.data(), g.data(), b.data(), alpha.data(), alpha.size());
  EXPECT_ARRAY_NEAR(r, (F4{0.0f, 130 * 63.f / 255, 140 * 127.f / 255, 150}),
                    1e-5f);
  EXPECT_ARRAY_NEAR(g, (F4{0.0f, 134 * 63.f / 255, 144 * 127.f / 255, 154}),
                    1e-5f);
  EXPECT_ARRAY_NEAR(b, (F4{0.0f, 137 * 63.f / 255, 147 * 127.f / 255, 157}),
                    1e-5f);

  UnpremultiplyAlpha(r.data(), g.data(), b.data(), alpha.data(), alpha.size());
  EXPECT_ARRAY_NEAR(r, (F4{120, 130, 140, 150}), 1e-4f);
  EXPECT_ARRAY_NEAR(g, (F4{124, 134, 144, 154}), 1e-4f);
  EXPECT_ARRAY_NEAR(b, (F4{127, 137, 147, 157}), 1e-4f);
}

TEST(AlphaTest, UnpremultiplyAndPremultiply) {
  using F4 = std::array<float, 4>;
  const F4 alpha{0.f, 63.f / 255, 127.f / 255, 1.f};
  F4 r{50, 60, 70, 80};
  F4 g{54, 64, 74, 84};
  F4 b{57, 67, 77, 87};

  UnpremultiplyAlpha(r.data(), g.data(), b.data(), alpha.data(), alpha.size());
  EXPECT_ARRAY_NEAR(
      r, (F4{50.0f * (1 << 26), 60 * 255.f / 63, 70 * 255.f / 127, 80}), 1e-4f);
  EXPECT_ARRAY_NEAR(
      g, (F4{54.0f * (1 << 26), 64 * 255.f / 63, 74 * 255.f / 127, 84}), 1e-4f);
  EXPECT_ARRAY_NEAR(
      b, (F4{57.0f * (1 << 26), 67 * 255.f / 63, 77 * 255.f / 127, 87}), 1e-4f);

  PremultiplyAlpha(r.data(), g.data(), b.data(), alpha.data(), alpha.size());
  EXPECT_ARRAY_NEAR(r, (F4{50, 60, 70, 80}), 1e-4);
  EXPECT_ARRAY_NEAR(g, (F4{54, 64, 74, 84}), 1e-4);
  EXPECT_ARRAY_NEAR(b, (F4{57, 67, 77, 87}), 1e-4);
}

}  // namespace
}  // namespace jxl
