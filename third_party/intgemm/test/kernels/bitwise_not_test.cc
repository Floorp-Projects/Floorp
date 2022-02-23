#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstdlib>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_bitwise_not_test() {
  if (kCPU < CPUType_)
    return;

  using vec_t = vector_t<CPUType_, int>;
  constexpr static std::size_t VECTOR_LENGTH = sizeof(vec_t) / sizeof(int);

  AlignedVector<int> input(VECTOR_LENGTH);
  AlignedVector<int> output(VECTOR_LENGTH);

  std::iota(input.begin(), input.end(), 0);

  *output.template as<vec_t>() = kernels::bitwise_not(*input.template as<vec_t>());
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == ~input[i]);
}

template INTGEMM_SSE2 void kernel_bitwise_not_test<CPUType::SSE2>();
KERNEL_TEST_CASE("bitwise_not SSE2") { return kernel_bitwise_not_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_bitwise_not_test<CPUType::AVX2>();
KERNEL_TEST_CASE("bitwise_not AVX2") { return kernel_bitwise_not_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_bitwise_not_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("bitwise_not AVX512BW") { return kernel_bitwise_not_test<CPUType::AVX512BW>(); }
#endif

}
