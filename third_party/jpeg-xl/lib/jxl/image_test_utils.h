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

#ifndef LIB_JXL_IMAGE_TEST_UTILS_H_
#define LIB_JXL_IMAGE_TEST_UTILS_H_

#include <stddef.h>

#include <cmath>
#include <limits>
#include <random>

#include "gtest/gtest.h"
#include "lib/jxl/base/compiler_specific.h"
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
    ADD_FAILURE() << "requsted rectangle is not fully inside the image";
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
// absolute errors and/or small relative errors. Returns max_relative.
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
      fprintf(stderr, "c=%zu: max +/- %E exceeds +/- %.2E\n", c, max_l1,
              threshold_l1);
    } else {
      fprintf(stderr, "c=%zu: max +/- %E, x %E exceeds +/- %.2E, x %.2E\n", c,
              max_l1, max_relative, threshold_l1, threshold_relative);
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

// Generator for independent, uniformly distributed integers [0, max].
template <typename T, typename Random>
class GeneratorRandom {
 public:
  GeneratorRandom(Random* rng, const T max) : rng_(*rng), dist_(0, max) {}

  GeneratorRandom(Random* rng, const T min, const T max)
      : rng_(*rng), dist_(min, max) {}

  T operator()(const size_t x, const size_t y, const int c) const {
    return dist_(rng_);
  }

 private:
  Random& rng_;
  mutable std::uniform_int_distribution<> dist_;
};

template <typename Random>
class GeneratorRandom<float, Random> {
 public:
  GeneratorRandom(Random* rng, const float max)
      : rng_(*rng), dist_(0.0f, max) {}

  GeneratorRandom(Random* rng, const float min, const float max)
      : rng_(*rng), dist_(min, max) {}

  float operator()(const size_t x, const size_t y, const int c) const {
    return dist_(rng_);
  }

 private:
  Random& rng_;
  mutable std::uniform_real_distribution<float> dist_;
};

template <typename Random>
class GeneratorRandom<double, Random> {
 public:
  GeneratorRandom(Random* rng, const double max)
      : rng_(*rng), dist_(0.0, max) {}

  GeneratorRandom(Random* rng, const double min, const double max)
      : rng_(*rng), dist_(min, max) {}

  double operator()(const size_t x, const size_t y, const int c) const {
    return dist_(rng_);
  }

 private:
  Random& rng_;
  mutable std::uniform_real_distribution<> dist_;
};

// Assigns generator(x, y, 0) to each pixel (x, y).
template <class Generator, class Image>
void GenerateImage(const Generator& generator, Image* image) {
  using T = typename Image::T;
  for (size_t y = 0; y < image->ysize(); ++y) {
    T* const JXL_RESTRICT row = image->Row(y);
    for (size_t x = 0; x < image->xsize(); ++x) {
      row[x] = generator(x, y, 0);
    }
  }
}

template <typename T>
void RandomFillImage(Plane<T>* image,
                     const T max = std::numeric_limits<T>::max()) {
  std::mt19937_64 rng(129);
  const GeneratorRandom<T, std::mt19937_64> generator(&rng, max);
  GenerateImage(generator, image);
}

template <typename T>
void RandomFillImage(Plane<T>* image, const T min, const T max,
                     const int seed) {
  std::mt19937_64 rng(seed);
  const GeneratorRandom<T, std::mt19937_64> generator(&rng, min, max);
  GenerateImage(generator, image);
}

// Assigns generator(x, y, c) to each pixel (x, y).
template <class Generator, typename T>
void GenerateImage(const Generator& generator, Image3<T>* image) {
  for (size_t c = 0; c < 3; ++c) {
    for (size_t y = 0; y < image->ysize(); ++y) {
      T* JXL_RESTRICT row = image->PlaneRow(c, y);
      for (size_t x = 0; x < image->xsize(); ++x) {
        row[x] = generator(x, y, c);
      }
    }
  }
}

template <typename T>
void RandomFillImage(Image3<T>* image,
                     const T max = std::numeric_limits<T>::max()) {
  std::mt19937_64 rng(129);
  const GeneratorRandom<T, std::mt19937_64> generator(&rng, max);
  GenerateImage(generator, image);
}

template <typename T>
void RandomFillImage(Image3<T>* image, const T min, const T max,
                     const int seed) {
  std::mt19937_64 rng(seed);
  const GeneratorRandom<T, std::mt19937_64> generator(&rng, min, max);
  GenerateImage(generator, image);
}

}  // namespace jxl

#endif  // LIB_JXL_IMAGE_TEST_UTILS_H_
