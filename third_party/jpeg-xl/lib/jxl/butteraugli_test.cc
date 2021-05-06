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

#include "jxl/butteraugli.h"

#include "gtest/gtest.h"
#include "jxl/butteraugli_cxx.h"
#include "lib/jxl/test_utils.h"

TEST(ButteraugliTest, Lossless) {
  uint32_t xsize = 171;
  uint32_t ysize = 219;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  JxlButteraugliApiPtr api(JxlButteraugliApiCreate(nullptr));
  JxlButteraugliResultPtr result(JxlButteraugliCompute(
      api.get(), xsize, ysize, &pixel_format, pixels.data(), pixels.size(),
      &pixel_format, pixels.data(), pixels.size()));
  EXPECT_EQ(0.0, JxlButteraugliResultGetDistance(result.get(), 8.0));
}

TEST(ButteraugliTest, Distmap) {
  uint32_t xsize = 171;
  uint32_t ysize = 219;
  std::vector<uint8_t> pixels = jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  JxlButteraugliApiPtr api(JxlButteraugliApiCreate(nullptr));
  JxlButteraugliResultPtr result(JxlButteraugliCompute(
      api.get(), xsize, ysize, &pixel_format, pixels.data(), pixels.size(),
      &pixel_format, pixels.data(), pixels.size()));
  EXPECT_EQ(0.0, JxlButteraugliResultGetDistance(result.get(), 8.0));
  const float* distmap;
  uint32_t row_stride;
  JxlButteraugliResultGetDistmap(result.get(), &distmap, &row_stride);
  for (uint32_t y = 0; y < ysize; y++) {
    for (uint32_t x = 0; x < xsize; x++) {
      EXPECT_EQ(0.0, distmap[y * row_stride + x]);
    }
  }
}

TEST(ButteraugliTest, Distorted) {
  uint32_t xsize = 171;
  uint32_t ysize = 219;
  std::vector<uint8_t> orig_pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  std::vector<uint8_t> dist_pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  dist_pixels[0] += 128;

  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  JxlButteraugliApiPtr api(JxlButteraugliApiCreate(nullptr));
  JxlButteraugliResultPtr result(JxlButteraugliCompute(
      api.get(), xsize, ysize, &pixel_format, orig_pixels.data(),
      orig_pixels.size(), &pixel_format, dist_pixels.data(),
      dist_pixels.size()));
  EXPECT_NE(0.0, JxlButteraugliResultGetDistance(result.get(), 8.0));
}

TEST(ButteraugliTest, Api) {
  uint32_t xsize = 171;
  uint32_t ysize = 219;
  std::vector<uint8_t> orig_pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  std::vector<uint8_t> dist_pixels =
      jxl::test::GetSomeTestImage(xsize, ysize, 4, 0);
  dist_pixels[0] += 128;

  JxlPixelFormat pixel_format = {4, JXL_TYPE_UINT16, JXL_BIG_ENDIAN, 0};

  JxlButteraugliApiPtr api(JxlButteraugliApiCreate(nullptr));
  JxlButteraugliApiSetHFAsymmetry(api.get(), 1.0f);
  JxlButteraugliApiSetIntensityTarget(api.get(), 250.0f);
  JxlButteraugliResultPtr result(JxlButteraugliCompute(
      api.get(), xsize, ysize, &pixel_format, orig_pixels.data(),
      orig_pixels.size(), &pixel_format, dist_pixels.data(),
      dist_pixels.size()));
  double distance0 = JxlButteraugliResultGetDistance(result.get(), 8.0);

  JxlButteraugliApiSetHFAsymmetry(api.get(), 2.0f);
  result.reset(JxlButteraugliCompute(api.get(), xsize, ysize, &pixel_format,
                                     orig_pixels.data(), orig_pixels.size(),
                                     &pixel_format, dist_pixels.data(),
                                     dist_pixels.size()));
  double distance1 = JxlButteraugliResultGetDistance(result.get(), 8.0);

  EXPECT_NE(distance0, distance1);

  JxlButteraugliApiSetIntensityTarget(api.get(), 80.0f);
  result.reset(JxlButteraugliCompute(api.get(), xsize, ysize, &pixel_format,
                                     orig_pixels.data(), orig_pixels.size(),
                                     &pixel_format, dist_pixels.data(),
                                     dist_pixels.size()));
  double distance2 = JxlButteraugliResultGetDistance(result.get(), 8.0);

  EXPECT_NE(distance1, distance2);
}
