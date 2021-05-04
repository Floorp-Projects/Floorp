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

#include "lib/jxl/convolve.h"

#include <time.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/convolve_test.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
#include <hwy/nanobenchmark.h>
#include <hwy/tests/test_util-inl.h>
#include <random>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"

#ifndef JXL_DEBUG_CONVOLVE
#define JXL_DEBUG_CONVOLVE 0
#endif

#include "lib/jxl/convolve-inl.h"

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

void TestNeighbors() {
  const Neighbors::D d;
  const Neighbors::V v = Iota(d, 0);
  HWY_ALIGN float actual[hwy::kTestMaxVectorSize / sizeof(float)] = {0};

  HWY_ALIGN float first_l1[hwy::kTestMaxVectorSize / sizeof(float)] = {
      0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  Store(Neighbors::FirstL1(v), d, actual);
  const size_t N = Lanes(d);
  EXPECT_EQ(std::vector<float>(first_l1, first_l1 + N),
            std::vector<float>(actual, actual + N));

#if HWY_TARGET != HWY_SCALAR
  HWY_ALIGN float first_l2[hwy::kTestMaxVectorSize / sizeof(float)] = {
      1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  Store(Neighbors::FirstL2(v), d, actual);
  EXPECT_EQ(std::vector<float>(first_l2, first_l2 + N),
            std::vector<float>(actual, actual + N));

  HWY_ALIGN float first_l3[hwy::kTestMaxVectorSize / sizeof(float)] = {
      2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  Store(Neighbors::FirstL3(v), d, actual);
  EXPECT_EQ(std::vector<float>(first_l3, first_l3 + N),
            std::vector<float>(actual, actual + N));
#endif  // HWY_TARGET != HWY_SCALAR
}

template <class Random>
void VerifySymmetric3(const size_t xsize, const size_t ysize, ThreadPool* pool,
                      Random* rng) {
  const Rect rect(0, 0, xsize, ysize);

  ImageF in(xsize, ysize);
  GenerateImage(GeneratorRandom<float, Random>(rng, 1.0f), &in);

  ImageF out_expected(xsize, ysize);
  ImageF out_actual(xsize, ysize);

  const WeightsSymmetric3& weights = WeightsSymmetric3Lowpass();
  Symmetric3(in, rect, weights, pool, &out_expected);
  SlowSymmetric3(in, rect, weights, pool, &out_actual);

  VerifyRelativeError(out_expected, out_actual, 1E-5f, 1E-5f);
}

// Ensures Symmetric and Separable give the same result.
template <class Random>
void VerifySymmetric5(const size_t xsize, const size_t ysize, ThreadPool* pool,
                      Random* rng) {
  const Rect rect(0, 0, xsize, ysize);

  ImageF in(xsize, ysize);
  GenerateImage(GeneratorRandom<float, Random>(rng, 1.0f), &in);

  ImageF out_expected(xsize, ysize);
  ImageF out_actual(xsize, ysize);

  Separable5(in, Rect(in), WeightsSeparable5Lowpass(), pool, &out_expected);
  Symmetric5(in, rect, WeightsSymmetric5Lowpass(), pool, &out_actual);

  VerifyRelativeError(out_expected, out_actual, 1E-5f, 1E-5f);
}

template <class Random>
void VerifySeparable5(const size_t xsize, const size_t ysize, ThreadPool* pool,
                      Random* rng) {
  const Rect rect(0, 0, xsize, ysize);

  ImageF in(xsize, ysize);
  GenerateImage(GeneratorRandom<float, Random>(rng, 1.0f), &in);

  ImageF out_expected(xsize, ysize);
  ImageF out_actual(xsize, ysize);

  const WeightsSeparable5& weights = WeightsSeparable5Lowpass();
  Separable5(in, Rect(in), weights, pool, &out_expected);
  SlowSeparable5(in, rect, weights, pool, &out_actual);

  VerifyRelativeError(out_expected, out_actual, 1E-5f, 1E-5f);
}

template <class Random>
void VerifySeparable7(const size_t xsize, const size_t ysize, ThreadPool* pool,
                      Random* rng) {
  const Rect rect(0, 0, xsize, ysize);

  ImageF in(xsize, ysize);
  GenerateImage(GeneratorRandom<float, Random>(rng, 1.0f), &in);

  ImageF out_expected(xsize, ysize);
  ImageF out_actual(xsize, ysize);

  // Gaussian sigma 1.0
  const WeightsSeparable7 weights = {{HWY_REP4(0.383103f), HWY_REP4(0.241843f),
                                      HWY_REP4(0.060626f), HWY_REP4(0.00598f)},
                                     {HWY_REP4(0.383103f), HWY_REP4(0.241843f),
                                      HWY_REP4(0.060626f), HWY_REP4(0.00598f)}};

  SlowSeparable7(in, rect, weights, pool, &out_expected);
  Separable7(in, Rect(in), weights, pool, &out_actual);

  VerifyRelativeError(out_expected, out_actual, 1E-5f, 1E-5f);
}

// For all xsize/ysize and kernels:
void TestConvolve() {
  TestNeighbors();

  ThreadPoolInternal pool(4);
  pool.Run(kConvolveMaxRadius, 40, ThreadPool::SkipInit(),
           [](const int task, int /*thread*/) {
             const size_t xsize = task;
             std::mt19937_64 rng(129 + 13 * xsize);

             ThreadPool* null_pool = nullptr;
             ThreadPoolInternal pool3(3);
             for (size_t ysize = kConvolveMaxRadius; ysize < 16; ++ysize) {
               JXL_DEBUG(JXL_DEBUG_CONVOLVE,
                         "%zu x %zu (target %d)===============================",
                         xsize, ysize, HWY_TARGET);

               JXL_DEBUG(JXL_DEBUG_CONVOLVE, "Sym3------------------");
               VerifySymmetric3(xsize, ysize, null_pool, &rng);
               VerifySymmetric3(xsize, ysize, &pool3, &rng);

               JXL_DEBUG(JXL_DEBUG_CONVOLVE, "Sym5------------------");
               VerifySymmetric5(xsize, ysize, null_pool, &rng);
               VerifySymmetric5(xsize, ysize, &pool3, &rng);

               JXL_DEBUG(JXL_DEBUG_CONVOLVE, "Sep5------------------");
               VerifySeparable5(xsize, ysize, null_pool, &rng);
               VerifySeparable5(xsize, ysize, &pool3, &rng);

               JXL_DEBUG(JXL_DEBUG_CONVOLVE, "Sep7------------------");
               VerifySeparable7(xsize, ysize, null_pool, &rng);
               VerifySeparable7(xsize, ysize, &pool3, &rng);
             }
           });
}

// Measures durations, verifies results, prints timings. `unpredictable1`
// must have value 1 (unknown to the compiler to prevent elision).
template <class Conv>
void BenchmarkConv(const char* caption, const Conv& conv,
                   const hwy::FuncInput unpredictable1) {
  const size_t kNumInputs = 1;
  const hwy::FuncInput inputs[kNumInputs] = {unpredictable1};
  hwy::Result results[kNumInputs];

  const size_t kDim = 160;  // in+out fit in L2
  ImageF in(kDim, kDim);
  ZeroFillImage(&in);
  in.Row(kDim / 2)[kDim / 2] = unpredictable1;
  ImageF out(kDim, kDim);

  hwy::Params p;
  p.verbose = false;
  p.max_evals = 7;
  p.target_rel_mad = 0.002;
  const size_t num_results = MeasureClosure(
      [&in, &conv, &out](const hwy::FuncInput input) {
        conv(in, &out);
        return out.Row(input)[0];
      },
      inputs, kNumInputs, results, p);
  if (num_results != kNumInputs) {
    fprintf(stderr, "MeasureClosure failed.\n");
  }
  for (size_t i = 0; i < num_results; ++i) {
    const double seconds = static_cast<double>(results[i].ticks) /
                           hwy::platform::InvariantTicksPerSecond();
    printf("%12s: %7.2f MP/s (MAD=%4.2f%%)\n", caption,
           kDim * kDim * 1E-6 / seconds,
           static_cast<double>(results[i].variability) * 100.0);
  }
}

struct ConvSymmetric3 {
  void operator()(const ImageF& in, ImageF* JXL_RESTRICT out) const {
    ThreadPool* null_pool = nullptr;
    Symmetric3(in, Rect(in), WeightsSymmetric3Lowpass(), null_pool, out);
  }
};

struct ConvSeparable5 {
  void operator()(const ImageF& in, ImageF* JXL_RESTRICT out) const {
    ThreadPool* null_pool = nullptr;
    Separable5(in, Rect(in), WeightsSeparable5Lowpass(), null_pool, out);
  }
};

void BenchmarkAll() {
#if 0  // disabled to avoid test timeouts, run manually on demand
  const hwy::FuncInput unpredictable1 = time(nullptr) != 1234;
  BenchmarkConv("Symmetric3", ConvSymmetric3(), unpredictable1);
  BenchmarkConv("Separable5", ConvSeparable5(), unpredictable1);
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {

class ConvolveTest : public hwy::TestWithParamTarget {};
HWY_TARGET_INSTANTIATE_TEST_SUITE_P(ConvolveTest);

HWY_EXPORT_AND_TEST_P(ConvolveTest, TestConvolve);

HWY_EXPORT_AND_TEST_P(ConvolveTest, BenchmarkAll);

}  // namespace jxl
#endif
