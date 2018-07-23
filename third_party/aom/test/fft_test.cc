/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>

#include <algorithm>
#include <complex>
#include <vector>

#include "aom_dsp/fft_common.h"
#include "aom_mem/aom_mem.h"
#if ARCH_X86 || ARCH_X86_64
#include "aom_ports/x86.h"
#endif
#include "av1/common/common.h"
#include "config/aom_dsp_rtcd.h"
#include "test/acm_random.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

typedef void (*tform_fun_t)(const float *input, float *temp, float *output);

// Simple 1D FFT implementation
template <typename InputType>
void fft(const InputType *data, std::complex<float> *result, int n) {
  if (n == 1) {
    result[0] = data[0];
    return;
  }
  std::vector<InputType> temp(n);
  for (int k = 0; k < n / 2; ++k) {
    temp[k] = data[2 * k];
    temp[n / 2 + k] = data[2 * k + 1];
  }
  fft(&temp[0], result, n / 2);
  fft(&temp[n / 2], result + n / 2, n / 2);
  for (int k = 0; k < n / 2; ++k) {
    std::complex<float> w = std::complex<float>((float)cos(2. * PI * k / n),
                                                (float)-sin(2. * PI * k / n));
    std::complex<float> a = result[k];
    std::complex<float> b = result[n / 2 + k];
    result[k] = a + w * b;
    result[n / 2 + k] = a - w * b;
  }
}

void transpose(std::vector<std::complex<float> > *data, int n) {
  for (int y = 0; y < n; ++y) {
    for (int x = y + 1; x < n; ++x) {
      std::swap((*data)[y * n + x], (*data)[x * n + y]);
    }
  }
}

// Simple 2D FFT implementation
template <class InputType>
std::vector<std::complex<float> > fft2d(const InputType *input, int n) {
  std::vector<std::complex<float> > rowfft(n * n);
  std::vector<std::complex<float> > result(n * n);
  for (int y = 0; y < n; ++y) {
    fft(input + y * n, &rowfft[y * n], n);
  }
  transpose(&rowfft, n);
  for (int y = 0; y < n; ++y) {
    fft(&rowfft[y * n], &result[y * n], n);
  }
  transpose(&result, n);
  return result;
}

struct FFTTestArg {
  int n;
  void (*fft)(const float *input, float *temp, float *output);
  int flag;
  FFTTestArg(int n_in, tform_fun_t fft_in, int flag_in)
      : n(n_in), fft(fft_in), flag(flag_in) {}
};

std::ostream &operator<<(std::ostream &os, const FFTTestArg &test_arg) {
  return os << "fft_arg { n:" << test_arg.n << " fft:" << test_arg.fft
            << " flag:" << test_arg.flag << "}";
}

class FFT2DTest : public ::testing::TestWithParam<FFTTestArg> {
 protected:
  void SetUp() {
    int n = GetParam().n;
    input_ = (float *)aom_memalign(32, sizeof(*input_) * n * n);
    temp_ = (float *)aom_memalign(32, sizeof(*temp_) * n * n);
    output_ = (float *)aom_memalign(32, sizeof(*output_) * n * n * 2);
    memset(input_, 0, sizeof(*input_) * n * n);
    memset(temp_, 0, sizeof(*temp_) * n * n);
    memset(output_, 0, sizeof(*output_) * n * n * 2);
#if ARCH_X86 || ARCH_X86_64
    disabled_ = GetParam().flag != 0 && !(x86_simd_caps() & GetParam().flag);
#else
    disabled_ = GetParam().flag != 0;
#endif
  }
  void TearDown() {
    aom_free(input_);
    aom_free(temp_);
    aom_free(output_);
  }
  int disabled_;
  float *input_;
  float *temp_;
  float *output_;
};

TEST_P(FFT2DTest, Correct) {
  if (disabled_) return;

  int n = GetParam().n;
  for (int i = 0; i < n * n; ++i) {
    input_[i] = 1;
    std::vector<std::complex<float> > expected = fft2d<float>(&input_[0], n);
    GetParam().fft(&input_[0], &temp_[0], &output_[0]);
    for (int y = 0; y < n; ++y) {
      for (int x = 0; x < (n / 2) + 1; ++x) {
        EXPECT_NEAR(expected[y * n + x].real(), output_[2 * (y * n + x)], 1e-5);
        EXPECT_NEAR(expected[y * n + x].imag(), output_[2 * (y * n + x) + 1],
                    1e-5);
      }
    }
    input_[i] = 0;
  }
}

TEST_P(FFT2DTest, Benchmark) {
  if (disabled_) return;

  int n = GetParam().n;
  float sum = 0;
  for (int i = 0; i < 1000 * (64 - n); ++i) {
    input_[i % (n * n)] = 1;
    GetParam().fft(&input_[0], &temp_[0], &output_[0]);
    sum += output_[0];
    input_[i % (n * n)] = 0;
  }
}

INSTANTIATE_TEST_CASE_P(
    FFT2DTestC, FFT2DTest,
    ::testing::Values(FFTTestArg(2, aom_fft2x2_float_c, 0),
                      FFTTestArg(4, aom_fft4x4_float_c, 0),
                      FFTTestArg(8, aom_fft8x8_float_c, 0),
                      FFTTestArg(16, aom_fft16x16_float_c, 0),
                      FFTTestArg(32, aom_fft32x32_float_c, 0)));
#if ARCH_X86 || ARCH_X86_64
INSTANTIATE_TEST_CASE_P(
    FFT2DTestSSE2, FFT2DTest,
    ::testing::Values(FFTTestArg(4, aom_fft4x4_float_sse2, HAS_SSE2),
                      FFTTestArg(8, aom_fft8x8_float_sse2, HAS_SSE2),
                      FFTTestArg(16, aom_fft16x16_float_sse2, HAS_SSE2),
                      FFTTestArg(32, aom_fft32x32_float_sse2, HAS_SSE2)));

INSTANTIATE_TEST_CASE_P(
    FFT2DTestAVX2, FFT2DTest,
    ::testing::Values(FFTTestArg(8, aom_fft8x8_float_avx2, HAS_AVX2),
                      FFTTestArg(16, aom_fft16x16_float_avx2, HAS_AVX2),
                      FFTTestArg(32, aom_fft32x32_float_avx2, HAS_AVX2)));
#endif

struct IFFTTestArg {
  int n;
  tform_fun_t ifft;
  int flag;
  IFFTTestArg(int n_in, tform_fun_t ifft_in, int flag_in)
      : n(n_in), ifft(ifft_in), flag(flag_in) {}
};

std::ostream &operator<<(std::ostream &os, const IFFTTestArg &test_arg) {
  return os << "ifft_arg { n:" << test_arg.n << " fft:" << test_arg.ifft
            << " flag:" << test_arg.flag << "}";
}

class IFFT2DTest : public ::testing::TestWithParam<IFFTTestArg> {
 protected:
  void SetUp() {
    int n = GetParam().n;
    input_ = (float *)aom_memalign(32, sizeof(*input_) * n * n * 2);
    temp_ = (float *)aom_memalign(32, sizeof(*temp_) * n * n * 2);
    output_ = (float *)aom_memalign(32, sizeof(*output_) * n * n);
    memset(input_, 0, sizeof(*input_) * n * n * 2);
    memset(temp_, 0, sizeof(*temp_) * n * n * 2);
    memset(output_, 0, sizeof(*output_) * n * n);
#if ARCH_X86 || ARCH_X86_64
    disabled_ = GetParam().flag != 0 && !(x86_simd_caps() & GetParam().flag);
#else
    disabled_ = GetParam().flag != 0;
#endif
  }
  void TearDown() {
    aom_free(input_);
    aom_free(temp_);
    aom_free(output_);
  }
  int disabled_;
  float *input_;
  float *temp_;
  float *output_;
};

TEST_P(IFFT2DTest, Correctness) {
  if (disabled_) return;
  int n = GetParam().n;
  ASSERT_GE(n, 2);
  std::vector<float> expected(n * n);
  std::vector<float> actual(n * n);
  // Do forward transform then invert to make sure we get back expected
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      expected[y * n + x] = 1;
      std::vector<std::complex<float> > input_c = fft2d(&expected[0], n);
      for (int i = 0; i < n * n; ++i) {
        input_[2 * i + 0] = input_c[i].real();
        input_[2 * i + 1] = input_c[i].imag();
      }
      GetParam().ifft(&input_[0], &temp_[0], &output_[0]);

      for (int yy = 0; yy < n; ++yy) {
        for (int xx = 0; xx < n; ++xx) {
          EXPECT_NEAR(expected[yy * n + xx], output_[yy * n + xx] / (n * n),
                      1e-5);
        }
      }
      expected[y * n + x] = 0;
    }
  }
};

TEST_P(IFFT2DTest, Benchmark) {
  if (disabled_) return;
  int n = GetParam().n;
  float sum = 0;
  for (int i = 0; i < 1000 * (64 - n); ++i) {
    input_[i % (n * n)] = 1;
    GetParam().ifft(&input_[0], &temp_[0], &output_[0]);
    sum += output_[0];
    input_[i % (n * n)] = 0;
  }
}
INSTANTIATE_TEST_CASE_P(
    IFFT2DTestC, IFFT2DTest,
    ::testing::Values(IFFTTestArg(2, aom_ifft2x2_float_c, 0),
                      IFFTTestArg(4, aom_ifft4x4_float_c, 0),
                      IFFTTestArg(8, aom_ifft8x8_float_c, 0),
                      IFFTTestArg(16, aom_ifft16x16_float_c, 0),
                      IFFTTestArg(32, aom_ifft32x32_float_c, 0)));
#if ARCH_X86 || ARCH_X86_64
INSTANTIATE_TEST_CASE_P(
    IFFT2DTestSSE2, IFFT2DTest,
    ::testing::Values(IFFTTestArg(4, aom_ifft4x4_float_sse2, HAS_SSE2),
                      IFFTTestArg(8, aom_ifft8x8_float_sse2, HAS_SSE2),
                      IFFTTestArg(16, aom_ifft16x16_float_sse2, HAS_SSE2),
                      IFFTTestArg(32, aom_ifft32x32_float_sse2, HAS_SSE2)));

INSTANTIATE_TEST_CASE_P(
    IFFT2DTestAVX2, IFFT2DTest,
    ::testing::Values(IFFTTestArg(8, aom_ifft8x8_float_avx2, HAS_AVX2),
                      IFFTTestArg(16, aom_ifft16x16_float_avx2, HAS_AVX2),
                      IFFTTestArg(32, aom_ifft32x32_float_avx2, HAS_AVX2)));
#endif
}  // namespace
