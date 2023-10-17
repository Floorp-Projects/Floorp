// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/butteraugli/butteraugli.h"

#include <jxl/types.h>
#include <stddef.h>

#include <algorithm>
#include <utility>

#include "lib/extras/metrics.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/enc_external_image.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/test_image.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using extras::PackedImage;
using extras::PackedPixelFile;
using test::TestImage;

Image3F SinglePixelImage(float red, float green, float blue) {
  Image3F img(1, 1);
  img.PlaneRow(0, 0)[0] = red;
  img.PlaneRow(1, 0)[0] = green;
  img.PlaneRow(2, 0)[0] = blue;
  return img;
}

Image3F GetColorImage(const PackedPixelFile& ppf) {
  JXL_CHECK(!ppf.frames.empty());
  const PackedImage& image = ppf.frames[0].color;
  const JxlPixelFormat& format = image.format;
  const uint8_t* pixels = reinterpret_cast<const uint8_t*>(image.pixels());
  Image3F color(image.xsize, image.ysize);
  for (size_t c = 0; c < format.num_channels; ++c) {
    JXL_CHECK(
        ConvertFromExternal(Span<const uint8_t>(pixels, image.pixels_size),
                            image.xsize, image.ysize, ppf.info.bits_per_sample,
                            format, c, nullptr, &color.Plane(c)));
  }
  return color;
}

void AddUniformNoise(Image3F* img, float d, size_t seed) {
  Rng generator(seed);
  for (size_t y = 0; y < img->ysize(); ++y) {
    for (int c = 0; c < 3; ++c) {
      for (size_t x = 0; x < img->xsize(); ++x) {
        img->PlaneRow(c, y)[x] += generator.UniformF(-d, d);
      }
    }
  }
}

void AddEdge(Image3F* img, float d, size_t x0, size_t y0) {
  const size_t h = std::min<size_t>(img->ysize() - y0, 100);
  const size_t w = std::min<size_t>(img->xsize() - x0, 5);
  for (size_t dy = 0; dy < h; ++dy) {
    for (size_t dx = 0; dx < w; ++dx) {
      img->PlaneRow(1, y0 + dy)[x0 + dx] += d;
    }
  }
}

TEST(ButteraugliInPlaceTest, SinglePixel) {
  Image3F rgb0 = SinglePixelImage(0.5f, 0.5f, 0.5f);
  Image3F rgb1 = SinglePixelImage(0.5f, 0.49f, 0.5f);
  ButteraugliParams ba;
  ImageF diffmap;
  double diffval;
  EXPECT_TRUE(ButteraugliInterface(rgb0, rgb1, ba, diffmap, diffval));
  EXPECT_NEAR(diffval, 2.5, 0.5);
  ImageF diffmap2;
  double diffval2;
  EXPECT_TRUE(ButteraugliInterfaceInPlace(std::move(rgb0), std::move(rgb1), ba,
                                          diffmap2, diffval2));
  EXPECT_NEAR(diffval, diffval2, 1e-10);
}

TEST(ButteraugliInPlaceTest, LargeImage) {
  const size_t xsize = 1024;
  const size_t ysize = 1024;
  TestImage img;
  img.SetDimensions(xsize, ysize).AddFrame().RandomFill(777);
  Image3F rgb0 = GetColorImage(img.ppf());
  Image3F rgb1(xsize, ysize);
  CopyImageTo(rgb0, &rgb1);
  AddUniformNoise(&rgb1, 0.02f, 7777);
  AddEdge(&rgb1, 0.1f, xsize / 2, xsize / 2);
  ButteraugliParams ba;
  ImageF diffmap;
  double diffval;
  EXPECT_TRUE(ButteraugliInterface(rgb0, rgb1, ba, diffmap, diffval));
  double distp = ComputeDistanceP(diffmap, ba, 3.0);
  EXPECT_NEAR(diffval, 4.0, 0.5);
  EXPECT_NEAR(distp, 1.5, 0.5);
  ImageF diffmap2;
  double diffval2;
  EXPECT_TRUE(ButteraugliInterfaceInPlace(std::move(rgb0), std::move(rgb1), ba,
                                          diffmap2, diffval2));
  double distp2 = ComputeDistanceP(diffmap2, ba, 3.0);
  EXPECT_NEAR(diffval, diffval2, 1e-10);
  EXPECT_NEAR(distp, distp2, 1e-7);
}

}  // namespace
}  // namespace jxl
