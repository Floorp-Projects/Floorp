#include "test.h"
#include "../intgemm/aligned.h"
#include "../intgemm/avx2_gemm.h"
#include "../intgemm/avx512_gemm.h"
#include "../intgemm/sse2_gemm.h"
#include "../intgemm/ssse3_gemm.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace intgemm {
namespace {

template <typename Backend>
void PrepareBTransposedRef(const float* input, typename Backend::Integer* output, float quant_mult, Index B_transposed_cols, Index B_transposed_rows) {
  using vec_t = intgemm::vector_t<Backend::kUses, typename Backend::Integer>;
  constexpr Index vec_len = sizeof(vec_t) / sizeof(typename Backend::Integer);

  for (Index i = 0; i < B_transposed_rows * B_transposed_cols / 8; i += vec_len)
    for (Index j = 0; j < 8; ++j)
      for (Index k = 0; k < vec_len; ++k) {
        Index col = (i + k) % B_transposed_cols;
        Index row = 8 * ((i + k) / B_transposed_cols) + j;
        *output++ = static_cast<typename Backend::Integer>(input[row * B_transposed_cols + col] * quant_mult);
      }
}

template <typename Backend>
bool Test(const AlignedVector<float>& input, Index B_rows, Index B_cols, float quant_mult) {
  bool success = true;

  AlignedVector<typename Backend::Integer> output(input.size());
  Backend::PrepareBTransposed(input.begin(), output.begin(), quant_mult, B_rows, B_cols);

  AlignedVector<typename Backend::Integer> reference(input.size());
  PrepareBTransposedRef<Backend>(input.begin(), reference.begin(), quant_mult, B_rows, B_cols);

  for (std::size_t i = 0; i < output.size(); ++i) {
    if (output[i] != reference[i]) {
      UNSCOPED_INFO("Error at " << i << ", output = " << int(output[i]) << ", reference = " << int(reference[i]));
      success = false;
      break;
    }
  }
  return success;
}

template <typename Backend>
bool TestMany(Index B_rows, Index B_cols, float quant_mult) {
  AlignedVector<float> input(B_rows * B_cols);

  std::generate(input.begin(), input.end(), []() {
    static constexpr int divider = sizeof(intgemm::vector_t<Backend::kUses, typename Backend::Integer>) / sizeof(typename Backend::Integer);
    static int value = 0;
    return static_cast<float>((value++) % divider);
  });

  return Test<Backend>(input, B_rows, B_cols, quant_mult);
}

TEST_CASE("PrepareBTransposed SSE2", "") {
  if (kCPU < CPUType::SSE2)
    return;

  CHECK(TestMany<SSE2::Kernels16>(4, 128, 2.0f));
}

TEST_CASE("PrepareBTransposed SSSE3", "") {
  if (kCPU < CPUType::SSSE3)
    return;

  CHECK(TestMany<SSSE3::Kernels8>(4, 128, 2.0f));
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("PrepareBTransposed AVX2", "") {
  if (kCPU < CPUType::AVX2)
    return;

  CHECK(TestMany<AVX2::Kernels8>(8, 128, 2.0f));
  CHECK(TestMany<AVX2::Kernels16>(8, 128, 2.0f));
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
TEST_CASE("PrepareBTransposed AVX512", "") {
  if (kCPU < CPUType::AVX512BW)
    return;

  CHECK(TestMany<AVX512BW::Kernels8>(16, 128, 2.0f));
  CHECK(TestMany<AVX512BW::Kernels16>(16, 128, 2.0f));
}
#endif

}
}
