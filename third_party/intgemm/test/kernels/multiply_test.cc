#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstdint>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_, typename Type_>
void kernel_multiply_test() {
  if (kCPU < CPUType_)
    return;

  using vec_t = vector_t<CPUType_, Type_>;
  constexpr int VECTOR_LENGTH = sizeof(vec_t) / sizeof(Type_);

  AlignedVector<Type_> input1(VECTOR_LENGTH);
  AlignedVector<Type_> input2(VECTOR_LENGTH);
  AlignedVector<Type_> output(VECTOR_LENGTH);

  std::iota(input1.begin(), input1.end(), static_cast<Type_>(-VECTOR_LENGTH / 2));
  std::iota(input2.begin(), input2.end(), static_cast<Type_>(-VECTOR_LENGTH / 3));

  *output.template as<vec_t>() = kernels::multiply<Type_>(*input1.template as<vec_t>(), *input2.template as<vec_t>());
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == Type_(input1[i] * input2[i]));
}

template INTGEMM_SSE2 void kernel_multiply_test<CPUType::SSE2, int8_t>();
template INTGEMM_SSE2 void kernel_multiply_test<CPUType::SSE2, int16_t>();
template INTGEMM_SSE2 void kernel_multiply_test<CPUType::SSE2, int>();
template INTGEMM_SSE2 void kernel_multiply_test<CPUType::SSE2, float>();
template INTGEMM_SSE2 void kernel_multiply_test<CPUType::SSE2, double>();
KERNEL_TEST_CASE("multiply/int8 SSE2") { return kernel_multiply_test<CPUType::SSE2, int8_t>(); }
KERNEL_TEST_CASE("multiply/int16 SSE2") { return kernel_multiply_test<CPUType::SSE2, int16_t>(); }
KERNEL_TEST_CASE("multiply/int SSE2") { return kernel_multiply_test<CPUType::SSE2, int>(); }
KERNEL_TEST_CASE("multiply/float SSE2") { return kernel_multiply_test<CPUType::SSE2, float>(); }
KERNEL_TEST_CASE("multiply/double SSE2") { return kernel_multiply_test<CPUType::SSE2, double>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_multiply_test<CPUType::AVX2, int8_t>();
template INTGEMM_AVX2 void kernel_multiply_test<CPUType::AVX2, int16_t>();
template INTGEMM_AVX2 void kernel_multiply_test<CPUType::AVX2, int>();
template INTGEMM_AVX2 void kernel_multiply_test<CPUType::AVX2, float>();
template INTGEMM_AVX2 void kernel_multiply_test<CPUType::AVX2, double>();
KERNEL_TEST_CASE("multiply/int8 AVX2") { return kernel_multiply_test<CPUType::AVX2, int8_t>(); }
KERNEL_TEST_CASE("multiply/int16 AVX2") { return kernel_multiply_test<CPUType::AVX2, int16_t>(); }
KERNEL_TEST_CASE("multiply/int AVX2") { return kernel_multiply_test<CPUType::AVX2, int>(); }
KERNEL_TEST_CASE("multiply/float AVX2") { return kernel_multiply_test<CPUType::AVX2, float>(); }
KERNEL_TEST_CASE("multiply/double AVX2") { return kernel_multiply_test<CPUType::AVX2, double>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_multiply_test<CPUType::AVX512BW, int8_t>();
template INTGEMM_AVX512BW void kernel_multiply_test<CPUType::AVX512BW, int16_t>();
template INTGEMM_AVX512BW void kernel_multiply_test<CPUType::AVX512BW, int>();
template INTGEMM_AVX512BW void kernel_multiply_test<CPUType::AVX512BW, float>();
template INTGEMM_AVX512BW void kernel_multiply_test<CPUType::AVX512BW, double>();
KERNEL_TEST_CASE("multiply/int8 AVX512BW") { return kernel_multiply_test<CPUType::AVX512BW, int8_t>(); }
KERNEL_TEST_CASE("multiply/int16 AVX512BW") { return kernel_multiply_test<CPUType::AVX512BW, int16_t>(); }
KERNEL_TEST_CASE("multiply/int AVX512BW") { return kernel_multiply_test<CPUType::AVX512BW, int>(); }
KERNEL_TEST_CASE("multiply/float AVX512BW") { return kernel_multiply_test<CPUType::AVX512BW, float>(); }
KERNEL_TEST_CASE("multiply/double AVX512BW") { return kernel_multiply_test<CPUType::AVX512BW, double>(); }
#endif

}
