// This test triggers an internal compiler error in gcc 5.
#if defined(__OPTIMIZE__) || defined(__clang__) || defined(__INTEL_COMPILER) || !defined(__GNUC__) || (__GNUC__ != 5)
#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstdint>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_upcast8to16_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int8_t);

  AlignedVector<int8_t> input(LENGTH);
  AlignedVector<int16_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int8_t>(-LENGTH / 2));

  auto result = kernels::upcast8to16(*input.template as<vi>());
  output.template as<vi>()[0] = result.first;
  output.template as<vi>()[1] = result.second;

  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int16_t(input[i]));
}

template INTGEMM_SSE2 void kernel_upcast8to16_test<CPUType::SSE2>();
KERNEL_TEST_CASE("upcast8to16 SSE2") { return kernel_upcast8to16_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_upcast8to16_test<CPUType::AVX2>();
KERNEL_TEST_CASE("upcast8to16 AVX2") { return kernel_upcast8to16_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_upcast8to16_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("upcast8to16 AVX512BW") { return kernel_upcast8to16_test<CPUType::AVX512BW>(); }
#endif

template <CPUType CPUType_>
void kernel_upcast16to32_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int16_t);

  AlignedVector<int16_t> input(LENGTH);
  AlignedVector<int32_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int16_t>(-LENGTH / 2));

  auto result = kernels::upcast16to32(*input.template as<vi>());
  output.template as<vi>()[0] = result.first;
  output.template as<vi>()[1] = result.second;

  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int32_t(input[i]));
}

template INTGEMM_SSE2 void kernel_upcast16to32_test<CPUType::SSE2>();
KERNEL_TEST_CASE("upcast16to32 SSE2") { return kernel_upcast16to32_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_upcast16to32_test<CPUType::AVX2>();
KERNEL_TEST_CASE("upcast16to32 AVX2") { return kernel_upcast16to32_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_upcast16to32_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("upcast16to32 AVX512BW") { return kernel_upcast16to32_test<CPUType::AVX512BW>(); }
#endif


template <CPUType CPUType_>
void kernel_upcast8to32_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int8_t);

  AlignedVector<int8_t> input(LENGTH);
  AlignedVector<int32_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int8_t>(-LENGTH / 2));

  auto result = kernels::upcast8to32(*input.template as<vi>());
  output.template as<vi>()[0] = result.first;
  output.template as<vi>()[1] = result.second;
  output.template as<vi>()[2] = result.third;
  output.template as<vi>()[3] = result.fourth;

  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int32_t(input[i]));
}

template INTGEMM_SSE2 void kernel_upcast8to32_test<CPUType::SSE2>();
KERNEL_TEST_CASE("upcast8to32 SSE2") { return kernel_upcast8to32_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_upcast8to32_test<CPUType::AVX2>();
KERNEL_TEST_CASE("upcast8to32 AVX2") { return kernel_upcast8to32_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_upcast8to32_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("upcast8to32 AVX512BW") { return kernel_upcast8to32_test<CPUType::AVX512BW>(); }
#endif

}
#endif
