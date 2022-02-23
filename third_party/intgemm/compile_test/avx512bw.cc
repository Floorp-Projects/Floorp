// Some compilers don't have AVX512BW support.  Test for them.
#include <immintrin.h>

// clang-cl bug doesn't include these headers when pretending to be MSVC
// https://github.com/llvm/llvm-project/blob/e9a294449575a1e1a0daca470f64914695dc9adc/clang/lib/Headers/immintrin.h#L69-L72
#if defined(_MSC_VER) && defined(__clang__)
#include <avxintrin.h>
#include <avx2intrin.h>
#include <smmintrin.h>
#include <avx512fintrin.h>
#include <avx512dqintrin.h>
#include <avx512bwintrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define INTGEMM_AVX512BW
#elif defined(__INTEL_COMPILER)
#define INTGEMM_AVX512BW __attribute__ ((target ("avx512f")))
#else
#define INTGEMM_AVX512BW __attribute__ ((target ("avx512bw")))
#endif

INTGEMM_AVX512BW int Test() {
  // AVX512BW
  __m512i value = _mm512_set1_epi32(1);
  value = _mm512_maddubs_epi16(value, value);
  return *(int*)&value;
}

int main() {
}
