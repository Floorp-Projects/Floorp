// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_IMAGE_TEST_UTILS_H_
#define LIB_JXL_IMAGE_TEST_UTILS_H_

#include <stddef.h>

#include <cmath>
#include <limits>

#include "gtest/gtest.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/random.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image.h"

namespace jxl {

template <typename T>
void VerifyEqual(const Plane<T>& expected, const Plane<T>& actual) {
  JXL_CHECK(SameSize(expected, actual));
  for (size_t y = 0; y < expected.ysize(); ++y) {
    const T* const JXL_RESTRICT row_expected = expected.Row(y);
    const T* const JXL_RESTRICT row_actual = actual.Row(y);
    for (size_t x = 0; x < expected.xsize(); ++x) {
      ASSERT_EQ(row_expected[x], row_actual[x]) << x << " " << y;
    }
  }
}

template <typename T>
void VerifyEqual(const Image3<T>& expected, const Image3<T>& actual) {
  for (size_t c = 0; c < 3; ++c) {
    VerifyEqual(expected.Plane(c), actual.Plane(c));
  }
}

template <typename T>
bool SamePixels(const Plane<T>& image1, const Plane<T>& image2,
                const Rect rect) {
  if (!rect.IsInside(image1) || !rect.IsInside(image2)) {
    ADD_FAILURE() << "requested rectangle is not fully inside the image";
    return false;
  }
  size_t mismatches = 0;
  for (size_t y = rect.y0(); y < rect.ysize(); ++y) {
    const T* const JXL_RESTRICT row1 = image1.Row(y);
    const T* const JXL_RESTRICT row2 = image2.Row(y);
    for (size_t x = rect.x0(); x < rect.xsize(); ++x) {
      if (row1[x] != row2[x]) {
        ADD_FAILURE() << "pixel mismatch" << x << ", " << y << ": "
                      << double(row1[x]) << " != " << double(row2[x]);
        if (++mismatches > 4) {
          return false;
        }
      }
    }
  }
  return mismatches == 0;
}

template <typename T>
bool SamePixels(const Plane<T>& image1, const Plane<T>& image2) {
  JXL_CHECK(SameSize(image1, image2));
  return SamePixels(image1, image2, Rect(image1));
}

template <typename T>
bool SamePixels(const Image3<T>& image1, const Image3<T>& image2) {
  JXL_CHECK(SameSize(image1, image2));
  for (size_t c = 0; c < 3; ++c) {
    if (!SamePixels(image1.Plane(c), image2.Plane(c))) {
      return false;
    }
  }
  return true;
}

// Use for floating-point images with fairly large numbers; tolerates small
// absolute errors and/or small relative errors.
template <typename T>
void VerifyRelativeError(const Plane<T>& expected, const Plane<T>& actual,
                         const double threshold_l1,
                         const double threshold_relative,
                         const intptr_t border = 0, const size_t c = 0) {
  JXL_CHECK(SameSize(expected, actual));
  const intptr_t xsize = expected.xsize();
  const intptr_t ysize = expected.ysize();

  // Max over current scanline to give a better idea whether there are
  // systematic errors or just one outlier. Invalid if negative.
  double max_l1 = -1;
  double max_relative = -1;
  bool any_bad = false;
  for (intptr_t y = border; y < ysize - border; ++y) {
    const T* const JXL_RESTRICT row_expected = expected.Row(y);
    const T* const JXL_RESTRICT row_actual = actual.Row(y);
    for (intptr_t x = border; x < xsize - border; ++x) {
      const double l1 = std::abs(row_expected[x] - row_actual[x]);

      // Cannot compute relative, only check/update L1.
      if (std::abs(row_expected[x]) < 1E-10) {
        if (l1 > threshold_l1) {
          any_bad = true;
          max_l1 = std::max(max_l1, l1);
        }
      } else {
        const double relative = l1 / std::abs(double(row_expected[x]));
        if (l1 > threshold_l1 && relative > threshold_relative) {
          // Fails both tolerances => will exit below, update max_*.
          any_bad = true;
          max_l1 = std::max(max_l1, l1);
          max_relative = std::max(max_relative, relative);
        }
      }
    }
  }
  if (any_bad) {
    // Never had a valid relative value, don't print it.
    if (max_relative < 0) {
      fprintf(stderr, "c=%" PRIu64 ": max +/- %E exceeds +/- %.2E\n",
              static_cast<uint64_t>(c), max_l1, threshold_l1);
    } else {
      fprintf(stderr,
              "c=%" PRIu64 ": max +/- %E, x %E exceeds +/- %.2E, x %.2E\n",
              static_cast<uint64_t>(c), max_l1, max_relative, threshold_l1,
              threshold_relative);
    }
    // Dump the expected image and actual image if the region is small enough.
    const intptr_t kMaxTestDumpSize = 16;
    if (xsize <= kMaxTestDumpSize + 2 * border &&
        ysize <= kMaxTestDumpSize + 2 * border) {
      fprintf(stderr, "Expected image:\n");
      for (intptr_t y = border; y < ysize - border; ++y) {
        const T* const JXL_RESTRICT row_expected = expected.Row(y);
        for (intptr_t x = border; x < xsize - border; ++x) {
          fprintf(stderr, "%10lf ", static_cast<double>(row_expected[x]));
        }
        fprintf(stderr, "\n");
      }

      fprintf(stderr, "Actual image:\n");
      for (intptr_t y = border; y < ysize - border; ++y) {
        const T* const JXL_RESTRICT row_expected = expected.Row(y);
        const T* const JXL_RESTRICT row_actual = actual.Row(y);
        for (intptr_t x = border; x < xsize - border; ++x) {
          const double l1 = std::abs(row_expected[x] - row_actual[x]);

          bool bad = l1 > threshold_l1;
          if (row_expected[x] > 1E-10) {
            const double relative = l1 / std::abs(double(row_expected[x]));
            bad &= relative > threshold_relative;
          }
          if (bad) {
            fprintf(stderr, "%10lf ", static_cast<double>(row_actual[x]));
          } else {
            fprintf(stderr, "%10s ", "==");
          }
        }
        fprintf(stderr, "\n");
      }
    }

    // Find first failing x for further debugging.
    for (intptr_t y = border; y < ysize - border; ++y) {
      const T* const JXL_RESTRICT row_expected = expected.Row(y);
      const T* const JXL_RESTRICT row_actual = actual.Row(y);

      for (intptr_t x = border; x < xsize - border; ++x) {
        const double l1 = std::abs(row_expected[x] - row_actual[x]);

        bool bad = l1 > threshold_l1;
        if (row_expected[x] > 1E-10) {
          const double relative = l1 / std::abs(double(row_expected[x]));
          bad &= relative > threshold_relative;
        }
        if (bad) {
          FAIL() << x << ", " << y << " (" << expected.xsize() << " x "
                 << expected.ysize() << ") expected "
                 << static_cast<double>(row_expected[x]) << " actual "
                 << static_cast<double>(row_actual[x]);
        }
      }
    }
    return;  // if any_bad, we should have exited.
  }
}

template <typename T>
void VerifyRelativeError(const Image3<T>& expected, const Image3<T>& actual,
                         const float threshold_l1,
                         const float threshold_relative,
                         const intptr_t border = 0) {
  for (size_t c = 0; c < 3; ++c) {
    VerifyRelativeError(expected.Plane(c), actual.Plane(c), threshold_l1,
                        threshold_relative, border, c);
  }
}

template <typename T, typename U = T>
void GenerateImage(Rng& rng, Plane<T>* image, U begin, U end) {
  for (size_t y = 0; y < image->ysize(); ++y) {
    T* const JXL_RESTRICT row = image->Row(y);
    for (size_t x = 0; x < image->xsize(); ++x) {
      if (std::is_same<T, float>::value || std::is_same<T, double>::value) {
        row[x] = rng.UniformF(begin, end);
      } else if (std::is_signed<T>::value) {
        row[x] = rng.UniformI(begin, end);
      } else {
        row[x] = rng.UniformU(begin, end);
      }
    }
  }
}

template <typename T>
void RandomFillImage(Plane<T>* image, const T begin, const T end,
                     const int seed = 129) {
  Rng rng(seed);
  GenerateImage(rng, image, begin, end);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value>::type RandomFillImage(
    Plane<T>* image) {
  Rng rng(129);
  GenerateImage(rng, image, int64_t(0),
                int64_t(std::numeric_limits<T>::max()) + 1);
}

JXL_INLINE void RandomFillImage(Plane<float>* image) {
  Rng rng(129);
  GenerateImage(rng, image, 0.0f, std::numeric_limits<float>::max());
}

template <typename T, typename U>
void GenerateImage(Rng& rng, Image3<T>* image, U begin, U end) {
  for (size_t c = 0; c < 3; ++c) {
    GenerateImage(rng, &image->Plane(c), begin, end);
  }
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value>::type RandomFillImage(
    Image3<T>* image) {
  Rng rng(129);
  GenerateImage(rng, image, int64_t(0),
                int64_t(std::numeric_limits<T>::max()) + 1);
}

JXL_INLINE void RandomFillImage(Image3F* image) {
  Rng rng(129);
  GenerateImage(rng, image, 0.0f, std::numeric_limits<float>::max());
}

template <typename T, typename U>
void RandomFillImage(Image3<T>* image, const U begin, const U end,
                     const int seed = 129) {
  Rng rng(seed);
  GenerateImage(rng, image, begin, end);
}

}  // namespace jxl

#endif  // LIB_JXL_IMAGE_TEST_UTILS_H_
