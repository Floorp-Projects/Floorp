#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <numeric>

namespace intgemm {

template <CPUType CPUType_>
void kernel_quantize_test() {
  if (kCPU < CPUType_)
    return;

  using input_vec_t = vector_t<CPUType_, float>;
  using output_vec_t = vector_t<CPUType_, int>;

  AlignedVector<float> input(sizeof(input_vec_t) / sizeof(float));
  AlignedVector<int> output(sizeof(output_vec_t) / sizeof(int));

  std::iota(input.begin(), input.end(), 0.0f);
  auto quant_mult = set1_ps<input_vec_t>(2.f);

  *output.template as<output_vec_t>() = kernels::quantize(*input.template as<input_vec_t>(), quant_mult);
  for (std::size_t i = 0; i < output.size(); ++i)
    CHECK(output[i] == int(i*2.f));
}

template INTGEMM_SSE2 void kernel_quantize_test<CPUType::SSE2>();
KERNEL_TEST_CASE("quantize SSE2") { return kernel_quantize_test<CPUType::SSE2>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_quantize_test<CPUType::AVX2>();
KERNEL_TEST_CASE("quantize AVX2") { return kernel_quantize_test<CPUType::AVX2>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_quantize_test<CPUType::AVX512BW>();
KERNEL_TEST_CASE("quantize AVX512BW") { return kernel_quantize_test<CPUType::AVX512BW>(); }
#endif

}
