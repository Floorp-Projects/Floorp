// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/image_ops.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/printf_macros.h"
#include "lib/jxl/base/rect.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/image.h"
#include "lib/jxl/test_memory_manager.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

template <typename T>
void TestFillImpl(Image3<T>* img, const char* layout) {
  FillImage(static_cast<T>(1), img);
  for (size_t y = 0; y < img->ysize(); ++y) {
    for (size_t c = 0; c < 3; ++c) {
      T* JXL_RESTRICT row = img->PlaneRow(c, y);
      for (size_t x = 0; x < img->xsize(); ++x) {
        if (row[x] != static_cast<T>(1)) {
          printf("Not 1 at c=%" PRIuS " %" PRIuS ", %" PRIuS " (%" PRIuS
                 " x %" PRIuS ") (%s)\n",
                 c, x, y, img->xsize(), img->ysize(), layout);
          abort();
        }
        row[x] = static_cast<T>(2);
      }
    }
  }

  // Same for ZeroFillImage and swapped c/y loop ordering.
  ZeroFillImage(img);
  for (size_t c = 0; c < 3; ++c) {
    for (size_t y = 0; y < img->ysize(); ++y) {
      T* JXL_RESTRICT row = img->PlaneRow(c, y);
      for (size_t x = 0; x < img->xsize(); ++x) {
        if (row[x] != static_cast<T>(0)) {
          printf("Not 0 at c=%" PRIuS " %" PRIuS ", %" PRIuS " (%" PRIuS
                 " x %" PRIuS ") (%s)\n",
                 c, x, y, img->xsize(), img->ysize(), layout);
          abort();
        }
        row[x] = static_cast<T>(3);
      }
    }
  }
}

template <typename T>
void TestFillT() {
  for (uint32_t xsize : {0, 1, 15, 16, 31, 32}) {
    for (uint32_t ysize : {0, 1, 15, 16, 31, 32}) {
      JXL_ASSIGN_OR_DIE(
          Image3<T> image,
          Image3<T>::Create(jxl::test::MemoryManager(), xsize, ysize));
      TestFillImpl(&image, "size ctor");
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

TEST(ImageTest, CopyImageToWithPaddingTest) {
  JXL_ASSIGN_OR_DIE(
      Plane<uint32_t> src,
      Plane<uint32_t>::Create(jxl::test::MemoryManager(), 100, 61));
  for (size_t y = 0; y < src.ysize(); y++) {
    for (size_t x = 0; x < src.xsize(); x++) {
      src.Row(y)[x] = x * 1000 + y;
    }
  }
  Rect src_rect(10, 20, 30, 40);
  EXPECT_TRUE(src_rect.IsInside(src));

  JXL_ASSIGN_OR_DIE(
      Plane<uint32_t> dst,
      Plane<uint32_t>::Create(jxl::test::MemoryManager(), 60, 50));
  FillImage(0u, &dst);
  Rect dst_rect(20, 5, 30, 40);
  EXPECT_TRUE(dst_rect.IsInside(dst));

  CopyImageToWithPadding(src_rect, src, /*padding=*/2, dst_rect, &dst);

  // ysize is + 3 instead of + 4 because we are at the y image boundary on the
  // source image.
  Rect padded_dst_rect(20 - 2, 5 - 2, 30 + 4, 40 + 3);
  for (size_t y = 0; y < dst.ysize(); y++) {
    for (size_t x = 0; x < dst.xsize(); x++) {
      if (Rect(x, y, 1, 1).IsInside(padded_dst_rect)) {
        EXPECT_EQ((x - dst_rect.x0() + src_rect.x0()) * 1000 +
                      (y - dst_rect.y0() + src_rect.y0()),
                  dst.Row(y)[x]);
      } else {
        EXPECT_EQ(0u, dst.Row(y)[x]);
      }
    }
  }
}

}  // namespace
}  // namespace jxl
