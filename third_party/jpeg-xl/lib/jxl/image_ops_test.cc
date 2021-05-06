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

#include "lib/jxl/image_ops.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <random>
#include <utility>

#include "gtest/gtest.h"
#include "lib/jxl/image.h"
#include "lib/jxl/image_test_utils.h"

namespace jxl {
namespace {

template <typename T>
void TestPacked(const size_t xsize, const size_t ysize) {
  Plane<T> image1(xsize, ysize);
  RandomFillImage(&image1);
  const std::vector<T>& packed = PackedFromImage(image1);
  const Plane<T>& image2 = ImageFromPacked(packed, xsize, ysize);
  EXPECT_TRUE(SamePixels(image1, image2));
}

TEST(ImageTest, TestPacked) {
  TestPacked<uint8_t>(1, 1);
  TestPacked<uint8_t>(7, 1);
  TestPacked<uint8_t>(1, 7);

  TestPacked<int16_t>(1, 1);
  TestPacked<int16_t>(7, 1);
  TestPacked<int16_t>(1, 7);

  TestPacked<uint16_t>(1, 1);
  TestPacked<uint16_t>(7, 1);
  TestPacked<uint16_t>(1, 7);

  TestPacked<float>(1, 1);
  TestPacked<float>(7, 1);
  TestPacked<float>(1, 7);
}

// Ensure entire payload is readable/writable for various size/offset combos.
TEST(ImageTest, TestAllocator) {
  std::mt19937 rng(129);
  const size_t k32 = 32;
  const size_t kAlign = CacheAligned::kAlignment;
  for (size_t size : {k32 * 1, k32 * 2, k32 * 3, k32 * 4, k32 * 5,
                      CacheAligned::kAlias, 2 * CacheAligned::kAlias + 4}) {
    for (size_t offset = 0; offset <= CacheAligned::kAlias; offset += kAlign) {
      uint8_t* bytes =
          static_cast<uint8_t*>(CacheAligned::Allocate(size, offset));
      JXL_CHECK(reinterpret_cast<uintptr_t>(bytes) % kAlign == 0);
      // Ensure we can write/read the last byte. Use RNG to fool the compiler
      // into thinking the write is necessary.
      memset(bytes, 0, size);
      bytes[size - 1] = 1;  // greatest element
      std::uniform_int_distribution<uint32_t> dist(0, size - 1);
      uint32_t pos = dist(rng);  // random but != greatest
      while (pos == size - 1) {
        pos = dist(rng);
      }
      JXL_CHECK(bytes[pos] < bytes[size - 1]);

      CacheAligned::Free(bytes);
    }
  }
}

template <typename T>
void TestFillImpl(Image3<T>* img, const char* layout) {
  FillImage(T(1), img);
  for (size_t y = 0; y < img->ysize(); ++y) {
    for (size_t c = 0; c < 3; ++c) {
      T* JXL_RESTRICT row = img->PlaneRow(c, y);
      for (size_t x = 0; x < img->xsize(); ++x) {
        if (row[x] != T(1)) {
          printf("Not 1 at c=%zu %zu, %zu (%zu x %zu) (%s)\n", c, x, y,
                 img->xsize(), img->ysize(), layout);
          abort();
        }
        row[x] = T(2);
      }
    }
  }

  // Same for ZeroFillImage and swapped c/y loop ordering.
  ZeroFillImage(img);
  for (size_t c = 0; c < 3; ++c) {
    for (size_t y = 0; y < img->ysize(); ++y) {
      T* JXL_RESTRICT row = img->PlaneRow(c, y);
      for (size_t x = 0; x < img->xsize(); ++x) {
        if (row[x] != T(0)) {
          printf("Not 0 at c=%zu %zu, %zu (%zu x %zu) (%s)\n", c, x, y,
                 img->xsize(), img->ysize(), layout);
          abort();
        }
        row[x] = T(3);
      }
    }
  }
}

template <typename T>
void TestFillT() {
  for (uint32_t xsize : {0, 1, 15, 16, 31, 32}) {
    for (uint32_t ysize : {0, 1, 15, 16, 31, 32}) {
      Image3<T> image(xsize, ysize);
      TestFillImpl(&image, "size ctor");

      Image3<T> planar(Plane<T>(xsize, ysize), Plane<T>(xsize, ysize),
                       Plane<T>(xsize, ysize));
      TestFillImpl(&planar, "planar");
    }
  }
}

// Ensure y/c/x and c/y/x loops visit pixels no more than once.
TEST(ImageTest, TestFill) {
  TestFillT<uint8_t>();
  TestFillT<int16_t>();
  TestFillT<float>();
  TestFillT<double>();
}

}  // namespace
}  // namespace jxl
