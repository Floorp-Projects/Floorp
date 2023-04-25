/* SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Copyright:
 *   2020      Evan Nemerson <evan@nemerson.com>
 *   2021      Zhi An Ng <zhin@google.com> (Copyright owned by Google, LLC)
 */

#if !defined(SIMDE_ARM_NEON_RSQRTE_H)
#define SIMDE_ARM_NEON_RSQRTE_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_float32_t
simde_vrsqrtes_f32(simde_float32_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrtes_f32(a);
  #else
    #if defined(SIMDE_IEEE754_STORAGE)
      /* https://basesandframes.files.wordpress.com/2020/04/even_faster_math_functions_green_2020.pdf
        Pages 100 - 103 */
      #if SIMDE_ACCURACY_PREFERENCE <= 0
        return (INT32_C(0x5F37624F) - (a >> 1));
      #else
        simde_float32 x = a;
        simde_float32 xhalf = SIMDE_FLOAT32_C(0.5) * x;
        int32_t ix;

        simde_memcpy(&ix, &x, sizeof(ix));

        #if SIMDE_ACCURACY_PREFERENCE == 1
          ix = INT32_C(0x5F375A82) - (ix >> 1);
        #else
          ix = INT32_C(0x5F37599E) - (ix >> 1);
        #endif

        simde_memcpy(&x, &ix, sizeof(x));

        #if SIMDE_ACCURACY_PREFERENCE >= 2
          x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);
        #endif
          x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);
        return x;
      #endif
    #elif defined(simde_math_sqrtf)
      return 1.0f / simde_math_sqrtf(a);
    #else
      HEDLEY_UNREACHABLE();
    #endif
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrtes_f32
  #define vrsqrtes_f32(a) simde_vrsqrtes_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64_t
simde_vrsqrted_f64(simde_float64_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrted_f64(a);
  #else
    #if defined(SIMDE_IEEE754_STORAGE)
      //https://www.mdpi.com/1099-4300/23/1/86/htm
      simde_float64_t x = a;
      simde_float64_t xhalf = SIMDE_FLOAT64_C(0.5) * x;
      int64_t ix;

      simde_memcpy(&ix, &x, sizeof(ix));
      ix = INT64_C(0x5FE6ED2102DCBFDA) - (ix >> 1);
      simde_memcpy(&x, &ix, sizeof(x));
      x = x * (SIMDE_FLOAT64_C(1.50087895511633457) - xhalf * x * x);
      x = x * (SIMDE_FLOAT64_C(1.50000057967625766) - xhalf * x * x);
      return x;
    #elif defined(simde_math_sqrtf)
      return SIMDE_FLOAT64_C(1.0) / simde_math_sqrt(a_.values[i]);
    #else
      HEDLEY_UNREACHABLE();
    #endif
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrted_f64
  #define vrsqrted_f64(a) simde_vrsqrted_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x2_t
simde_vrsqrte_u32(simde_uint32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrte_u32(a);
  #else
    simde_uint32x2_private
      a_ = simde_uint32x2_to_private(a),
      r_;

    for(size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[i])) ; i++) {
      if(a_.values[i] < 0x3FFFFFFF) {
        r_.values[i] = UINT32_MAX;
      } else {
        uint32_t a_temp = (a_.values[i] >> 23) & 511;
        if(a_temp < 256) {
          a_temp = a_temp * 2 + 1;
        } else {
          a_temp = (a_temp >> 1) << 1;
          a_temp = (a_temp + 1) * 2;
        }
        uint32_t b = 512;
        while((a_temp * (b + 1) * (b + 1)) < (1 << 28))
          b = b + 1;
        r_.values[i] = (b + 1) / 2;
        r_.values[i] = r_.values[i] << 23;
      }
    }
    return simde_uint32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrte_u32
  #define vrsqrte_u32(a) simde_vrsqrte_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x2_t
simde_vrsqrte_f32(simde_float32x2_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrte_f32(a);
  #else
    simde_float32x2_private
      r_,
      a_ = simde_float32x2_to_private(a);

    #if defined(SIMDE_IEEE754_STORAGE)
      /* https://basesandframes.files.wordpress.com/2020/04/even_faster_math_functions_green_2020.pdf
        Pages 100 - 103 */
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        #if SIMDE_ACCURACY_PREFERENCE <= 0
          r_.i32[i] = INT32_C(0x5F37624F) - (a_.i32[i] >> 1);
        #else
          simde_float32 x = a_.values[i];
          simde_float32 xhalf = SIMDE_FLOAT32_C(0.5) * x;
          int32_t ix;

          simde_memcpy(&ix, &x, sizeof(ix));

          #if SIMDE_ACCURACY_PREFERENCE == 1
            ix = INT32_C(0x5F375A82) - (ix >> 1);
          #else
            ix = INT32_C(0x5F37599E) - (ix >> 1);
          #endif

          simde_memcpy(&x, &ix, sizeof(x));

          #if SIMDE_ACCURACY_PREFERENCE >= 2
            x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);
          #endif
          x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);

          r_.values[i] = x;
        #endif
      }
    #elif defined(simde_math_sqrtf)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
        r_.f32[i] = 1.0f / simde_math_sqrtf(a_.f32[i]);
      }
    #else
      HEDLEY_UNREACHABLE();
    #endif

    return simde_float32x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrte_f32
  #define vrsqrte_f32(a) simde_vrsqrte_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x1_t
simde_vrsqrte_f64(simde_float64x1_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrte_f64(a);
  #else
    simde_float64x1_private
      r_,
      a_ = simde_float64x1_to_private(a);

    #if defined(SIMDE_IEEE754_STORAGE)
      //https://www.mdpi.com/1099-4300/23/1/86/htm
      SIMDE_VECTORIZE
      for(size_t i = 0 ; i < (sizeof(r_.values)/sizeof(r_.values[0])) ; i++) {
        simde_float64_t x = a_.values[i];
        simde_float64_t xhalf = SIMDE_FLOAT64_C(0.5) * x;
        int64_t ix;

        simde_memcpy(&ix, &x, sizeof(ix));
        ix = INT64_C(0x5FE6ED2102DCBFDA) - (ix >> 1);
        simde_memcpy(&x, &ix, sizeof(x));
        x = x * (SIMDE_FLOAT64_C(1.50087895511633457) - xhalf * x * x);
        x = x * (SIMDE_FLOAT64_C(1.50000057967625766) - xhalf * x * x);
        r_.values[i] = x;
      }
    #elif defined(simde_math_sqrtf)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = SIMDE_FLOAT64_C(1.0) / simde_math_sqrt(a_.values[i]);
      }
    #else
      HEDLEY_UNREACHABLE();
    #endif

    return simde_float64x1_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrte_f64
  #define vrsqrte_f64(a) simde_vrsqrte_f64((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_uint32x4_t
simde_vrsqrteq_u32(simde_uint32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrteq_u32(a);
  #else
    simde_uint32x4_private
      a_ = simde_uint32x4_to_private(a),
      r_;

    for(size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[i])) ; i++) {
      if(a_.values[i] < 0x3FFFFFFF) {
        r_.values[i] = UINT32_MAX;
      } else {
        uint32_t a_temp = (a_.values[i] >> 23) & 511;
        if(a_temp < 256) {
          a_temp = a_temp * 2 + 1;
        } else {
          a_temp = (a_temp >> 1) << 1;
          a_temp = (a_temp + 1) * 2;
        }
        uint32_t b = 512;
        while((a_temp * (b + 1) * (b + 1)) < (1 << 28))
          b = b + 1;
        r_.values[i] = (b + 1) / 2;
        r_.values[i] = r_.values[i] << 23;
      }
    }
    return simde_uint32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrteq_u32
  #define vrsqrteq_u32(a) simde_vrsqrteq_u32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float32x4_t
simde_vrsqrteq_f32(simde_float32x4_t a) {
  #if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    return vrsqrteq_f32(a);
  #elif defined(SIMDE_POWER_ALTIVEC_P6_NATIVE)
    return vec_rsqrte(a);
  #else
    simde_float32x4_private
      r_,
      a_ = simde_float32x4_to_private(a);

    #if defined(SIMDE_X86_SSE_NATIVE)
      r_.m128 = _mm_rsqrt_ps(a_.m128);
    #elif defined(SIMDE_IEEE754_STORAGE)
      /* https://basesandframes.files.wordpress.com/2020/04/even_faster_math_functions_green_2020.pdf
        Pages 100 - 103 */
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        #if SIMDE_ACCURACY_PREFERENCE <= 0
          r_.i32[i] = INT32_C(0x5F37624F) - (a_.i32[i] >> 1);
        #else
          simde_float32 x = a_.values[i];
          simde_float32 xhalf = SIMDE_FLOAT32_C(0.5) * x;
          int32_t ix;

          simde_memcpy(&ix, &x, sizeof(ix));

          #if SIMDE_ACCURACY_PREFERENCE == 1
            ix = INT32_C(0x5F375A82) - (ix >> 1);
          #else
            ix = INT32_C(0x5F37599E) - (ix >> 1);
          #endif

          simde_memcpy(&x, &ix, sizeof(x));

          #if SIMDE_ACCURACY_PREFERENCE >= 2
            x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);
          #endif
          x = x * (SIMDE_FLOAT32_C(1.5008909) - xhalf * x * x);

          r_.values[i] = x;
        #endif
      }
    #elif defined(simde_math_sqrtf)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.f32) / sizeof(r_.f32[0])) ; i++) {
        r_.f32[i] = 1.0f / simde_math_sqrtf(a_.f32[i]);
      }
    #else
      HEDLEY_UNREACHABLE();
    #endif

    return simde_float32x4_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A32V7_ENABLE_NATIVE_ALIASES)
  #undef vrsqrteq_f32
  #define vrsqrteq_f32(a) simde_vrsqrteq_f32((a))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_float64x2_t
simde_vrsqrteq_f64(simde_float64x2_t a) {
  #if defined(SIMDE_ARM_NEON_A64V8_NATIVE)
    return vrsqrteq_f64(a);
  #else
    simde_float64x2_private
      r_,
      a_ = simde_float64x2_to_private(a);

    #if defined(SIMDE_IEEE754_STORAGE)
      //https://www.mdpi.com/1099-4300/23/1/86/htm
      SIMDE_VECTORIZE
      for(size_t i = 0 ; i < (sizeof(r_.values)/sizeof(r_.values[0])) ; i++) {
        simde_float64_t x = a_.values[i];
        simde_float64_t xhalf = SIMDE_FLOAT64_C(0.5) * x;
        int64_t ix;

        simde_memcpy(&ix, &x, sizeof(ix));
        ix = INT64_C(0x5FE6ED2102DCBFDA) - (ix >> 1);
        simde_memcpy(&x, &ix, sizeof(x));
        x = x * (SIMDE_FLOAT64_C(1.50087895511633457) - xhalf * x * x);
        x = x * (SIMDE_FLOAT64_C(1.50000057967625766) - xhalf * x * x);
        r_.values[i] = x;
      }
    #elif defined(simde_math_sqrtf)
      SIMDE_VECTORIZE
      for (size_t i = 0 ; i < (sizeof(r_.values) / sizeof(r_.values[0])) ; i++) {
        r_.values[i] = SIMDE_FLOAT64_C(1.0) / simde_math_sqrt(a_.values[i]);
      }
    #else
      HEDLEY_UNREACHABLE();
    #endif

    return simde_float64x2_from_private(r_);
  #endif
}
#if defined(SIMDE_ARM_NEON_A64V8_ENABLE_NATIVE_ALIASES)
  #undef vrsqrteq_f64
  #define vrsqrteq_f64(a) simde_vrsqrteq_f64((a))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP
#endif /* !defined(SIMDE_ARM_NEON_RSQRTE_H) */
