#include "test.h"
#include "../intgemm/aligned.h"
#include "../intgemm/avx2_gemm.h"
#include "../intgemm/avx512_gemm.h"
#include "../intgemm/sse2_gemm.h"
#include "../intgemm/ssse3_gemm.h"
#include "../intgemm/stats.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace intgemm {
namespace {

void QuantizeRef(const float *input, int16_t *output, float quant_mult, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    float value = roundf(input[i] * quant_mult);
    value = std::max(-32768.0f, value);
    value = std::min(32767.0f, value);
    // float should be exact in this range.
    output[i] = static_cast<int16_t>(value);
  }
}

void QuantizeRef(const float *input, int8_t *output, float quant_mult, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    float value = roundf(input[i] * quant_mult);
    value = std::max(-127.0f, value);
    value = std::min(127.0f, value);
    output[i] = static_cast<int8_t>(value);
  }
}

MeanStd VectorMeanStd(AlignedVector<float>& vals, int num_items, bool absolute) {
  float normal_sums = 0;
  float squares_sum = 0;
  if (absolute) {
    std::for_each(vals.begin(), vals.end(), [&] (float n) {normal_sums+=std::abs(n);});
  } else {
    std::for_each(vals.begin(), vals.end(), [&] (float n) {normal_sums+=n;});
  }
  std::for_each(vals.begin(), vals.end(), [&] (float n) {squares_sum+=n*n;});

  MeanStd ret;
  ret.mean = normal_sums/num_items;
  ret.stddev = std::sqrt((squares_sum/num_items) - (ret.mean*ret.mean));
  return ret;
}

template <MeanStd (*Backend) (const float *, const float *, bool)>
void testVectorMeanStd(int num_items, bool absolute=false) {
  std::mt19937 gen;
  std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
  AlignedVector<float> inputVec(num_items);

  for (auto&& it : inputVec) {
    it = dist(gen);
  }

  MeanStd reference = VectorMeanStd(inputVec, num_items, absolute);
  MeanStd fast = Backend(inputVec.begin(), inputVec.end(), absolute);

  float meanDifference = std::fabs(reference.mean - fast.mean);
  float stdDifference = std::fabs(reference.stddev - fast.stddev);
  float eps = 0.00002f; //Accumulating horizontal sums can lead to errors.

  CHECK_MESSAGE(meanDifference <= eps, "Items: " << num_items << " Absolute: " << absolute << " Reference mean: " << reference.mean << " actual: " << fast.mean);
  CHECK_MESSAGE(stdDifference <= eps, "Items: " << num_items << " Absolute: " << absolute << " Reference mean: " << reference.stddev << " actual: " << fast.stddev);

}

template <class I> bool IsOff(float from, I ref, I test) {
  if (ref == test) return false;
  if (ref - test > 1 && test - ref > 1) return true;
  float off_test = std::fabs(static_cast<float>(test) - from);
  float off_ref = std::fabs(static_cast<float>(ref) - from);
  // Allow 0.5 to round either way.
  if (off_test > 0.49 && off_test < 0.51 && off_ref > 0.49 && off_ref < 0.51) return false;
  return true;
}

template <class Backend> bool Test(const float *input_unaligned, float quant_mult, std::size_t size) {
  using Integer = typename Backend::Integer;
  bool success = true;
  AlignedVector<float> input(size);
  std::memcpy(input.begin(), input_unaligned, sizeof(float) * size);

  AlignedVector<Integer> ref(size);
  AlignedVector<Integer> test(size);
  QuantizeRef(input.begin(), ref.begin(), quant_mult, static_cast<Index>(size));
  Backend::Quantize(input.begin(), test.begin(), quant_mult, static_cast<Index>(size));
  for (std::size_t i = 0; i < size; ++i) {
    if (IsOff(input[i] * quant_mult, ref[i], test[i])) {
      UNSCOPED_INFO("Error at " << i << " from " << input[i] << '*' << quant_mult << '=' << (input[i]*quant_mult) << " ref = " << static_cast<int>(ref[i]) << " test = " << static_cast<int>(test[i]));
      success = false;
    }
  }
  return success;
}

template <class Backend> void TestMany(std::size_t grow) {
  float input[33] = {
    0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f,
    14.f, 15.f, 16.f, 17.f, 18.f, 19.f, 20.f, 21.f, 22.f, 23.f, 24.f, 25.f,
    26.f, 27.f, 28.f, 29.f, 30.f, 31.f, 32.f};
  float corners[33] = {
    -32769.f, -32768.f, -32767.f, -129.f, -128.f, -127.f, -1.f, 0.f, 1.f,
    126.f, 127.f, 128.f, 129.f, 32766.f, 32768.f, 32769.f, -1.9f, -1.5f, -1.1f,
    -1.f, -0.9f, -0.5f, -0.1f, 0.0f, 0.1f, 0.5f, 0.9f, 1.0f, 1.1f, 1.5f, 1.9f,
    16056.8f, 2.5f};
  for (std::size_t len = 0; len <= 33; len += grow) {
    CHECK(Test<Backend>(input, 1.0f, len));
    CHECK(Test<Backend>(input, 32.0f, len));
    CHECK(Test<Backend>(corners, 1.0f, len));
    CHECK(Test<Backend>(corners, -1.0f, len));
    CHECK(Test<Backend>(corners, -0.49f, len));
  }
}

TEST_CASE ("Quantize SSE2", "[quantize]") {
  if (kCPU < CPUType::SSE2) return;
  TestMany<SSE2::Kernels16>(8);
}

TEST_CASE ("Quantize SSSE3", "[quantize]") {
  if (kCPU < CPUType::SSSE3) return;
  TestMany<SSSE3::Kernels8>(1);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE ("Quantize AVX2", "[quantize]") {
  if (kCPU < CPUType::AVX2) return;
  TestMany<AVX2::Kernels8>(1);
  TestMany<AVX2::Kernels16>(16);
}
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE ("Quantize AVX512", "[quantize]") {
  if (kCPU < CPUType::AVX512BW) return;
  TestMany<AVX512BW::Kernels8>(1);
  TestMany<AVX512BW::Kernels16>(16);
}
#endif

TEST_CASE("QuantizeStd SSSE3", "[VectorMeanStd]") {
  if (kCPU < CPUType::SSSE3) return;
  testVectorMeanStd<SSE2::VectorMeanStd>(64);
  testVectorMeanStd<SSE2::VectorMeanStd>(64, true);
  testVectorMeanStd<SSE2::VectorMeanStd>(256);
  testVectorMeanStd<SSE2::VectorMeanStd>(256, true);
  testVectorMeanStd<SSE2::VectorMeanStd>(2048);
  testVectorMeanStd<SSE2::VectorMeanStd>(2048, true);
  testVectorMeanStd<SSE2::VectorMeanStd>(65536);
  testVectorMeanStd<SSE2::VectorMeanStd>(65536, true);
  testVectorMeanStd<SSE2::VectorMeanStd>(81920);
  testVectorMeanStd<SSE2::VectorMeanStd>(81920, true);
  testVectorMeanStd<SSE2::VectorMeanStd>(120832);
  testVectorMeanStd<SSE2::VectorMeanStd>(120832, true);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("QuantizeStd AVX2", "[VectorMeanStd]") {
  if (kCPU < CPUType::AVX2) return;
  testVectorMeanStd<AVX2::VectorMeanStd>(64);
  testVectorMeanStd<AVX2::VectorMeanStd>(64, true);
  testVectorMeanStd<AVX2::VectorMeanStd>(256);
  testVectorMeanStd<AVX2::VectorMeanStd>(256, true);
  testVectorMeanStd<AVX2::VectorMeanStd>(2048);
  testVectorMeanStd<AVX2::VectorMeanStd>(2048, true);
  testVectorMeanStd<AVX2::VectorMeanStd>(65536);
  testVectorMeanStd<AVX2::VectorMeanStd>(65536, true);
  testVectorMeanStd<AVX2::VectorMeanStd>(81920);
  testVectorMeanStd<AVX2::VectorMeanStd>(81920, true);
  testVectorMeanStd<AVX2::VectorMeanStd>(120832);
  testVectorMeanStd<AVX2::VectorMeanStd>(120832, true);
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("QuantizeStd AVX512BW", "[VectorMeanStd]") {
  if (kCPU < CPUType::AVX512BW) return;
  testVectorMeanStd<AVX512BW::VectorMeanStd>(64);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(64, true);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(256);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(256, true);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(2048);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(2048, true);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(65536);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(65536, true);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(81920);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(81920, true);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(120832);
  testVectorMeanStd<AVX512BW::VectorMeanStd>(120832, true);
}
#endif

} // namespace
} // namespace intgemm
