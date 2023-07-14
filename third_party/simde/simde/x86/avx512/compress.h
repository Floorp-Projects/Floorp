#if !defined(SIMDE_X86_AVX512_COMPRESS_H)
#define SIMDE_X86_AVX512_COMPRESS_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_mask_compress_pd (simde__m256d src, simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_mask_compress_pd(src, k, a);
  #else
    simde__m256d_private
      a_ = simde__m256d_to_private(a),
      src_ = simde__m256d_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    for ( ; ri < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; ri++) {
      a_.f64[ri] = src_.f64[ri];
    }

    return simde__m256d_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compress_pd
  #define _mm256_mask_compress_pd(src, k, a) simde_mm256_mask_compress_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm256_mask_compressstoreu_pd (void* base_addr, simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm256_mask_compressstoreu_pd(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm256_mask_storeu_pd(base_addr, store_mask, _mm256_maskz_compress_pd(k, a));
  #else
    simde__m256d_private
      a_ = simde__m256d_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.f64[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compressstoreu_pd
  #define _mm256_mask_compressstoreu_pd(base_addr, k, a) simde_mm256_mask_compressstoreu_pd(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256d
simde_mm256_maskz_compress_pd (simde__mmask8 k, simde__m256d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_maskz_compress_pd(k, a);
  #else
    simde__m256d_private
      a_ = simde__m256d_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    for ( ; ri < (sizeof(a_.f64) / sizeof(a_.f64[0])); ri++) {
      a_.f64[ri] = SIMDE_FLOAT64_C(0.0);
    }

    return simde__m256d_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_compress_pd
  #define _mm256_maskz_compress_pd(k, a) simde_mm256_maskz_compress_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_mask_compress_ps (simde__m256 src, simde__mmask8 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_mask_compress_ps(src, k, a);
  #else
    simde__m256_private
      a_ = simde__m256_to_private(a),
      src_ = simde__m256_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    for ( ; ri < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; ri++) {
      a_.f32[ri] = src_.f32[ri];
    }

    return simde__m256_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compress_ps
  #define _mm256_mask_compress_ps(src, k, a) simde_mm256_mask_compress_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm256_mask_compressstoreu_ps (void* base_addr, simde__mmask8 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm256_mask_compressstoreu_ps(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm256_mask_storeu_ps(base_addr, store_mask, _mm256_maskz_compress_ps(k, a));
  #else
    simde__m256_private
      a_ = simde__m256_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.f32[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compressstoreu_pd
  #define _mm256_mask_compressstoreu_ps(base_addr, k, a) simde_mm256_mask_compressstoreu_ps(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256
simde_mm256_maskz_compress_ps (simde__mmask8 k, simde__m256 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_maskz_compress_ps(k, a);
  #else
    simde__m256_private
      a_ = simde__m256_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    for ( ; ri < (sizeof(a_.f32) / sizeof(a_.f32[0])); ri++) {
      a_.f32[ri] = SIMDE_FLOAT32_C(0.0);
    }

    return simde__m256_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_compress_ps
  #define _mm256_maskz_compress_ps(k, a) simde_mm256_maskz_compress_ps(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_compress_epi32 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_mask_compress_epi32(src, k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      src_ = simde__m256i_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    for ( ; ri < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; ri++) {
      a_.i32[ri] = src_.i32[ri];
    }

    return simde__m256i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compress_epi32
  #define _mm256_mask_compress_epi32(src, k, a) simde_mm256_mask_compress_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm256_mask_compressstoreu_epi32 (void* base_addr, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm256_mask_compressstoreu_epi32(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm256_mask_storeu_epi32(base_addr, store_mask, _mm256_maskz_compress_epi32(k, a));
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.i32[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compressstoreu_epi32
  #define _mm256_mask_compressstoreu_epi32(base_addr, k, a) simde_mm256_mask_compressstoreu_epi32(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_compress_epi32 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_maskz_compress_epi32(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    for ( ; ri < (sizeof(a_.i32) / sizeof(a_.i32[0])); ri++) {
      a_.f32[ri] = INT32_C(0);
    }

    return simde__m256i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_compress_epi32
  #define _mm256_maskz_compress_epi32(k, a) simde_mm256_maskz_compress_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_mask_compress_epi64 (simde__m256i src, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_mask_compress_epi64(src, k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a),
      src_ = simde__m256i_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    for ( ; ri < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; ri++) {
      a_.i64[ri] = src_.i64[ri];
    }

    return simde__m256i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compress_epi64
  #define _mm256_mask_compress_epi64(src, k, a) simde_mm256_mask_compress_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm256_mask_compressstoreu_epi64 (void* base_addr, simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm256_mask_compressstoreu_epi64(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm256_mask_storeu_epi64(base_addr, store_mask, _mm256_maskz_compress_epi64(k, a));
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.i64[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_mask_compressstoreu_epi64
  #define _mm256_mask_compressstoreu_epi64(base_addr, k, a) simde_mm256_mask_compressstoreu_epi64(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m256i
simde_mm256_maskz_compress_epi64 (simde__mmask8 k, simde__m256i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm256_maskz_compress_epi64(k, a);
  #else
    simde__m256i_private
      a_ = simde__m256i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    for ( ; ri < (sizeof(a_.i64) / sizeof(a_.i64[0])); ri++) {
      a_.i64[ri] = INT64_C(0);
    }

    return simde__m256i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm256_maskz_compress_epi64
  #define _mm256_maskz_compress_epi64(k, a) simde_mm256_maskz_compress_epi64(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_mask_compress_pd (simde__m512d src, simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_compress_pd(src, k, a);
  #else
    simde__m512d_private
      a_ = simde__m512d_to_private(a),
      src_ = simde__m512d_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    for ( ; ri < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; ri++) {
      a_.f64[ri] = src_.f64[ri];
    }

    return simde__m512d_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compress_pd
  #define _mm512_mask_compress_pd(src, k, a) simde_mm512_mask_compress_pd(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_compressstoreu_pd (void* base_addr, simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm512_mask_compressstoreu_pd(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm512_mask_storeu_pd(base_addr, store_mask, _mm512_maskz_compress_pd(k, a));
  #else
    simde__m512d_private
      a_ = simde__m512d_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.f64[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compressstoreu_pd
  #define _mm512_mask_compressstoreu_pd(base_addr, k, a) simde_mm512_mask_compressstoreu_pd(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512d
simde_mm512_maskz_compress_pd (simde__mmask8 k, simde__m512d a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_compress_pd(k, a);
  #else
    simde__m512d_private
      a_ = simde__m512d_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f64) / sizeof(a_.f64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f64[ri++] = a_.f64[i];
      }
    }

    for ( ; ri < (sizeof(a_.f64) / sizeof(a_.f64[0])); ri++) {
      a_.f64[ri] = SIMDE_FLOAT64_C(0.0);
    }

    return simde__m512d_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_compress_pd
  #define _mm512_maskz_compress_pd(k, a) simde_mm512_maskz_compress_pd(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_mask_compress_ps (simde__m512 src, simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_compress_ps(src, k, a);
  #else
    simde__m512_private
      a_ = simde__m512_to_private(a),
      src_ = simde__m512_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    for ( ; ri < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; ri++) {
      a_.f32[ri] = src_.f32[ri];
    }

    return simde__m512_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compress_ps
  #define _mm512_mask_compress_ps(src, k, a) simde_mm512_mask_compress_ps(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_compressstoreu_ps (void* base_addr, simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm512_mask_compressstoreu_ps(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask16 store_mask = _pext_u32(-1, k);
    _mm512_mask_storeu_ps(base_addr, store_mask, _mm512_maskz_compress_ps(k, a));
  #else
    simde__m512_private
      a_ = simde__m512_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.f32[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compressstoreu_pd
  #define _mm512_mask_compressstoreu_ps(base_addr, k, a) simde_mm512_mask_compressstoreu_ps(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512
simde_mm512_maskz_compress_ps (simde__mmask16 k, simde__m512 a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_compress_ps(k, a);
  #else
    simde__m512_private
      a_ = simde__m512_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.f32) / sizeof(a_.f32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.f32[ri++] = a_.f32[i];
      }
    }

    for ( ; ri < (sizeof(a_.f32) / sizeof(a_.f32[0])); ri++) {
      a_.f32[ri] = SIMDE_FLOAT32_C(0.0);
    }

    return simde__m512_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_compress_ps
  #define _mm512_maskz_compress_ps(k, a) simde_mm512_maskz_compress_ps(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_compress_epi32 (simde__m512i src, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_compress_epi32(src, k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      src_ = simde__m512i_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    for ( ; ri < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; ri++) {
      a_.i32[ri] = src_.i32[ri];
    }

    return simde__m512i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compress_epi32
  #define _mm512_mask_compress_epi32(src, k, a) simde_mm512_mask_compress_epi32(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_compressstoreu_epi16 (void* base_addr, simde__mmask32 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VBMI2_NATIVE) && !defined(__znver4__)
    _mm512_mask_compressstoreu_epi16(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VBMI2_NATIVE) && defined(__znver4__)
    simde__mmask32 store_mask = _pext_u32(-1, k);
    _mm512_mask_storeu_epi16(base_addr, store_mask, _mm512_maskz_compress_epi16(k, a));
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i16) / sizeof(a_.i16[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i16[ri++] = a_.i16[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.i16[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VBMI2_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compressstoreu_epi16
  #define _mm512_mask_compressstoreu_epi16(base_addr, k, a) simde_mm512_mask_compressstoreu_epi16(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_compressstoreu_epi32 (void* base_addr, simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm512_mask_compressstoreu_epi32(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask16 store_mask = _pext_u32(-1, k);
    _mm512_mask_storeu_epi32(base_addr, store_mask, _mm512_maskz_compress_epi32(k, a));
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.i32[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compressstoreu_epi32
  #define _mm512_mask_compressstoreu_epi32(base_addr, k, a) simde_mm512_mask_compressstoreu_epi32(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_compress_epi32 (simde__mmask16 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_compress_epi32(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i32) / sizeof(a_.i32[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i32[ri++] = a_.i32[i];
      }
    }

    for ( ; ri < (sizeof(a_.i32) / sizeof(a_.i32[0])); ri++) {
      a_.f32[ri] = INT32_C(0);
    }

    return simde__m512i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_compress_epi32
  #define _mm512_maskz_compress_epi32(k, a) simde_mm512_maskz_compress_epi32(k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_mask_compress_epi64 (simde__m512i src, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_mask_compress_epi64(src, k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a),
      src_ = simde__m512i_to_private(src);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    for ( ; ri < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; ri++) {
      a_.i64[ri] = src_.i64[ri];
    }

    return simde__m512i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compress_epi64
  #define _mm512_mask_compress_epi64(src, k, a) simde_mm512_mask_compress_epi64(src, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
void
simde_mm512_mask_compressstoreu_epi64 (void* base_addr, simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && !defined(__znver4__)
    _mm512_mask_compressstoreu_epi64(base_addr, k, a);
  #elif defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE) && defined(__znver4__)
    simde__mmask8 store_mask = _pext_u32(-1, k);
    _mm512_mask_storeu_epi64(base_addr, store_mask, _mm512_maskz_compress_epi64(k, a));
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    simde_memcpy(base_addr, &a_, ri * sizeof(a_.i64[0]));

    return;
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_mask_compressstoreu_epi64
  #define _mm512_mask_compressstoreu_epi64(base_addr, k, a) simde_mm512_mask_compressstoreu_epi64(base_addr, k, a)
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde__m512i
simde_mm512_maskz_compress_epi64 (simde__mmask8 k, simde__m512i a) {
  #if defined(SIMDE_X86_AVX512VL_NATIVE) && defined(SIMDE_X86_AVX512F_NATIVE)
    return _mm512_maskz_compress_epi64(k, a);
  #else
    simde__m512i_private
      a_ = simde__m512i_to_private(a);
    size_t ri = 0;

    SIMDE_VECTORIZE
    for (size_t i = 0 ; i < (sizeof(a_.i64) / sizeof(a_.i64[0])) ; i++) {
      if ((k >> i) & 1) {
        a_.i64[ri++] = a_.i64[i];
      }
    }

    for ( ; ri < (sizeof(a_.i64) / sizeof(a_.i64[0])); ri++) {
      a_.i64[ri] = INT64_C(0);
    }

    return simde__m512i_from_private(a_);
  #endif
}
#if defined(SIMDE_X86_AVX512VL_ENABLE_NATIVE_ALIASES) && defined(SIMDE_X86_AVX512F_ENABLE_NATIVE_ALIASES)
  #undef _mm512_maskz_compress_epi64
  #define _mm512_maskz_compress_epi64(k, a) simde_mm512_maskz_compress_epi64(k, a)
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_X86_AVX512_COMPRESS_H) */
