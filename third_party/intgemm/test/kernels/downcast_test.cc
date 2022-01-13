#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstddef>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_downcast32to8_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int8_t);

  AlignedVector<int32_t> input(LENGTH);
  AlignedVector<int8_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int32_t>(-LENGTH / 2));

  *output.template as<vi>() = kernels::downcast32to8(
    input.template as<vi>()[0], input.template as<vi>()[1],
    input.template as<vi>()[2], input.template as<vi>()[3]);
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int8_t(input[i]));
}

template INTGEMM_SSE2 void kernel_downcast32to8_test<CPUType::SSE2>();
KERNEL_TEST_CASE("downcast32to8 SSE2") { return kernel_downcast32to8_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_downcast32to8_test<CPUType::AVX2>();
KERNEL_TEST_CASE("downcast32to8 AVX2") { return kernel_downcast32to8_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_downcast32to8_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("downcast32to8 AVX512BW") { return kernel_downcast32to8_test<CPUType::AVX512BW>(); }
#endif

template <CPUType CPUType_>
void kernel_downcast32to16_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int16_t);

  AlignedVector<int32_t> input(LENGTH);
  AlignedVector<int16_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int32_t>(-LENGTH / 2));

  *output.template as<vi>() = kernels::downcast32to16(
    input.template as<vi>()[0], input.template as<vi>()[1]);
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int16_t(input[i]));
}

template INTGEMM_SSE2 void kernel_downcast32to16_test<CPUType::SSE2>();
KERNEL_TEST_CASE("downcast32to16 SSE2") { return kernel_downcast32to16_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_downcast32to16_test<CPUType::AVX2>();
KERNEL_TEST_CASE("downcast32to16 AVX2") { return kernel_downcast32to16_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_downcast32to16_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("downcast32to16 AVX512BW") { return kernel_downcast32to16_test<CPUType::AVX512BW>(); }
#endif

template <CPUType CPUType_>
void kernel_downcast16to8_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int8_t);

  AlignedVector<int16_t> input(LENGTH);
  AlignedVector<int8_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int16_t>(-LENGTH / 2));

  *output.template as<vi>() = kernels::downcast16to8(
    input.template as<vi>()[0], input.template as<vi>()[1]);
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int8_t(input[i]));
}

template INTGEMM_SSE2 void kernel_downcast16to8_test<CPUType::SSE2>();
KERNEL_TEST_CASE("downcast16to8 SSE2") { return kernel_downcast16to8_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_downcast16to8_test<CPUType::AVX2>();
KERNEL_TEST_CASE("downcast16to8 AVX2") { return kernel_downcast16to8_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_downcast16to8_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("downcast16to8 AVX512BW") { return kernel_downcast16to8_test<CPUType::AVX512BW>(); }
#endif

}
