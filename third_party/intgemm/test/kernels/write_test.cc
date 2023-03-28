#include "../test.h"
#include "../../intgemm/aligned.h"
#include "../../intgemm/kernels.h"

#include <cstddef>
#include <numeric>

namespace intgemm {

template <CPUType CPUType_, typename ElemType_>
void kernel_write_test() {
  if (kCPU < CPUType_)
    return;

  using vec_t = vector_t<CPUType_, ElemType_>;
  constexpr static std::size_t VECTOR_LENGTH = sizeof(vec_t) / sizeof(ElemType_);

  AlignedVector<ElemType_> input(VECTOR_LENGTH);
  AlignedVector<ElemType_> output(VECTOR_LENGTH);

  std::iota(input.begin(), input.end(), static_cast<ElemType_>(0));

  kernels::write(*input.template as<vec_t>(), output.begin(), 0);
  for (std::size_t i = 0; i < VECTOR_LENGTH; ++i)
    CHECK(output[i] == ElemType_(i));
}

template INTGEMM_SSE2 void kernel_write_test<CPUType::SSE2, int8_t>();
template INTGEMM_SSE2 void kernel_write_test<CPUType::SSE2, int16_t>();
template INTGEMM_SSE2 void kernel_write_test<CPUType::SSE2, int>();
template INTGEMM_SSE2 void kernel_write_test<CPUType::SSE2, float>();
template INTGEMM_SSE2 void kernel_write_test<CPUType::SSE2, double>();
KERNEL_TEST_CASE("write/int8 SSE2") { return kernel_write_test<CPUType::SSE2, int8_t>(); }
KERNEL_TEST_CASE("write/int16 SSE2") { return kernel_write_test<CPUType::SSE2, int16_t>(); }
KERNEL_TEST_CASE("write/int SSE2") { return kernel_write_test<CPUType::SSE2, int>(); }
KERNEL_TEST_CASE("write/float SSE2") { return kernel_write_test<CPUType::SSE2, float>(); }
KERNEL_TEST_CASE("write/double SSE2") { return kernel_write_test<CPUType::SSE2, double>(); }

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
template INTGEMM_AVX2 void kernel_write_test<CPUType::AVX2, int8_t>();
template INTGEMM_AVX2 void kernel_write_test<CPUType::AVX2, int16_t>();
template INTGEMM_AVX2 void kernel_write_test<CPUType::AVX2, int>();
template INTGEMM_AVX2 void kernel_write_test<CPUType::AVX2, float>();
template INTGEMM_AVX2 void kernel_write_test<CPUType::AVX2, double>();
KERNEL_TEST_CASE("write/int8 AVX2") { return kernel_write_test<CPUType::AVX2, int8_t>(); }
KERNEL_TEST_CASE("write/int16 AVX2") { return kernel_write_test<CPUType::AVX2, int16_t>(); }
KERNEL_TEST_CASE("write/int AVX2") { return kernel_write_test<CPUType::AVX2, int>(); }
KERNEL_TEST_CASE("write/float AVX2") { return kernel_write_test<CPUType::AVX2, float>(); }
KERNEL_TEST_CASE("write/double AVX2") { return kernel_write_test<CPUType::AVX2, double>(); }
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
template INTGEMM_AVX512BW void kernel_write_test<CPUType::AVX512BW, int8_t>();
template INTGEMM_AVX512BW void kernel_write_test<CPUType::AVX512BW, int16_t>();
template INTGEMM_AVX512BW void kernel_write_test<CPUType::AVX512BW, int>();
template INTGEMM_AVX512BW void kernel_write_test<CPUType::AVX512BW, float>();
template INTGEMM_AVX512BW void kernel_write_test<CPUType::AVX512BW, double>();
KERNEL_TEST_CASE("write/int8 AVX512BW") { return kernel_write_test<CPUType::AVX512BW, int8_t>(); }
KERNEL_TEST_CASE("write/int16 AVX512BW") { return kernel_write_test<CPUType::AVX512BW, int16_t>(); }
KERNEL_TEST_CASE("write/int AVX512BW") { return kernel_write_test<CPUType::AVX512BW, int>(); }
KERNEL_TEST_CASE("write/float AVX512BW") { return kernel_write_test<CPUType::AVX512BW, float>(); }
KERNEL_TEST_CASE("write/double AVX512BW") { return kernel_write_test<CPUType::AVX512BW, double>(); }
#endif

}
