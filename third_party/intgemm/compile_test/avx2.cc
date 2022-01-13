// Some compilers don't have AVX2 support.  Test for them.
#include <immintrin.h>

// clang-cl bug doesn't include these headers when pretending to be MSVC
// https://github.com/llvm/llvm-project/blob/e9a294449575a1e1a0daca470f64914695dc9adc/clang/lib/Headers/immintrin.h#L69-L72
#if defined(_MSC_VER) && defined(__clang__)
#include <avxintrin.h>
#include <avx2intrin.h>
#include <smmintrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define INTGEMM_AVX2
#else
#define INTGEMM_AVX2 __attribute__ ((target ("avx2")))
#endif

INTGEMM_AVX2 int Test() {
  __m256i value = _mm256_set1_epi32(1);
  value = _mm256_abs_epi8(value);
  return *(int*)&value;
}

int main() {
}
