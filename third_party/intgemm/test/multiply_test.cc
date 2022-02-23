#include "test.h"
#include "../intgemm/aligned.h"
#include "../intgemm/callbacks.h"
#include "../intgemm/interleave.h"
#include "../intgemm/intgemm.h"
#include "../intgemm/multiply.h"
#include "../intgemm/stats.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>

namespace intgemm {

#ifndef __INTEL_COMPILER
INTGEMM_SSE2
#endif
TEST_CASE("Transpose 16", "[transpose]") {
  if (kCPU < CPUType::SSE2) return;
  const unsigned N = 8;
  AlignedVector<int16_t> input(N * N);
  std::iota(input.begin(), input.end(), static_cast<int16_t>(0));

  AlignedVector<int16_t> ref(N * N);
  references::Transpose(input.begin(), ref.begin(), N, N);

  // Overwrite input.
  __m128i *t = input.as<__m128i>();
  Transpose16InLane(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);

  for (std::size_t i = 0; i < input.size(); ++i) {
  	CHECK_MESSAGE(ref[i] == input[i], "16-bit transpose failure at: " << i << ": " << ref[i] << " != " << input[i]);
  }
}

#ifndef __INTEL_COMPILER
INTGEMM_SSSE3
#endif
TEST_CASE("Transpose 8", "[transpose]") {
  if (kCPU < CPUType::SSSE3) return;
  const unsigned N = 16;
  AlignedVector<int8_t> input(N * N);
  std::iota(input.begin(), input.end(), static_cast<int8_t>(0));

  AlignedVector<int8_t> ref(input.size());
  references::Transpose(input.begin(), ref.begin(), N, N);

  // Overwrite input.
  __m128i *t = input.as<__m128i>();
  Transpose8InLane(t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7], t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15]);

  for (std::size_t i = 0; i < input.size(); ++i) {
    CHECK_MESSAGE(ref[i] == input[i], "8-bit transpose failure at " << i << ": " << (int16_t)ref[i] << " != " << (int16_t)input[i]);
  }
}

template <class Routine> void TestPrepare(Index rows = 32, Index cols = 16) {
  std::mt19937 gen;
  // Go somewhat out of range too.
  std::uniform_real_distribution<float> dist(-129.0, 129.0);
  // Create array.
  AlignedVector<float> input(rows * cols);
  for (auto& it : input) {
    it = dist(gen);
  }

  using Integer = typename Routine::Integer;
  // Call Prepare
  AlignedVector<Integer> test(input.size());
  Routine::PrepareB(input.begin(), test.begin(), 1, rows, cols);

  // Compute reference output.
  AlignedVector<Integer> quantized(input.size());
  Routine::Quantize(input.begin(), quantized.begin(), 1, static_cast<Index>(input.size()));
  AlignedVector<Integer> reference(input.size());
  // Note this won't work for Int8/Int16 generic routines because tile sizes vary.
  references::Rearragement(quantized.begin(), reference.begin(), Routine::kBTileRow, Routine::kBTileCol, rows, cols);
  CHECK_MESSAGE(memcmp(reference.begin(), test.begin(), test.size() * sizeof(Integer)) == 0, Routine::kName << " Mismatch:\n" <<
  	"Quantized Input" << '\n' << PrintMatrix(quantized.begin(), rows, cols) << "Reference" << '\n' <<
  	 PrintMatrix(reference.begin(), rows, cols) << "Routine" << '\n' << PrintMatrix(test.begin(), rows, cols));
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("Prepare AVX512", "[prepare]") {
  if (kCPU < CPUType::AVX512BW) return;
	TestPrepare<AVX512BW::Kernels8>(64, 8);
	TestPrepare<AVX512BW::Kernels8>(256, 32);
    TestPrepare<AVX512BW::Kernels16>(64, 8);
    TestPrepare<AVX512BW::Kernels16>(256, 32);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("Prepare AVX2", "[prepare]") {
  if (kCPU < CPUType::AVX2) return;
  TestPrepare<AVX2::Kernels8>(64, 32);
  TestPrepare<AVX2::Kernels16>(64, 32);
}
#endif

TEST_CASE("Prepare SSSE3", "[prepare]") {
  if (kCPU < CPUType::SSSE3) return;
  TestPrepare<SSSE3::Kernels8>(16, 8);
  TestPrepare<SSSE3::Kernels8>(32, 16);
  TestPrepare<SSSE3::Kernels8>(32, 32);
}

TEST_CASE("Prepare SSE2", "[prepare]") {
  if (kCPU < CPUType::SSE2) return;
  TestPrepare<SSE2::Kernels16>(8, 8);
  TestPrepare<SSE2::Kernels16>(32, 32);
}

template <class Routine> void TestSelectColumnsB(Index rows = 64, Index cols = 16) {
  std::mt19937 gen;
  // Go somewhat out of range too.
  std::uniform_real_distribution<float> dist(-129.0, 129.0);
  AlignedVector<float> input(rows * cols);
  for (auto& it : input) {
    it = dist(gen);
  }
  using Integer = typename Routine::Integer;
  AlignedVector<Integer> prepared(input.size());
  Routine::PrepareB(input.begin(), prepared.begin(), 1, rows, cols);

  const int kSelectCols = 24;
  Index select_cols[kSelectCols];
  std::uniform_int_distribution<Index> col_dist(0, cols - 1);
  for (auto& it : select_cols) {
    it = col_dist(gen);
  }

  AlignedVector<Integer> test(rows * kSelectCols);
  Routine::SelectColumnsB(prepared.begin(), test.begin(), rows, select_cols, select_cols + kSelectCols);

  // Select columns manually in float space.
  AlignedVector<float> selected(rows * kSelectCols);
  for (Index r = 0; r < rows; ++r) {
    for (int c = 0; c < kSelectCols; ++c) {
      assert(c + r * kSelectCols < rows * kSelectCols);
      selected[c + r * kSelectCols] = input[select_cols[c] + r * cols];
    }
  }
  AlignedVector<Integer> ref(rows * kSelectCols);
  Routine::PrepareB(selected.begin(), ref.begin(), 1, rows, kSelectCols);
  CHECK_MESSAGE(memcmp(ref.begin(), test.begin(), sizeof(Integer) * rows * kSelectCols) == 0, "Reference:\n" <<
  	PrintMatrix(ref.begin(), rows, kSelectCols) << PrintMatrix(test.begin(), rows, kSelectCols));
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("SelectColumnsB AVX512", "[select]") {
  if (kCPU < CPUType::AVX512BW) return;
    TestSelectColumnsB<AVX512BW::Kernels8>();
    TestSelectColumnsB<AVX512BW::Kernels16>(256, 256);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("SelectColumnsB AVX2", "[select]") {
  if (kCPU < CPUType::AVX2) return;
  TestSelectColumnsB<AVX2::Kernels8>(256, 256);
  TestSelectColumnsB<AVX2::Kernels16>(256, 256);
}
#endif

TEST_CASE("SelectColumnsB SSSE3", "[select]") {
  if (kCPU < CPUType::SSSE3) return;
  TestSelectColumnsB<SSSE3::Kernels8>();
  TestSelectColumnsB<SSSE3::Kernels8>(256, 256);
}

TEST_CASE("SelectColumnsB SSE2", "[select]") {
  if (kCPU < CPUType::SSE2) return;
  TestSelectColumnsB<SSE2::Kernels16>();
  TestSelectColumnsB<SSE2::Kernels16>(256, 256);
}

template <class Register> void TestMax() {
  Register r = set1_ps<Register>(-2.0);
  for (std::size_t i = 0; i < sizeof(Register) / sizeof(float); ++i) {
    Register c = r;
    reinterpret_cast<float*>(&c)[i] = -1.0;
    CHECK_MESSAGE((MaxFloat32(c) == -1.0), "MaxFloat32 produced " << MaxFloat32(c));
  }
}

TEST_CASE("Max", "[max]") {
  TestMax<__m128>();
}

void CompareMaxAbs(const float *begin, const float *end, float test, std::size_t offset) {
  float largest = std::fabs(*std::max_element(begin, end));
  float smallest = std::fabs(*std::min_element(begin, end));
  largest = std::max(largest, smallest);
  CHECK_MESSAGE(largest == test, "Error: " << largest << " versus " << test << " in length " << (end - begin) << " offset " << offset);
}

template <float (*Backend) (const float *, const float *)> void TestMaxAbsolute() {
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-8.0, 8.0);
  const std::size_t kLengthMax = 65;
  AlignedVector<float> test(kLengthMax);
  for (std::size_t len = 1; len < kLengthMax; ++len) {
    for (std::size_t t = 0; t < len; ++t) {
      // Fill with [-8, 8).
      for (auto& it : test) {
        it = dist(gen);
      }
      CompareMaxAbs(test.begin(), test.begin() + len, Backend(test.begin(), test.begin() + len), t);
      test[t] = -32.0;
      CompareMaxAbs(test.begin(), test.begin() + len, Backend(test.begin(), test.begin() + len), t);
      test[t] = 32.0;
      CompareMaxAbs(test.begin(), test.begin() + len, Backend(test.begin(), test.begin() + len), t);
    }
  }
}

TEST_CASE("MaxAbsolute SSE2", "[max]") {
  if (kCPU < CPUType::SSE2) return;
  TestMaxAbsolute<SSE2::MaxAbsolute>();
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("MaxAbsolute AVX2", "[max]") {
  if (kCPU < CPUType::AVX2) return;
  TestMaxAbsolute<AVX2::MaxAbsolute>();
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("MaxAbsolute AVX512BW", "[max]") {
  if (kCPU < CPUType::AVX512BW) return;
  TestMaxAbsolute<AVX512BW::MaxAbsolute>();
}
#endif

// Based on https://arxiv.org/abs/1705.01991

// Copyright (c) 2017 Microsoft Corporation

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// Compute A*B slowly in floats.

template <class Routine> void TestMultiply(Index A_rows, Index width, Index B_cols,
 float int_tolerance=.1, float float_tolerance=1, float MSE_float_tolerance=0, float MSE_int_tolerance=0) {
  using Integer = typename Routine::Integer;
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }

  float quant_mult = (sizeof(Integer) == 2) ? 1024 : 64;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<Integer> A_prep(A.size());
  AlignedVector<Integer> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);
  OMPParallelWrap<callbacks::UnquantizeAndWrite, Routine>(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndWrite(unquant_mult, test_C.begin()));
  // Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::Sequence(
  //   callbacks::Unquantize(unquant_mult),
  //   callbacks::Write<float>(test_C.begin())
  // ));

  AlignedVector<Integer> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Assuming A is just quantization here.
  references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo&) {
    return sum * unquant_mult;
  });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo&) {
    return static_cast<float>(sum);
  });

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

template <class Routine> void TestMultiplyRelu(Index A_rows, Index width, Index B_cols,
 float int_tolerance=.1, float float_tolerance=1, float MSE_float_tolerance=0, float MSE_int_tolerance=0) {
  using Integer = typename Routine::Integer;
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }

  float quant_mult = (sizeof(Integer) == 2) ? 1024 : 64;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<Integer> A_prep(A.size());
  AlignedVector<Integer> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);
  OMPParallelWrap<callbacks::UnquantizeAndWriteRelu, Routine>(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndWriteRelu(unquant_mult, test_C.begin()));
  // Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::Sequence(
  //   callbacks::Unquantize(unquant_mult),
  //   callbacks::Write<float>(test_C.begin())
  // ));

  AlignedVector<Integer> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Assuming A is just quantization here.
  references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo&) {
    float ret = std::max(0.0f, sum * unquant_mult);
    return ret;
  });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo&) {
    return static_cast<float>(std::max(0.0,sum));
  });

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

//Code duplication may be avoided through some use of variadic templates, as the different WriteC symbols
//Require different number of arguments. I don't think the refactoring is worth it.
template <class Routine> void TestMultiplyBias(Index A_rows, Index width, Index B_cols,
 float int_tolerance = 0.1f, float float_tolerance = 1.0f, float MSE_float_tolerance = 0.0f, float MSE_int_tolerance = 0.0f) {
  using Integer = typename Routine::Integer;
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  AlignedVector<float> bias(B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }
  for (auto& it : bias) {
    it = dist(gen);
  }

  float quant_mult = (sizeof(Integer) == 2) ? 1024 : 64;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<Integer> A_prep(A.size());
  AlignedVector<Integer> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult, bias.begin(), test_C.begin()));

  AlignedVector<Integer> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Assuming A is just quantization here.
  references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return sum * unquant_mult + bias[info.col_idx];
  });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo& info) {
    return static_cast<float>(sum) + bias[info.col_idx];
  });

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

template <class Routine> void TestMultiplyBiasRelu(Index A_rows, Index width, Index B_cols,
 float int_tolerance = 0.1f, float float_tolerance = 1.0f, float MSE_float_tolerance = 0.0f, float MSE_int_tolerance = 0.0f) {
  using Integer = typename Routine::Integer;
  std::ostringstream info;
  info << Routine::kName << "\t" << A_rows << '\t' << width << '\t' << B_cols << '\n';

  // Initialize A and B.
  AlignedVector<float> A(A_rows * width);
  AlignedVector<float> B(width * B_cols);
  AlignedVector<float> bias(B_cols);
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  for (auto& it : A) {
    it = dist(gen);
  }
  for (auto& it : B) {
    it = dist(gen);
  }
  for (auto& it : bias) {
    it = dist(gen);
  }

  float quant_mult = (sizeof(Integer) == 2) ? 1024 : 64;
  float unquant_mult = 1.0f / (quant_mult*quant_mult);

  AlignedVector<Integer> A_prep(A.size());
  AlignedVector<Integer> B_prep(B.size());
  Routine::PrepareA(A.begin(), A_prep.begin(), quant_mult, A_rows, width);
  Routine::PrepareB(B.begin(), B_prep.begin(), quant_mult, width, B_cols);

  AlignedVector<float> test_C(A_rows * B_cols);

  Routine::Multiply(A_prep.begin(), B_prep.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWriteRelu(unquant_mult, bias.begin(), test_C.begin()));

  AlignedVector<Integer> B_quant(B.size());
  Routine::Quantize(B.begin(), B_quant.begin(), quant_mult, static_cast<Index>(B.size()));
  AlignedVector<float> slowint_C(test_C.size());
  // Assuming A is just quantization here.
  references::Multiply(A_prep.begin(), B_quant.begin(), slowint_C.begin(), A_rows, width, B_cols, [&](int32_t sum, const callbacks::OutputBufferInfo& info) {
    return std::max(0.0f, sum * unquant_mult + bias[info.col_idx]);
  });

  AlignedVector<float> float_C(test_C.size());
  references::Multiply(A.begin(), B.begin(), float_C.begin(), A_rows, width, B_cols, [&](double sum, const callbacks::OutputBufferInfo& info) {
    return std::max(0.0f, static_cast<float>(sum) + bias[info.col_idx]);
  });

  CompareMSE(float_C.begin(), slowint_C.begin(), test_C.begin(), test_C.size(), info.str(),
   int_tolerance, float_tolerance, MSE_float_tolerance, MSE_int_tolerance);
}

TEST_CASE ("Multiply SSE2 16bit", "[multiply]") {
  if (kCPU < CPUType::SSE2) return;
  TestMultiply<SSE2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiply<SSE2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiply<SSE2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiply<SSE2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiply<SSE2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiply<SSE2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply SSE2 16bit with relu", "[multiply_relu]") {
  if (kCPU < CPUType::SSE2) return;
  TestMultiplyRelu<SSE2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<SSE2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyRelu<SSE2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<SSE2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<SSE2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<SSE2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply SSE2 16bit with bias", "[biased_multiply]") {
  if (kCPU < CPUType::SSE2) return;
  TestMultiplyBias<SSE2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<SSE2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyBias<SSE2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<SSE2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<SSE2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<SSE2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply SSE2 16bit with bias and relu", "[biased_multiply_relu]") {
  if (kCPU < CPUType::SSE2) return;
  TestMultiplyBiasRelu<SSE2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<SSE2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyBiasRelu<SSE2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<SSE2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<SSE2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<SSE2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply SSSE3 8bit", "[multiply]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiply<SSSE3::Kernels8>(8, 256, 256, 1.2f, 1.2f, 0.064f, 0.026f);
  TestMultiply<SSSE3::Kernels8>(8, 2048, 256, 33, 33, 4.4f, 4.4f);
  TestMultiply<SSSE3::Kernels8>(320, 256, 256, 1.9f, 1.9f, 0.1f, 0.01f);
  TestMultiply<SSSE3::Kernels8>(472, 256, 256, 2.1f, 2.1f, 0.1f, 0.011f);
  TestMultiply<SSSE3::Kernels8>(248, 256, 256, 1.7f, 1.7f, 0.1f, 0.012f);
  TestMultiply<SSSE3::Kernels8>(200, 256, 256, 1.8f, 1.9f, 0.1f, 0.011f);
}

TEST_CASE ("Multiply SSSE3 8bit with relu", "[multiply_relu]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyRelu<SSSE3::Kernels8>(8, 256, 256, 1.2f, 1.2f, 0.064f, 0.026f);
  TestMultiplyRelu<SSSE3::Kernels8>(8, 2048, 256, 33, 33, 4.4f, 4.4f);
  TestMultiplyRelu<SSSE3::Kernels8>(320, 256, 256, 1.9f, 1.9f, 0.1f, 0.01f);
  TestMultiplyRelu<SSSE3::Kernels8>(472, 256, 256, 2.1f, 2.1f, 0.1f, 0.011f);
  TestMultiplyRelu<SSSE3::Kernels8>(248, 256, 256, 1.7f, 1.7f, 0.1f, 0.012f);
  TestMultiplyRelu<SSSE3::Kernels8>(200, 256, 256, 1.8f, 1.9f, 0.1f, 0.011f);
}

TEST_CASE ("Multiply SSSE3 8bit with bias", "[biased_multiply]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyBias<SSSE3::Kernels8>(8, 256, 256, 1.2f, 1.2f, 0.064f, 0.026f);
  TestMultiplyBias<SSSE3::Kernels8>(8, 2048, 256, 33, 33, 4.4f, 4.4f);
  TestMultiplyBias<SSSE3::Kernels8>(320, 256, 256, 1.9f, 1.9f, 0.1f, 0.01f);
  TestMultiplyBias<SSSE3::Kernels8>(472, 256, 256, 2.1f, 2.1f, 0.1f, 0.011f);
  TestMultiplyBias<SSSE3::Kernels8>(248, 256, 256, 1.7f, 1.7f, 0.1f, 0.012f);
  TestMultiplyBias<SSSE3::Kernels8>(200, 256, 256, 1.8f, 1.9f, 0.1f, 0.011f);
}

TEST_CASE ("Multiply SSSE3 8bit with bias and relu", "[biased_multiply_relu]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMultiplyBiasRelu<SSSE3::Kernels8>(8, 256, 256, 1.2f, 1.2f, 0.064f, 0.026f);
  TestMultiplyBiasRelu<SSSE3::Kernels8>(8, 2048, 256, 33, 33, 4.4f, 4.4f);
  TestMultiplyBiasRelu<SSSE3::Kernels8>(320, 256, 256, 1.9f, 1.9f, 0.1f, 0.01f);
  TestMultiplyBiasRelu<SSSE3::Kernels8>(472, 256, 256, 2.1f, 2.1f, 0.1f, 0.011f);
  TestMultiplyBiasRelu<SSSE3::Kernels8>(248, 256, 256, 1.7f, 1.7f, 0.1f, 0.012f);
  TestMultiplyBiasRelu<SSSE3::Kernels8>(200, 256, 256, 1.8f, 1.9f, 0.1f, 0.011f);
}


#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE ("Multiply AVX2 8bit", "[multiply]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiply<AVX2::Kernels8>(8, 256, 256, .1f, 1, 0.1f);
  TestMultiply<AVX2::Kernels8>(8, 2048, 256, 19, 19, 1.8f, 1.8f);
  TestMultiply<AVX2::Kernels8>(320, 256, 256, .1f, 1, 0.1f);
  TestMultiply<AVX2::Kernels8>(472, 256, 256, .1f, 1, 0.1f);
  TestMultiply<AVX2::Kernels8>(248, 256, 256, .1f, 1, 0.1f);
  TestMultiply<AVX2::Kernels8>(200, 256, 256, .1f, 1, 0.1f);
}

TEST_CASE ("Multiply AVX2 8bit with relu", "[multiply_relu]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyRelu<AVX2::Kernels8>(8, 256, 256, .1f, 1, 0.1f);
  TestMultiplyRelu<AVX2::Kernels8>(8, 2048, 256, 19, 19, 1.8f, 1.8f);
  TestMultiplyRelu<AVX2::Kernels8>(320, 256, 256, .1f, 1, 0.1f);
  TestMultiplyRelu<AVX2::Kernels8>(472, 256, 256, .1f, 1, 0.1f);
  TestMultiplyRelu<AVX2::Kernels8>(248, 256, 256, .1f, 1, 0.1f);
  TestMultiplyRelu<AVX2::Kernels8>(200, 256, 256, .1f, 1, 0.1f);
}

TEST_CASE ("Multiply AVX2 8bit with bias", "[biased_multiply]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyBias<AVX2::Kernels8>(8, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBias<AVX2::Kernels8>(8, 2048, 256, 19, 19, 1.8f, 1.8f);
  TestMultiplyBias<AVX2::Kernels8>(320, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBias<AVX2::Kernels8>(472, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBias<AVX2::Kernels8>(248, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBias<AVX2::Kernels8>(200, 256, 256, .1f, 1, 0.1f);
}

TEST_CASE ("Multiply AVX2 8bit with bias and relu", "[biased_multiply_relu]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyBiasRelu<AVX2::Kernels8>(8, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBiasRelu<AVX2::Kernels8>(8, 2048, 256, 19, 19, 1.8f, 1.8f);
  TestMultiplyBiasRelu<AVX2::Kernels8>(320, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBiasRelu<AVX2::Kernels8>(472, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBiasRelu<AVX2::Kernels8>(248, 256, 256, .1f, 1, 0.1f);
  TestMultiplyBiasRelu<AVX2::Kernels8>(200, 256, 256, .1f, 1, 0.1f);
}

TEST_CASE ("Multiply AVX2 16bit", "[multiply]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiply<AVX2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiply<AVX2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiply<AVX2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiply<AVX2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiply<AVX2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiply<AVX2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply AVX2 16bit with relu", "[multiply_relu]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyRelu<AVX2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<AVX2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyRelu<AVX2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<AVX2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<AVX2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyRelu<AVX2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply AVX2 16bit with bias", "[biased_multiply]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyBias<AVX2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<AVX2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyBias<AVX2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<AVX2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<AVX2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBias<AVX2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}

TEST_CASE ("Multiply AVX2 16bit with bias and relu", "[biased_multiply_relu]") {
  if (kCPU < CPUType::AVX2) return;
  TestMultiplyBiasRelu<AVX2::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<AVX2::Kernels16>(8, 2048, 256, .1f, 1, 0.02f);
  TestMultiplyBiasRelu<AVX2::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<AVX2::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<AVX2::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
  TestMultiplyBiasRelu<AVX2::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
  TEST_CASE ("Multiply AVX512 8bit", "[multiply]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiply<AVX512BW::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
    TestMultiply<AVX512BW::Kernels8>(8, 2048, 256, 3.7f, 4, 0.37f, 0.33f);
    TestMultiply<AVX512BW::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
    TestMultiply<AVX512BW::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
    TestMultiply<AVX512BW::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
    TestMultiply<AVX512BW::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
  }

  TEST_CASE ("Multiply AVX512 8bit with relu", "[multiply_relu]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyRelu<AVX512BW::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
    TestMultiplyRelu<AVX512BW::Kernels8>(8, 2048, 256, 3.7f, 4, 0.37f, 0.33f);
    TestMultiplyRelu<AVX512BW::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
    TestMultiplyRelu<AVX512BW::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyRelu<AVX512BW::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyRelu<AVX512BW::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
  }

  TEST_CASE ("Multiply AVX512 8bit with bias", "[biased_multiply]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyBias<AVX512BW::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
    TestMultiplyBias<AVX512BW::Kernels8>(8, 2048, 256, 3.7f, 4, 0.37f, 0.33f);
    TestMultiplyBias<AVX512BW::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
    TestMultiplyBias<AVX512BW::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyBias<AVX512BW::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyBias<AVX512BW::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
  }

  TEST_CASE ("Multiply AVX512 8bit with bias and relu", "[biased_multiply_relu]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(8, 2048, 256, 3.7f, 4, 0.37f, 0.33f);
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
    TestMultiplyBiasRelu<AVX512BW::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
  }

  #ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
    TEST_CASE ("Multiply AVX512VNNI 8bit", "[multiply]") {
      if (kCPU < CPUType::AVX512VNNI) return;
      TestMultiply<AVX512VNNI::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
      TestMultiply<AVX512VNNI::Kernels8>(8, 2048, 256, 0, 0.55f, 0.25f);
      TestMultiply<AVX512VNNI::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
      TestMultiply<AVX512VNNI::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
      TestMultiply<AVX512VNNI::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
      TestMultiply<AVX512VNNI::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
    }

    TEST_CASE ("Multiply AVX512VNNI 8bit with relu", "[multiply_relu]") {
      if (kCPU < CPUType::AVX512VNNI) return;
      TestMultiplyRelu<AVX512VNNI::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
      TestMultiplyRelu<AVX512VNNI::Kernels8>(8, 2048, 256, 0, 0.55f, 0.25f);
      TestMultiplyRelu<AVX512VNNI::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
      TestMultiplyRelu<AVX512VNNI::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyRelu<AVX512VNNI::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyRelu<AVX512VNNI::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
    }

    TEST_CASE ("Multiply AVX512VNNI 8bit with bias", "[biased_multiply]") {
      if (kCPU < CPUType::AVX512VNNI) return;
      TestMultiplyBias<AVX512VNNI::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
      TestMultiplyBias<AVX512VNNI::Kernels8>(8, 2048, 256, 0, 0.55f, 0.25f);
      TestMultiplyBias<AVX512VNNI::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
      TestMultiplyBias<AVX512VNNI::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyBias<AVX512VNNI::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyBias<AVX512VNNI::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
    }

    TEST_CASE ("Multiply AVX512VNNI 8bit with bias and relu", "[biased_multiply_relu]") {
      if (kCPU < CPUType::AVX512VNNI) return;
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(8, 256, 256, 0, 0.25f, 0.062f);
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(8, 2048, 256, 0, 0.55f, 0.25f);
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(320, 256, 256, 0, 0.26f, 0.059f);
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(472, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(248, 256, 256, 0, 0.29f, 0.059f);
      TestMultiplyBiasRelu<AVX512VNNI::Kernels8>(200, 256, 256, 0, 0.28f, 0.06f);
    }
  #endif

  TEST_CASE ("Multiply AVX512 16bit", "[multiply]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiply<AVX512BW::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
    TestMultiply<AVX512BW::Kernels16>(8, 2048, 256, .1f, 1, 0.011f);
    TestMultiply<AVX512BW::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
    TestMultiply<AVX512BW::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
    TestMultiply<AVX512BW::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
    TestMultiply<AVX512BW::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
  }

  TEST_CASE ("Multiply AVX512 16bit with relu", "[multiply_relu]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyRelu<AVX512BW::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
    TestMultiplyRelu<AVX512BW::Kernels16>(8, 2048, 256, .1f, 1, 0.011f);
    TestMultiplyRelu<AVX512BW::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
    TestMultiplyRelu<AVX512BW::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
    TestMultiplyRelu<AVX512BW::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
    TestMultiplyRelu<AVX512BW::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
  }


  TEST_CASE ("Multiply AVX512 16bit with bias", "[biased_multiply]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyBias<AVX512BW::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBias<AVX512BW::Kernels16>(8, 2048, 256, .1f, 1, 0.011f);
    TestMultiplyBias<AVX512BW::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBias<AVX512BW::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBias<AVX512BW::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBias<AVX512BW::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
  }

  TEST_CASE ("Multiply AVX512 16bit with bias and relu", "[biased_multiply_relu]") {
    if (kCPU < CPUType::AVX512BW) return;
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(8, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(8, 2048, 256, .1f, 1, 0.011f);
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(320, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(472, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(248, 256, 256, .1f, 1, 0.01f);
    TestMultiplyBiasRelu<AVX512BW::Kernels16>(200, 256, 256, .1f, 1, 0.01f);
  }
#endif

} // namespace intgemm
