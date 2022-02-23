#pragma once
#include "intgemm/intgemm_config.h"

#include <exception>
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
#include <immintrin.h>
#endif
#include <emmintrin.h>

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

#if (defined(_MSC_VER) && !defined(__clang__)) || defined(__INTEL_COMPILER)
/* Real MSVC does not appear to have target attributes but is also fine with
 * just using intrinsics anywhere.  clang-cl pretending to be MSVC requires
 * target attributes, so it's excluded from the above.
 *
 * The Intel compiler has a bug whereby constructors with target attributes do
 * not link.  Like this program doesn't compile with icpc:
 * class Foo {
 *   public:
 *     __attribute__ ((target ("avx2"))) Foo() {}
 * };
 * int main() { Foo a; }
 *
 * It appears to be erroneously activating function multiversioning when only
 * one version of a constructor with target attributes is defined.  Normal
 * methods with one target attribute work fine.  The Intel compiler also allows
 * intrinsics without any target attributes so we just leave them blank.
 */
  #define INTGEMM_SSE2
  #define INTGEMM_SSSE3
  #define INTGEMM_AVX2
  #define INTGEMM_AVX512F
  #define INTGEMM_AVX512BW
  #define INTGEMM_AVX512DQ
  #define INTGEMM_AVX512VNNI
#else
  /* gcc and clang take lists of all the flavors */
  #define INTGEMM_SSE2 __attribute__ ((target ("sse2")))
  #define INTGEMM_SSSE3 __attribute__ ((target ("ssse3")))
  #define INTGEMM_AVX2 __attribute__ ((target ("avx2")))
  #define INTGEMM_AVX512F __attribute__ ((target ("avx512f")))
  #define INTGEMM_AVX512BW __attribute__ ((target ("avx512f,avx512bw,avx512dq")))
  #define INTGEMM_AVX512DQ __attribute__ ((target ("avx512f,avx512bw,avx512dq")))
  #define INTGEMM_AVX512VNNI __attribute__ ((target ("avx512f,avx512bw,avx512dq,avx512vnni")))
#endif
namespace intgemm {

// This will be thrown if a CPU isn't supported by the routines (16-bit without SSE2 or 8-bit without SSSE3).
class UnsupportedCPU : public std::exception {
  public:
    UnsupportedCPU() {}

    ~UnsupportedCPU() throw() {}

    const char *what() const throw() override {
      return "Integer matrix multiplication has not been efficiently implemented for your CPU.";
    }
};

typedef unsigned int Index;

// If you want to detect the CPU and dispatch yourself, here's what to use:
enum class CPUType {
  UNSUPPORTED = 0,
  SSE2 = 1,
  SSSE3 = 2,
  AVX2 = 3,
  AVX512BW = 4,
  AVX512VNNI = 5
};

// Running CPU type.  This is defined in intgemm.cc (as the dispatcher).
extern const CPUType kCPU;

struct MeanStd {
  float mean;
  float stddev;
};

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
namespace AVX512VNNI {
typedef __m512i Register;
typedef __m512 FRegister;
} // namespace AVX512VNNI
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
namespace AVX512BW {
typedef __m512i Register;
typedef __m512 FRegister;
} // namespace AVX512BW
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
namespace AVX2 {
typedef __m256i Register;
typedef __m256 FRegister;
} // namespace AVX2
#endif
namespace SSSE3 {
typedef __m128i Register;
typedef __m128 FRegister;
} // namespace SSSE3
namespace SSE2 {
typedef __m128i Register;
typedef __m128 FRegister;
} // namespace SSE2

} // namespace intgemm
