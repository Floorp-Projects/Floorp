#if !defined(SIMDE_X86_AVX512_POPCNT_H)
#define SIMDE_X86_AVX512_POPCNT_H

#include "types.h"
#include "mov.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_popcnt_epi8 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_popcnt_epi8(a);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i8 = vcntq_s8(a_.neon_i8);
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i8x16_popcnt(a_.wasm_v128);
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      const __m128i low_nibble_set = _mm_set1_epi8(0x0f);
      const __m128i high_nibble_of_input = _mm_andnot_si128(low_nibble_set, a_.n);
      const __m128i low_nibble_of_input = _mm_and_si128(low_nibble_set, a_.n);
      const __m128i lut = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);

      r_.n =
        _mm_add_epi8(
          _mm_shuffle_epi8(
            lut,
            low_nibble_of_input
          ),
          _mm_shuffle_epi8(
            lut,
            _mm_srli_epi16(
              high_nibble_of_input,
              4
            )
          )
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      /* v -= ((v >> 1) & UINT8_C(0x55)); */
      r_.n =
        _mm_sub_epi8(
          a_.n,
          _mm_and_si128(
            _mm_srli_epi16(a_.n, 1),
            _mm_set1_epi8(0x55)
          )
        );

      /* v  = (v & 0x33) + ((v >> 2) & 0x33); */
      r_.n =
        _mm_add_epi8(
          _mm_and_si128(
            r_.n,
            _mm_set1_epi8(0x33)
          ),
          _mm_and_si128(
            _mm_srli_epi16(r_.n, 2),
            _mm_set1_epi8(0x33)
          )
        );

      /* v = (v + (v >> 4)) & 0xf */
      r_.n =
        _mm_and_si128(
          _mm_add_epi8(
            r_.n,
            _mm_srli_epi16(r_.n, 4)
          ),
          _mm_set1_epi8(0x0f)
        );
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r_.altivec_i8 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(signed char), vec_popcnt(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned char), a_.altivec_i8)));
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u8 -= ((a_.u8 >> 1) & 0x55);
      a_.u8  = ((a_.u8 & 0x33) + ((a_.u8 >> 2) & 0x33));
      a_.u8  = (a_.u8 + (a_.u8 >> 4)) & 15;
      r_.u8  = a_.u8 >> ((sizeof(uint8_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
        uint8_t v = HEDLEY_STATIC_CAST(uint8_t, a_.u8[i]);
        v -= ((v >> 1) & 0x55);
        v  = (v & 0x33) + ((v >> 2) & 0x33);
        v  = (v + (v >> 4)) & 0xf;
        r_.u8[i] = v >> (sizeof(uint8_t) - 1) * CHAR_BIT;
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_popcnt_epi8
  #define _mm_popcnt_epi8(a) simde_mm_popcnt_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_popcnt_epi8 (simde__m128i src, simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_popcnt_epi8(src, k, a);
  #else
    return simde_mm_mask_mov_epi8(src, k, simde_mm_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_popcnt_epi8
  #define _mm_mask_popcnt_epi8(src, k, a) simde_mm_mask_popcnt_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_popcnt_epi8 (simde__mmask16 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_popcnt_epi8(k, a);
  #else
    return simde_mm_maskz_mov_epi8(k, simde_mm_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_popcnt_epi8
  #define _mm_maskz_popcnt_epi8(k, a) simde_mm_maskz_popcnt_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_popcnt_epi16 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_popcnt_epi16(a);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i16 = vpaddlq_s8(vcntq_s8(a_.neon_i8));
    #elif defined(SIMDE_WASM_SIMD128_NATIVE)
      r_.wasm_v128 = wasm_i16x8_extadd_pairwise_i8x16(wasm_i8x16_popcnt(a_.wasm_v128));
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r_.altivec_u16 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), vec_popcnt(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned short), a_.altivec_u16)));
    #elif defined(SIMDE_X86_XOP_NATIVE)
      const __m128i low_nibble_set = _mm_set1_epi8(0x0f);
      const __m128i high_nibble_of_input = _mm_andnot_si128(low_nibble_set, a_.n);
      const __m128i low_nibble_of_input = _mm_and_si128(low_nibble_set, a_.n);
      const __m128i lut = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);

      r_.n =
        _mm_haddw_epi8(
          _mm_add_epi8(
            _mm_shuffle_epi8(
              lut,
              low_nibble_of_input
            ),
            _mm_shuffle_epi8(
              lut,
              _mm_srli_epi16(high_nibble_of_input, 4)
            )
          )
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.n =
        _mm_sub_epi16(
          a_.n,
          _mm_and_si128(
            _mm_srli_epi16(a_.n, 1),
            _mm_set1_epi16(0x5555)
          )
        );

      r_.n =
        _mm_add_epi16(
          _mm_and_si128(
            r_.n,
            _mm_set1_epi16(0x3333)
          ),
          _mm_and_si128(
            _mm_srli_epi16(r_.n, 2),
            _mm_set1_epi16(0x3333)
          )
        );

      r_.n =
        _mm_and_si128(
          _mm_add_epi16(
            r_.n,
            _mm_srli_epi16(r_.n, 4)
          ),
          _mm_set1_epi16(0x0f0f)
        );

      r_.n =
        _mm_srli_epi16(
          _mm_mullo_epi16(
            r_.n,
            _mm_set1_epi16(0x0101)
          ),
          (sizeof(uint16_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u16 -= ((a_.u16 >> 1) & UINT16_C(0x5555));
      a_.u16  = ((a_.u16 & UINT16_C(0x3333)) + ((a_.u16 >> 2) & UINT16_C(0x3333)));
      a_.u16  = (a_.u16 + (a_.u16 >> 4)) & UINT16_C(0x0f0f);
      r_.u16  = (a_.u16 * UINT16_C(0x0101)) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        uint16_t v = HEDLEY_STATIC_CAST(uint16_t, a_.u16[i]);
        v -= ((v >> 1) & UINT16_C(0x5555));
        v  = ((v & UINT16_C(0x3333)) + ((v >> 2) & UINT16_C(0x3333)));
        v  = (v + (v >> 4)) & UINT16_C(0x0f0f);
        r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, (v * UINT16_C(0x0101))) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_popcnt_epi16
  #define _mm_popcnt_epi16(a) simde_mm_popcnt_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_popcnt_epi16 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_popcnt_epi16(src, k, a);
  #else
    return simde_mm_mask_mov_epi16(src, k, simde_mm_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_popcnt_epi16
  #define _mm_mask_popcnt_epi16(src, k, a) simde_mm_mask_popcnt_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_popcnt_epi16 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_popcnt_epi16(k, a);
  #else
    return simde_mm_maskz_mov_epi16(k, simde_mm_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_popcnt_epi16
  #define _mm_maskz_popcnt_epi16(k, a) simde_mm_maskz_popcnt_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_popcnt_epi32 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_popcnt_epi32(a);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i32 = vpaddlq_s16(vpaddlq_s8(vcntq_s8(a_.neon_i8)));
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r_.altivec_u32 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), vec_popcnt(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned int), a_.altivec_u32)));
    #elif defined(SIMDE_X86_XOP_NATIVE)
      const __m128i low_nibble_set = _mm_set1_epi8(0x0f);
      const __m128i high_nibble_of_input = _mm_andnot_si128(low_nibble_set, a_.n);
      const __m128i low_nibble_of_input = _mm_and_si128(low_nibble_set, a_.n);
      const __m128i lut = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);

      r_.n =
        _mm_haddd_epi8(
          _mm_add_epi8(
            _mm_shuffle_epi8(
              lut,
              low_nibble_of_input
            ),
            _mm_shuffle_epi8(
              lut,
              _mm_srli_epi16(high_nibble_of_input, 4)
            )
          )
        );
    #elif defined(SIMDE_X86_SSE4_1_NATIVE)
      r_.n =
        _mm_sub_epi32(
          a_.n,
          _mm_and_si128(
            _mm_srli_epi32(a_.n, 1),
            _mm_set1_epi32(0x55555555)
          )
        );

      r_.n =
        _mm_add_epi32(
          _mm_and_si128(
            r_.n,
            _mm_set1_epi32(0x33333333)
          ),
          _mm_and_si128(
            _mm_srli_epi32(r_.n, 2),
            _mm_set1_epi32(0x33333333)
          )
        );

      r_.n =
        _mm_and_si128(
          _mm_add_epi32(
            r_.n,
            _mm_srli_epi32(r_.n, 4)
          ),
          _mm_set1_epi32(0x0f0f0f0f)
        );

      r_.n =
        _mm_srli_epi32(
          _mm_mullo_epi32(
            r_.n,
            _mm_set1_epi32(0x01010101)
          ),
          (sizeof(uint32_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u32 -= ((a_.u32 >> 1) & UINT32_C(0x55555555));
      a_.u32  = ((a_.u32 & UINT32_C(0x33333333)) + ((a_.u32 >> 2) & UINT32_C(0x33333333)));
      a_.u32  = (a_.u32 + (a_.u32 >> 4)) & UINT32_C(0x0f0f0f0f);
      r_.u32  = (a_.u32 * UINT32_C(0x01010101)) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        uint32_t v = HEDLEY_STATIC_CAST(uint32_t, a_.u32[i]);
        v -= ((v >> 1) & UINT32_C(0x55555555));
        v  = ((v & UINT32_C(0x33333333)) + ((v >> 2) & UINT32_C(0x33333333)));
        v  = (v + (v >> 4)) & UINT32_C(0x0f0f0f0f);
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, (v * UINT32_C(0x01010101))) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_popcnt_epi32
  #define _mm_popcnt_epi32(a) simde_mm_popcnt_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_popcnt_epi32 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_popcnt_epi32(src, k, a);
  #else
    return simde_mm_mask_mov_epi32(src, k, simde_mm_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_popcnt_epi32
  #define _mm_mask_popcnt_epi32(src, k, a) simde_mm_mask_popcnt_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_popcnt_epi32 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_popcnt_epi32(k, a);
  #else
    return simde_mm_maskz_mov_epi32(k, simde_mm_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_popcnt_epi32
  #define _mm_maskz_popcnt_epi32(k, a) simde_mm_maskz_popcnt_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_popcnt_epi64 (simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_popcnt_epi64(a);
  #else
    simde__m128i_private
      r_,
      a_ = simde__m128i_to_private(a);

    #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
      r_.neon_i64 = vpaddlq_s32(vpaddlq_s16(vpaddlq_s8(vcntq_s8(a_.neon_i8))));
    #elif defined(SIMDE_POWER_ALTIVEC_P8_NATIVE)
      r_.altivec_u64 = HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), vec_popcnt(HEDLEY_REINTERPRET_CAST(SIMDE_POWER_ALTIVEC_VECTOR(unsigned long long), a_.altivec_u64)));
    #elif defined(SIMDE_X86_SSSE3_NATIVE)
      const __m128i low_nibble_set = _mm_set1_epi8(0x0f);
      const __m128i high_nibble_of_input = _mm_andnot_si128(low_nibble_set, a_.n);
      const __m128i low_nibble_of_input = _mm_and_si128(low_nibble_set, a_.n);
      const __m128i lut = _mm_set_epi8(4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0);

      r_.n =
        _mm_sad_epu8(
          _mm_add_epi8(
            _mm_shuffle_epi8(
              lut,
              low_nibble_of_input
            ),
            _mm_shuffle_epi8(
              lut,
              _mm_srli_epi16(high_nibble_of_input, 4)
            )
          ),
          _mm_setzero_si128()
        );
    #elif defined(SIMDE_X86_SSE2_NATIVE)
      r_.n =
        _mm_sub_epi8(
          a_.n,
          _mm_and_si128(
            _mm_srli_epi16(a_.n, 1),
            _mm_set1_epi8(0x55)
          )
        );

      r_.n =
        _mm_add_epi8(
          _mm_and_si128(
            r_.n,
            _mm_set1_epi8(0x33)
          ),
          _mm_and_si128(
            _mm_srli_epi16(r_.n, 2),
            _mm_set1_epi8(0x33)
          )
        );

      r_.n =
        _mm_and_si128(
          _mm_add_epi8(
            r_.n,
            _mm_srli_epi16(r_.n, 4)
          ),
          _mm_set1_epi8(0x0f)
        );

      r_.n =
        _mm_sad_epu8(
          r_.n,
          _mm_setzero_si128()
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u64 -= ((a_.u64 >> 1) & UINT64_C(0x5555555555555555));
      a_.u64  = ((a_.u64 & UINT64_C(0x3333333333333333)) + ((a_.u64 >> 2) & UINT64_C(0x3333333333333333)));
      a_.u64  = (a_.u64 + (a_.u64 >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
      r_.u64  = (a_.u64 * UINT64_C(0x0101010101010101)) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
        uint64_t v = HEDLEY_STATIC_CAST(uint64_t, a_.u64[i]);
        v -= ((v >> 1) & UINT64_C(0x5555555555555555));
        v  = ((v & UINT64_C(0x3333333333333333)) + ((v >> 2) & UINT64_C(0x3333333333333333)));
        v  = (v + (v >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        r_.u64[i] = HEDLEY_STATIC_CAST(uint64_t, (v * UINT64_C(0x0101010101010101))) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m128i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_popcnt_epi64
  #define _mm_popcnt_epi64(a) simde_mm_popcnt_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_mask_popcnt_epi64 (simde__m128i src, simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_mask_popcnt_epi64(src, k, a);
  #else
    return simde_mm_mask_mov_epi64(src, k, simde_mm_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_mask_popcnt_epi64
  #define _mm_mask_popcnt_epi64(src, k, a) simde_mm_mask_popcnt_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m128i
simde_mm_maskz_popcnt_epi64 (simde__mmask8 k, simde__m128i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm_maskz_popcnt_epi64(k, a);
  #else
    return simde_mm_maskz_mov_epi64(k, simde_mm_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm_maskz_popcnt_epi64
  #define _mm_maskz_popcnt_epi64(k, a) simde_mm_maskz_popcnt_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_popcnt_epi8 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_popcnt_epi8(a);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi8(a_.m128i[i]);
      }
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      const __m256i low_nibble_set = _mm256_set1_epi8(0x0f);
      const __m256i high_nibble_of_input = _mm256_andnot_si256(low_nibble_set, a_.n);
      const __m256i low_nibble_of_input = _mm256_and_si256(low_nibble_set, a_.n);
      const __m256i lut =
        _mm256_set_epi8(
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
        );

      r_.n =
        _mm256_add_epi8(
          _mm256_shuffle_epi8(
            lut,
            low_nibble_of_input
          ),
          _mm256_shuffle_epi8(
            lut,
            _mm256_srli_epi16(
              high_nibble_of_input,
              4
            )
          )
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u8 -= ((a_.u8 >> 1) & 0x55);
      a_.u8  = ((a_.u8 & 0x33) + ((a_.u8 >> 2) & 0x33));
      a_.u8  = (a_.u8 + (a_.u8 >> 4)) & 15;
      r_.u8  = a_.u8 >> ((sizeof(uint8_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
        uint8_t v = HEDLEY_STATIC_CAST(uint8_t, a_.u8[i]);
        v -= ((v >> 1) & 0x55);
        v  = (v & 0x33) + ((v >> 2) & 0x33);
        v  = (v + (v >> 4)) & 0xf;
        r_.u8[i] = v >> (sizeof(uint8_t) - 1) * CHAR_BIT;
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_popcnt_epi8
  #define _mm256_popcnt_epi8(a) simde_mm256_popcnt_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_popcnt_epi8 (simde__m256i src, simde__mmask32 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_popcnt_epi8(src, k, a);
  #else
    return simde_mm256_mask_mov_epi8(src, k, simde_mm256_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_popcnt_epi8
  #define _mm256_mask_popcnt_epi8(src, k, a) simde_mm256_mask_popcnt_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_popcnt_epi8 (simde__mmask32 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_popcnt_epi8(k, a);
  #else
    return simde_mm256_maskz_mov_epi8(k, simde_mm256_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_popcnt_epi8
  #define _mm256_maskz_popcnt_epi8(k, a) simde_mm256_maskz_popcnt_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_popcnt_epi16 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_popcnt_epi16(a);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi16(a_.m128i[i]);
      }
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      r_.n =
        _mm256_sub_epi16(
          a_.n,
          _mm256_and_si256(
            _mm256_srli_epi16(a_.n, 1),
            _mm256_set1_epi16(0x5555)
          )
        );

      r_.n =
        _mm256_add_epi16(
          _mm256_and_si256(
            r_.n,
            _mm256_set1_epi16(0x3333)
          ),
          _mm256_and_si256(
            _mm256_srli_epi16(r_.n, 2),
            _mm256_set1_epi16(0x3333)
          )
        );

      r_.n =
        _mm256_and_si256(
          _mm256_add_epi16(
            r_.n,
            _mm256_srli_epi16(r_.n, 4)
          ),
          _mm256_set1_epi16(0x0f0f)
        );

      r_.n =
        _mm256_srli_epi16(
          _mm256_mullo_epi16(
            r_.n,
            _mm256_set1_epi16(0x0101)
          ),
          (sizeof(uint16_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u16 -= ((a_.u16 >> 1) & UINT16_C(0x5555));
      a_.u16  = ((a_.u16 & UINT16_C(0x3333)) + ((a_.u16 >> 2) & UINT16_C(0x3333)));
      a_.u16  = (a_.u16 + (a_.u16 >> 4)) & UINT16_C(0x0f0f);
      r_.u16  = (a_.u16 * UINT16_C(0x0101)) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        uint16_t v = HEDLEY_STATIC_CAST(uint16_t, a_.u16[i]);
        v -= ((v >> 1) & UINT16_C(0x5555));
        v  = ((v & UINT16_C(0x3333)) + ((v >> 2) & UINT16_C(0x3333)));
        v  = (v + (v >> 4)) & UINT16_C(0x0f0f);
        r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, (v * UINT16_C(0x0101))) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_popcnt_epi16
  #define _mm256_popcnt_epi16(a) simde_mm256_popcnt_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_popcnt_epi16 (simde__m256i src, simde__mmask16 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_popcnt_epi16(src, k, a);
  #else
    return simde_mm256_mask_mov_epi16(src, k, simde_mm256_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_popcnt_epi16
  #define _mm256_mask_popcnt_epi16(src, k, a) simde_mm256_mask_popcnt_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_popcnt_epi16 (simde__mmask16 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_popcnt_epi16(k, a);
  #else
    return simde_mm256_maskz_mov_epi16(k, simde_mm256_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_popcnt_epi16
  #define _mm256_maskz_popcnt_epi16(k, a) simde_mm256_maskz_popcnt_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_popcnt_epi32 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_popcnt_epi32(a);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi32(a_.m128i[i]);
      }
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      r_.n =
        _mm256_sub_epi32(
          a_.n,
          _mm256_and_si256(
            _mm256_srli_epi32(a_.n, 1),
            _mm256_set1_epi32(0x55555555)
          )
        );

      r_.n =
        _mm256_add_epi32(
          _mm256_and_si256(
            r_.n,
            _mm256_set1_epi32(0x33333333)
          ),
          _mm256_and_si256(
            _mm256_srli_epi32(r_.n, 2),
            _mm256_set1_epi32(0x33333333)
          )
        );

      r_.n =
        _mm256_and_si256(
          _mm256_add_epi32(
            r_.n,
            _mm256_srli_epi32(r_.n, 4)
          ),
          _mm256_set1_epi32(0x0f0f0f0f)
        );

      r_.n =
        _mm256_srli_epi32(
          _mm256_mullo_epi32(
            r_.n,
            _mm256_set1_epi32(0x01010101)
          ),
          (sizeof(uint32_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u32 -= ((a_.u32 >> 1) & UINT32_C(0x55555555));
      a_.u32  = ((a_.u32 & UINT32_C(0x33333333)) + ((a_.u32 >> 2) & UINT32_C(0x33333333)));
      a_.u32  = (a_.u32 + (a_.u32 >> 4)) & UINT32_C(0x0f0f0f0f);
      r_.u32  = (a_.u32 * UINT32_C(0x01010101)) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        uint32_t v = HEDLEY_STATIC_CAST(uint32_t, a_.u32[i]);
        v -= ((v >> 1) & UINT32_C(0x55555555));
        v  = ((v & UINT32_C(0x33333333)) + ((v >> 2) & UINT32_C(0x33333333)));
        v  = (v + (v >> 4)) & UINT32_C(0x0f0f0f0f);
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, (v * UINT32_C(0x01010101))) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_popcnt_epi32
  #define _mm256_popcnt_epi32(a) simde_mm256_popcnt_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_popcnt_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_popcnt_epi32(src, k, a);
  #else
    return simde_mm256_mask_mov_epi32(src, k, simde_mm256_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_popcnt_epi32
  #define _mm256_mask_popcnt_epi32(src, k, a) simde_mm256_mask_popcnt_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_popcnt_epi32 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_popcnt_epi32(k, a);
  #else
    return simde_mm256_maskz_mov_epi32(k, simde_mm256_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_popcnt_epi32
  #define _mm256_maskz_popcnt_epi32(k, a) simde_mm256_maskz_popcnt_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_popcnt_epi64 (simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_popcnt_epi64(a);
  #else
    simde__m256i_private
      r_,
      a_ = simde__m256i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < sizeof(r_.m128i) / sizeof(r_.m128i[0]) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi64(a_.m128i[i]);
      }
    #elif defined(SIMDE_X86_AVX2_NATIVE)
      const __m256i low_nibble_set = _mm256_set1_epi8(0x0f);
      const __m256i high_nibble_of_input = _mm256_andnot_si256(low_nibble_set, a_.n);
      const __m256i low_nibble_of_input = _mm256_and_si256(low_nibble_set, a_.n);
      const __m256i lut =
        _mm256_set_epi8(
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
        );

      r_.n =
        _mm256_sad_epu8(
          _mm256_add_epi8(
            _mm256_shuffle_epi8(
              lut,
              low_nibble_of_input
            ),
            _mm256_shuffle_epi8(
              lut,
              _mm256_srli_epi16(high_nibble_of_input, 4)
            )
          ),
          _mm256_setzero_si256()
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u64 -= ((a_.u64 >> 1) & UINT64_C(0x5555555555555555));
      a_.u64  = ((a_.u64 & UINT64_C(0x3333333333333333)) + ((a_.u64 >> 2) & UINT64_C(0x3333333333333333)));
      a_.u64  = (a_.u64 + (a_.u64 >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
      r_.u64  = (a_.u64 * UINT64_C(0x0101010101010101)) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
        uint64_t v = HEDLEY_STATIC_CAST(uint64_t, a_.u64[i]);
        v -= ((v >> 1) & UINT64_C(0x5555555555555555));
        v  = ((v & UINT64_C(0x3333333333333333)) + ((v >> 2) & UINT64_C(0x3333333333333333)));
        v  = (v + (v >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        r_.u64[i] = HEDLEY_STATIC_CAST(uint64_t, (v * UINT64_C(0x0101010101010101))) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m256i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_popcnt_epi64
  #define _mm256_popcnt_epi64(a) simde_mm256_popcnt_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_popcnt_epi64 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_mask_popcnt_epi64(src, k, a);
  #else
    return simde_mm256_mask_mov_epi64(src, k, simde_mm256_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_popcnt_epi64
  #define _mm256_mask_popcnt_epi64(src, k, a) simde_mm256_mask_popcnt_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_popcnt_epi64 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE) && defined(SIMDE_X86_AVX512VL_NATIVE)
    return _mm256_maskz_popcnt_epi64(k, a);
  #else
    return simde_mm256_maskz_mov_epi64(k, simde_mm256_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_popcnt_epi64
  #define _mm256_maskz_popcnt_epi64(k, a) simde_mm256_maskz_popcnt_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_popcnt_epi8 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_popcnt_epi8(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi8(a_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_popcnt_epi8(a_.m256i[i]);
      }
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
      const __m512i low_nibble_set = _mm512_set1_epi8(0x0f);
      const __m512i high_nibble_of_input = _mm512_andnot_si512(low_nibble_set, a_.n);
      const __m512i low_nibble_of_input = _mm512_and_si512(low_nibble_set, a_.n);
      const __m512i lut =
        simde_mm512_set_epi8(
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
        );

      r_.n =
        _mm512_add_epi8(
          _mm512_shuffle_epi8(
            lut,
            low_nibble_of_input
          ),
          _mm512_shuffle_epi8(
            lut,
            _mm512_srli_epi16(
              high_nibble_of_input,
              4
            )
          )
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u8 -= ((a_.u8 >> 1) & 0x55);
      a_.u8  = ((a_.u8 & 0x33) + ((a_.u8 >> 2) & 0x33));
      a_.u8  = (a_.u8 + (a_.u8 >> 4)) & 15;
      r_.u8  = a_.u8 >> ((sizeof(uint8_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u8) / sizeof(r_.u8[0])) ; i++) {
        uint8_t v = HEDLEY_STATIC_CAST(uint8_t, a_.u8[i]);
        v -= ((v >> 1) & 0x55);
        v  = (v & 0x33) + ((v >> 2) & 0x33);
        v  = (v + (v >> 4)) & 0xf;
        r_.u8[i] = v >> (sizeof(uint8_t) - 1) * CHAR_BIT;
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_popcnt_epi8
  #define _mm512_popcnt_epi8(a) simde_mm512_popcnt_epi8(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_popcnt_epi8 (simde__m512i src, simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_mask_popcnt_epi8(src, k, a);
  #else
    return simde_mm512_mask_mov_epi8(src, k, simde_mm512_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_popcnt_epi8
  #define _mm512_mask_popcnt_epi8(src, k, a) simde_mm512_mask_popcnt_epi8(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_popcnt_epi8 (simde__mmask64 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_maskz_popcnt_epi8(k, a);
  #else
    return simde_mm512_maskz_mov_epi8(k, simde_mm512_popcnt_epi8(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_popcnt_epi8
  #define _mm512_maskz_popcnt_epi8(k, a) simde_mm512_maskz_popcnt_epi8(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_popcnt_epi16 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_popcnt_epi16(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi16(a_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_popcnt_epi16(a_.m256i[i]);
      }
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
      r_.n =
        _mm512_sub_epi16(
          a_.n,
          _mm512_and_si512(
            _mm512_srli_epi16(a_.n, 1),
            _mm512_set1_epi16(0x5555)
          )
        );

      r_.n =
        _mm512_add_epi16(
          _mm512_and_si512(
            r_.n,
            _mm512_set1_epi16(0x3333)
          ),
          _mm512_and_si512(
            _mm512_srli_epi16(r_.n, 2),
            _mm512_set1_epi16(0x3333)
          )
        );

      r_.n =
        _mm512_and_si512(
          _mm512_add_epi16(
            r_.n,
            _mm512_srli_epi16(r_.n, 4)
          ),
          _mm512_set1_epi16(0x0f0f)
        );

      r_.n =
        _mm512_srli_epi16(
          _mm512_mullo_epi16(
            r_.n,
            _mm512_set1_epi16(0x0101)
          ),
          (sizeof(uint16_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u16 -= ((a_.u16 >> 1) & UINT16_C(0x5555));
      a_.u16  = ((a_.u16 & UINT16_C(0x3333)) + ((a_.u16 >> 2) & UINT16_C(0x3333)));
      a_.u16  = (a_.u16 + (a_.u16 >> 4)) & UINT16_C(0x0f0f);
      r_.u16  = (a_.u16 * UINT16_C(0x0101)) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u16) / sizeof(r_.u16[0])) ; i++) {
        uint16_t v = HEDLEY_STATIC_CAST(uint16_t, a_.u16[i]);
        v -= ((v >> 1) & UINT16_C(0x5555));
        v  = ((v & UINT16_C(0x3333)) + ((v >> 2) & UINT16_C(0x3333)));
        v  = (v + (v >> 4)) & UINT16_C(0x0f0f);
        r_.u16[i] = HEDLEY_STATIC_CAST(uint16_t, (v * UINT16_C(0x0101))) >> ((sizeof(uint16_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_popcnt_epi16
  #define _mm512_popcnt_epi16(a) simde_mm512_popcnt_epi16(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_popcnt_epi16 (simde__m512i src, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_mask_popcnt_epi16(src, k, a);
  #else
    return simde_mm512_mask_mov_epi16(src, k, simde_mm512_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_popcnt_epi16
  #define _mm512_mask_popcnt_epi16(src, k, a) simde_mm512_mask_popcnt_epi16(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_popcnt_epi16 (simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512BITALG_NATIVE)
    return _mm512_maskz_popcnt_epi16(k, a);
  #else
    return simde_mm512_maskz_mov_epi16(k, simde_mm512_popcnt_epi16(a));
  #endif
}
#if defined(SIMDE_X86_AVX512BITALG_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_popcnt_epi16
  #define _mm512_maskz_popcnt_epi16(k, a) simde_mm512_maskz_popcnt_epi16(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_popcnt_epi32 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_popcnt_epi32(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi32(a_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < (sizeof(r_.m256i) / sizeof(r_.m256i[0])) ; i++) {
        r_.m256i[i] = simde_mm256_popcnt_epi32(a_.m256i[i]);
      }
    #elif defined(SIMDE_X86_AVX512F_NATIVE)
      r_.n =
        _mm512_sub_epi32(
          a_.n,
          _mm512_and_si512(
            _mm512_srli_epi32(a_.n, 1),
            _mm512_set1_epi32(0x55555555)
          )
        );

      r_.n =
        _mm512_add_epi32(
          _mm512_and_si512(
            r_.n,
            _mm512_set1_epi32(0x33333333)
          ),
          _mm512_and_si512(
            _mm512_srli_epi32(r_.n, 2),
            _mm512_set1_epi32(0x33333333)
          )
        );

      r_.n =
        _mm512_and_si512(
          _mm512_add_epi32(
            r_.n,
            _mm512_srli_epi32(r_.n, 4)
          ),
          _mm512_set1_epi32(0x0f0f0f0f)
        );

      r_.n =
        _mm512_srli_epi32(
          _mm512_mullo_epi32(
            r_.n,
            _mm512_set1_epi32(0x01010101)
          ),
          (sizeof(uint32_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u32 -= ((a_.u32 >> 1) & UINT32_C(0x55555555));
      a_.u32  = ((a_.u32 & UINT32_C(0x33333333)) + ((a_.u32 >> 2) & UINT32_C(0x33333333)));
      a_.u32  = (a_.u32 + (a_.u32 >> 4)) & UINT32_C(0x0f0f0f0f);
      r_.u32  = (a_.u32 * UINT32_C(0x01010101)) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u32) / sizeof(r_.u32[0])) ; i++) {
        uint32_t v = HEDLEY_STATIC_CAST(uint32_t, a_.u32[i]);
        v -= ((v >> 1) & UINT32_C(0x55555555));
        v  = ((v & UINT32_C(0x33333333)) + ((v >> 2) & UINT32_C(0x33333333)));
        v  = (v + (v >> 4)) & UINT32_C(0x0f0f0f0f);
        r_.u32[i] = HEDLEY_STATIC_CAST(uint32_t, (v * UINT32_C(0x01010101))) >> ((sizeof(uint32_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_popcnt_epi32
  #define _mm512_popcnt_epi32(a) simde_mm512_popcnt_epi32(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_popcnt_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_mask_popcnt_epi32(src, k, a);
  #else
    return simde_mm512_mask_mov_epi32(src, k, simde_mm512_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_popcnt_epi32
  #define _mm512_mask_popcnt_epi32(src, k, a) simde_mm512_mask_popcnt_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_popcnt_epi32 (simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_maskz_popcnt_epi32(k, a);
  #else
    return simde_mm512_maskz_mov_epi32(k, simde_mm512_popcnt_epi32(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_popcnt_epi32
  #define _mm512_maskz_popcnt_epi32(k, a) simde_mm512_maskz_popcnt_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_popcnt_epi64 (simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_popcnt_epi64(a);
  #else
    simde__m512i_private
      r_,
      a_ = simde__m512i_to_private(a);

    #if SIMDE_NATURAL_VECTOR_SIZE_LE(128)
      for (size_t i = 0 ; i < (sizeof(r_.m128i) / sizeof(r_.m128i[0])) ; i++) {
        r_.m128i[i] = simde_mm_popcnt_epi64(a_.m128i[i]);
      }
    #elif SIMDE_NATURAL_VECTOR_SIZE_LE(256)
      for (size_t i = 0 ; i < sizeof(r_.m256i) / sizeof(r_.m256i[0]) ; i++) {
        r_.m256i[i] = simde_mm256_popcnt_epi64(a_.m256i[i]);
      }
    #elif defined(SIMDE_X86_AVX512BW_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
      const __m512i low_nibble_set = _mm512_set1_epi8(0x0f);
      const __m512i high_nibble_of_input = _mm512_andnot_si512(low_nibble_set, a_.n);
      const __m512i low_nibble_of_input = _mm512_and_si512(low_nibble_set, a_.n);
      const __m512i lut =
        simde_mm512_set_epi8(
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0,
          4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0
        );

      r_.n =
        _mm512_sad_epu8(
          _mm512_add_epi8(
            _mm512_shuffle_epi8(
              lut,
              low_nibble_of_input
            ),
            _mm512_shuffle_epi8(
              lut,
              _mm512_srli_epi16(high_nibble_of_input, 4)
            )
          ),
          _mm512_setzero_si512()
        );
    #elif defined(SIMDE_X86_AVX512F_NATIVE) && defined(SIMDE_X86_AVX512DQ_NATIVE)
      r_.n =
        _mm512_sub_epi64(
          a_.n,
          _mm512_and_si512(
            _mm512_srli_epi64(a_.n, 1),
            _mm512_set1_epi64(0x5555555555555555)
          )
        );

      r_.n =
        _mm512_add_epi64(
          _mm512_and_si512(
            r_.n,
            _mm512_set1_epi64(0x3333333333333333)
          ),
          _mm512_and_si512(
            _mm512_srli_epi64(r_.n, 2),
            _mm512_set1_epi64(0x3333333333333333)
          )
        );

      r_.n =
        _mm512_and_si512(
          _mm512_add_epi64(
            r_.n,
            _mm512_srli_epi64(r_.n, 4)
          ),
          _mm512_set1_epi64(0x0f0f0f0f0f0f0f0f)
        );

      r_.n =
        _mm512_srli_epi64(
          _mm512_mullo_epi64(
            r_.n,
            _mm512_set1_epi64(0x0101010101010101)
          ),
          (sizeof(uint64_t) - 1) * CHAR_BIT
        );
    #elif defined(SIMDE_VECTOR_SUBSCRIPT_SCALAR)
      a_.u64 -= ((a_.u64 >> 1) & UINT64_C(0x5555555555555555));
      a_.u64  = ((a_.u64 & UINT64_C(0x3333333333333333)) + ((a_.u64 >> 2) & UINT64_C(0x3333333333333333)));
      a_.u64  = (a_.u64 + (a_.u64 >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
      r_.u64  = (a_.u64 * UINT64_C(0x0101010101010101)) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
    #else
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.u64) / sizeof(r_.u64[0])) ; i++) {
        uint64_t v = HEDLEY_STATIC_CAST(uint64_t, a_.u64[i]);
        v -= ((v >> 1) & UINT64_C(0x5555555555555555));
        v  = ((v & UINT64_C(0x3333333333333333)) + ((v >> 2) & UINT64_C(0x3333333333333333)));
        v  = (v + (v >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        r_.u64[i] = HEDLEY_STATIC_CAST(uint64_t, (v * UINT64_C(0x0101010101010101))) >> ((sizeof(uint64_t) - 1) * CHAR_BIT);
      }
    #endif

    return simde__m512i_from_private(r_);
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_popcnt_epi64
  #define _mm512_popcnt_epi64(a) simde_mm512_popcnt_epi64(a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_popcnt_epi64 (simde__m512i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_mask_popcnt_epi64(src, k, a);
  #else
    return simde_mm512_mask_mov_epi64(src, k, simde_mm512_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_popcnt_epi64
  #define _mm512_mask_popcnt_epi64(src, k, a) simde_mm512_mask_popcnt_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_popcnt_epi64 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VPOPCNTDQ_NATIVE)
    return _mm512_maskz_popcnt_epi64(k, a);
  #else
    return simde_mm512_maskz_mov_epi64(k, simde_mm512_popcnt_epi64(a));
  #endif
}
#if defined(SIMDE_X86_AVX512VPOPCNTDQ_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_popcnt_epi64
  #define _mm512_maskz_popcnt_epi64(k, a) simde_mm512_maskz_popcnt_epi64(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_POPCNT_H) */
