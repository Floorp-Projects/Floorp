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
#include <avx512vnniintrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#elif defined(__INTEL_COMPILER)
__attribute__ ((target ("avx512f")))
#else
__attribute__ ((target ("avx512f,avx512bw,avx512dq,avx512vnni")))
#endif
bool Foo() {
  // AVX512F
  __m512i value = _mm512_set1_epi32(1);
  // AVX512BW
  value = _mm512_maddubs_epi16(value, value);
  // AVX512DQ
   __m256i value2 = _mm256_set1_epi8(1);
  value = _mm512_inserti32x8(value, value2, 1);
  // AVX512VNNI
  value = _mm512_dpbusd_epi32(value, value, value);
  return *(int*)&value;
}

int main() {
  return Foo();
}
