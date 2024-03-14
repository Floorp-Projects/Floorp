// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_gaborish.h"

#include <jxl/types.h>

#include <hwy/base.h>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

// weight1,2 need not be normalized.
WeightsSymmetric3 GaborishKernel(float weight1, float weight2) {
  constexpr float weight0 = 1.0f;

  // Normalize
  const float mul = 1.0f / (weight0 + 4 * (weight1 + weight2));
  const float w0 = weight0 * mul;
  const float w1 = weight1 * mul;
  const float w2 = weight2 * mul;

  const WeightsSymmetric3 w = {{HWY_REP4(w0)}, {HWY_REP4(w1)}, {HWY_REP4(w2)}};
  return w;
}

void ConvolveGaborish(const ImageF& in, float weight1, float weight2,
                      ThreadPool* pool, ImageF* JXL_RESTRICT out) {
  JXL_CHECK(SameSize(in, *out));
  Symmetric3(in, Rect(in), GaborishKernel(weight1, weight2), pool, out);
}

void TestRoundTrip(const Image3F& in, float max_l1) {
  JXL_ASSIGN_OR_DIE(Image3F fwd, Image3F::Create(in.xsize(), in.ysize()));
  ThreadPool* null_pool = nullptr;
  ConvolveGaborish(in.Plane(0), 0, 0, null_pool, &fwd.Plane(0));
  ConvolveGaborish(in.Plane(1), 0, 0, null_pool, &fwd.Plane(1));
  ConvolveGaborish(in.Plane(2), 0, 0, null_pool, &fwd.Plane(2));
  float w = 0.92718927264540152f;
  float weights[3] = {
      w,
      w,
      w,
  };
  JXL_CHECK(GaborishInverse(&fwd, Rect(fwd), weights, null_pool));
  JXL_ASSERT_OK(VerifyRelativeError(in, fwd, max_l1, 1E-4f, _));
}

TEST(GaborishTest, TestZero) {
  JXL_ASSIGN_OR_DIE(Image3F in, Image3F::Create(20, 20));
  ZeroFillImage(&in);
  TestRoundTrip(in, 0.0f);
}

// Disabled: large difference.
#if JXL_FALSE
TEST(GaborishTest, TestDirac) {
  JXL_ASSIGN_OR_DIE(Image3F in, Image3F::Create(20, 20));
  ZeroFillImage(&in);
  in.PlaneRow(1, 10)[10] = 10.0f;
  TestRoundTrip(in, 0.26f);
}
#endif

TEST(GaborishTest, TestFlat) {
  JXL_ASSIGN_OR_DIE(Image3F in, Image3F::Create(20, 20));
  FillImage(1.0f, &in);
  TestRoundTrip(in, 1E-5f);
}

}  // namespace
}  // namespace jxl
