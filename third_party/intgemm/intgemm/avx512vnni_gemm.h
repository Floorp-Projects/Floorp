#pragma once

#include "intgemm/intgemm_config.h"

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
#include "avx512_gemm.h"
#include "types.h"

namespace intgemm {
namespace AVX512VNNI {

// Workaround extra vmovdqa64 https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94663
INTGEMM_AVX512VNNI static inline void VNNI8(__m512i &c, __m512i a, __m512i b) {
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
    asm ("vpdpbusds %2, %1, %0" : "+x"(c) : "x"(a), "mx"(b));
#else
    c = _mm512_dpbusds_epi32(c, a, b);
#endif
}

struct Kernels8 : public AVX512BW::Kernels8 {
  template <typename Callback>
  INTGEMM_AVX512VNNI static void Multiply(const int8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) {
    assert(width % sizeof(Register) == 0);
    assert(B_cols % 8 == 0);
    assert(reinterpret_cast<uintptr_t>(A) % sizeof(Register) == 0);
    assert(reinterpret_cast<uintptr_t>(B) % sizeof(Register) == 0);
    auto callback_impl = callbacks::CallbackImpl<CPUType::AVX2, Callback>(callback);
    const Index simd_width = width / sizeof(Register);
    Register zeros = setzero_si<Register>();
    // Go over 8 columns of B at a time.
#pragma omp for
    for (Index B0_colidx = 0; B0_colidx < B_cols; B0_colidx += 8) {
      const Register *B0_col = reinterpret_cast<const Register*>(B) + B0_colidx * simd_width;
      // Process one row of A at a time.  Doesn't seem to be faster to do multiple rows of A at once.
      for (Index A_rowidx = 0; A_rowidx < A_rows; ++A_rowidx) {
        // Iterate over shared (inner) dimension.
        const Register *A_live = reinterpret_cast<const Register *>(A + A_rowidx * width);
        const Register *A_end = A_live + simd_width;
        const Register *B_live = B0_col;
        // TODO: separate first step.
        Register sum0 = zeros, sum1 = zeros, sum2 = zeros, sum3 = zeros, sum4 = zeros, sum5 = zeros, sum6 = zeros, sum7 = zeros;
        for (; A_live != A_end; ++A_live, B_live += 8) {
          Register a = *A_live;
          // Retrieve the conveniently consecutive values of B.
          Register b0 = *B_live;
          Register b1 = *(B_live + 1);
          Register b2 = *(B_live + 2);
          Register b3 = *(B_live + 3);
          Register b4 = *(B_live + 4);
          Register b5 = *(B_live + 5);
          Register b6 = *(B_live + 6);
          Register b7 = *(B_live + 7);
          // Get a mask where a is negative.
          __mmask64 neg_mask = _mm512_test_epi8_mask(a, _mm512_set1_epi8(-128));
          Register a_positive = _mm512_abs_epi8(a);
          // Negate by subtracting from zero with a mask.
          b0 = _mm512_mask_sub_epi8(b0, neg_mask, zeros, b0);
          b1 = _mm512_mask_sub_epi8(b1, neg_mask, zeros, b1);
          b2 = _mm512_mask_sub_epi8(b2, neg_mask, zeros, b2);
          b3 = _mm512_mask_sub_epi8(b3, neg_mask, zeros, b3);
          b4 = _mm512_mask_sub_epi8(b4, neg_mask, zeros, b4);
          b5 = _mm512_mask_sub_epi8(b5, neg_mask, zeros, b5);
          b6 = _mm512_mask_sub_epi8(b6, neg_mask, zeros, b6);
          b7 = _mm512_mask_sub_epi8(b7, neg_mask, zeros, b7);
          VNNI8(sum0, a_positive, b0);
          VNNI8(sum1, a_positive, b1);
          VNNI8(sum2, a_positive, b2);
          VNNI8(sum3, a_positive, b3);
          VNNI8(sum4, a_positive, b4);
          VNNI8(sum5, a_positive, b5);
          VNNI8(sum6, a_positive, b6);
          VNNI8(sum7, a_positive, b7);
        }
        Register pack0123 = Pack0123(sum0, sum1, sum2, sum3);
        Register pack4567 = Pack0123(sum4, sum5, sum6, sum7);
        auto total = PermuteSummer(pack0123, pack4567);
        callback_impl.Run(total, callbacks::OutputBufferInfo(A_rowidx, B0_colidx, A_rows, B_cols));
      }
    }
  }

  template <typename Callback>
  INTGEMM_AVX512VNNI static void Multiply8Shift(const uint8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) {
    assert(width % sizeof(Register) == 0);
    assert(B_cols % 8 == 0);
    assert(reinterpret_cast<uintptr_t>(A) % sizeof(Register) == 0);
    assert(reinterpret_cast<uintptr_t>(B) % sizeof(Register) == 0);
    auto callback_impl = callbacks::CallbackImpl<CPUType::AVX2, Callback>(callback);
    const Index simd_width = width / sizeof(Register);
    Register zeros = setzero_si<Register>();
    // Go over 8 columns of B at a time.
#pragma omp for
    for (Index B0_colidx = 0; B0_colidx < B_cols; B0_colidx += 8) {
      const Register *B0_col = reinterpret_cast<const Register*>(B) + B0_colidx * simd_width;
      // Process one row of A at a time.  Doesn't seem to be faster to do multiple rows of A at once.
      for (Index A_rowidx = 0; A_rowidx < A_rows; ++A_rowidx) {
        // Iterate over shared (inner) dimension.
        const Register *A_live = reinterpret_cast<const Register *>(A + A_rowidx * width);
        const Register *A_end = A_live + simd_width;
        const Register *B_live = B0_col;
        // TODO: separate first step.
        Register sum0 = zeros, sum1 = zeros, sum2 = zeros, sum3 = zeros, sum4 = zeros, sum5 = zeros, sum6 = zeros, sum7 = zeros;
        for (; A_live != A_end; ++A_live, B_live += 8) {
          Register a = *A_live;
          //MultiplyAdd
          VNNI8(sum0, a, *B_live);
          VNNI8(sum1, a, *(B_live + 1));
          VNNI8(sum2, a, *(B_live + 2));
          VNNI8(sum3, a, *(B_live + 3));
          VNNI8(sum4, a, *(B_live + 4));
          VNNI8(sum5, a, *(B_live + 5));
          VNNI8(sum6, a, *(B_live + 6));
          VNNI8(sum7, a, *(B_live + 7));
        }
        Register pack0123 = Pack0123(sum0, sum1, sum2, sum3);
        Register pack4567 = Pack0123(sum4, sum5, sum6, sum7);
        auto total = PermuteSummer(pack0123, pack4567);
        callback_impl.Run(total, callbacks::OutputBufferInfo(A_rowidx, B0_colidx, A_rows, B_cols));
      }
    }
  }

  template <typename Callback>
  INTGEMM_AVX512VNNI static void PrepareBias(const int8_t *B, Index width, Index B_cols, Callback callback) {
    assert(width % sizeof(Register) == 0);
    assert(B_cols % 8 == 0);
    assert(reinterpret_cast<uintptr_t>(B) % sizeof(Register) == 0);
    auto callback_impl = callbacks::CallbackImpl<CPUType::AVX2, Callback>(callback);
    Index simd_width = width / sizeof(Register);
    Register zeros = setzero_si<Register>();
    const Register a = set1_epi8<Register>(1);
    // Go over 8 columns of B at a time.
#pragma omp for
    for (Index B0_colidx = 0; B0_colidx < B_cols; B0_colidx += 8) {
      const Register *B0_col = reinterpret_cast<const Register*>(B) + B0_colidx * simd_width;
      const Register *B_live = B0_col; //In order to make the code look as much as possible as the above function
      const Register *B_end = B_live + simd_width*8;

      // TODO: separate first step.
      Register sum0 = zeros, sum1 = zeros, sum2 = zeros, sum3 = zeros, sum4 = zeros, sum5 = zeros, sum6 = zeros, sum7 = zeros;
      for (; B_live != B_end; B_live += 8) {
        // Retrieve the conveniently consecutive values of B.
        VNNI8(sum0, a, *B_live);
        VNNI8(sum1, a, *(B_live + 1));
        VNNI8(sum2, a, *(B_live + 2));
        VNNI8(sum3, a, *(B_live + 3));
        VNNI8(sum4, a, *(B_live + 4));
        VNNI8(sum5, a, *(B_live + 5));
        VNNI8(sum6, a, *(B_live + 6));
        VNNI8(sum7, a, *(B_live + 7));
      }
      Register pack0123 = Pack0123(sum0, sum1, sum2, sum3);
      Register pack4567 = Pack0123(sum4, sum5, sum6, sum7);
      auto total = PermuteSummer(pack0123, pack4567);
      callback_impl.Run(total, callbacks::OutputBufferInfo(0, B0_colidx, 1, B_cols));
    }
  }

  constexpr static const char *const kName = "8-bit AVX512VNNI";

  static const CPUType kUses = CPUType::AVX512VNNI;
};

} // namespace AVX512VNNI
} // namespace intgemm

#endif
