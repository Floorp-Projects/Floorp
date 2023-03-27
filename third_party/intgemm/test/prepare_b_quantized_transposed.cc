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
void PrepareBQuantizedTransposedRef(const typename Backend::Integer* input, typename Backend::Integer* output, Index B_transposed_cols, Index B_transposed_rows) {
  using vec_t = intgemm::vector_t<Backend::kUses, typename Backend::Integer>;
  constexpr Index vec_len = sizeof(vec_t) / sizeof(typename Backend::Integer);

  auto output_it = output;
  for (Index r = 0; r < B_transposed_rows; r += 8)
    for (Index c = 0; c < B_transposed_cols; c += vec_len)
      for (Index ri = 0; ri < 8; ++ri)
        for (Index ci = 0; ci < vec_len; ++ci)
          *output_it++ = input[(r + ri) * B_transposed_cols + c + ci];
}

template <typename Backend>
bool Test(const AlignedVector<typename Backend::Integer>& input, Index B_rows, Index B_cols) {
  bool success = true;

  AlignedVector<typename Backend::Integer> output(input.size());
  Backend::PrepareBQuantizedTransposed(input.begin(), output.begin(), B_rows, B_cols);

  AlignedVector<typename Backend::Integer> reference(input.size());
  PrepareBQuantizedTransposedRef<Backend>(input.begin(), reference.begin(), B_rows, B_cols);

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
bool TestMany(Index B_rows, Index B_cols) {
  AlignedVector<typename Backend::Integer> input(B_rows * B_cols);

  std::generate(input.begin(), input.end(), []() {
    static constexpr int divider = sizeof(intgemm::vector_t<Backend::kUses, typename Backend::Integer>) / sizeof(typename Backend::Integer);
    static int value = 0;
    return static_cast<typename Backend::Integer>((value++) % divider);
  });

  return Test<Backend>(input, B_rows, B_cols);
}

TEST_CASE("PrepareBQuantizedTransposed SSE2", "") {
  if (kCPU < CPUType::SSE2)
    return;

  CHECK(TestMany<SSE2::Kernels16>(32, 128));
}

TEST_CASE("PrepareBQuantizedTransposed SSSE3", "") {
  if (kCPU < CPUType::SSSE3)
    return;

  CHECK(TestMany<SSSE3::Kernels8>(32, 128));
}

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
TEST_CASE("PrepareBQuantizedTransposed AVX2", "") {
  if (kCPU < CPUType::AVX2)
    return;

  CHECK(TestMany<AVX2::Kernels8>(32, 128));
  CHECK(TestMany<AVX2::Kernels16>(32, 128));
}
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
  TEST_CASE("PrepareBQuantizedTransposed AVX512", "") {
    if (kCPU < CPUType::AVX512BW)
      return;

    CHECK(TestMany<AVX512BW::Kernels8>(64, 128));
    CHECK(TestMany<AVX512BW::Kernels16>(64, 128));
  }
#endif

}
}
