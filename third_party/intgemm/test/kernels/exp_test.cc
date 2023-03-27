#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstddef>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_exp_approx_taylor_test() {
  if (kCPU < CPUType_)
    return;

  using vec_t = vector_t<CPUType_, float>;
  constexpr static std::size_t VECTOR_LENGTH = sizeof(vec_t) / sizeof(float);

  AlignedVector<float> input(VECTOR_LENGTH);
  AlignedVector<float> output(VECTOR_LENGTH);

  std::iota(input.begin(), input.end(), -static_cast<float>(VECTOR_LENGTH / 2));

  *output.template as<vec_t>() = kernels::exp_approx_taylor(*input.template as<vec_t>());
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK_EPS(output[i], exp(input[i]), 0.001f);
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_exp_approx_taylor_test<CPUType::AVX2>();
KERNEL_TEST_CASE("exp_approx_taylor AVX2") { return kernel_exp_approx_taylor_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_exp_approx_taylor_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("exp_approx_taylor AVX512BW") { return kernel_exp_approx_taylor_test<CPUType::AVX512BW>(); }
#endif

}
