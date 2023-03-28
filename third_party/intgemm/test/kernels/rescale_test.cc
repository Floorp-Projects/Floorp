#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstdint>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_rescale_test() {
  if (kCPU < CPUType_)
    return;

  using vi = vector_t<CPUType_, int>;
  using vf = vector_t<CPUType_, float>;
  constexpr int LENGTH = sizeof(vi) / sizeof(int);

  AlignedVector<int32_t> input(LENGTH);
  AlignedVector<int32_t> output(LENGTH);

  std::iota(input.begin(), input.end(), static_cast<int32_t>(-LENGTH / 2));
  float scale = 2;

  *output.template as<vi>() = kernels::rescale(*input.template as<vi>(), intgemm::set1_ps<vf>(scale));
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == std::round(input[i] * scale));
}

template INTGEMM_SSE2 void kernel_rescale_test<CPUType::SSE2>();
KERNEL_TEST_CASE("rescale SSE2") { return kernel_rescale_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_rescale_test<CPUType::AVX2>();
KERNEL_TEST_CASE("rescale AVX2") { return kernel_rescale_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_rescale_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("rescale AVX512BW") { return kernel_rescale_test<CPUType::AVX512BW>(); }
#endif

}
