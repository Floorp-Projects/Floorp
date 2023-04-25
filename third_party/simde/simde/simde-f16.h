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
 *   2021      Evan Nemerson <evan@nemerson.com>
 */

#include "hedley.h"
#include "simde-common.h"
#include "simde-detect-clang.h"

#if !defined(SIMDE_FLOAT16_H)
#define SIMDE_FLOAT16_H

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

/* Portable version which should work on pretty much any compiler.
 * Obviously you can't rely on compiler support for things like
 * conversion to/from 32-bit floats, so make sure you always use the
 * functions and macros in this file!
 *
 * The portable implementations are (heavily) based on CC0 code by
 * Fabian Giesen: <https://gist.github.com/rygorous/2156668> (see also
 * <https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/>).
 * I have basically just modified it to get rid of some UB (lots of
 * aliasing, right shifting a negative value), use fixed-width types,
 * and work in C. */
#define SIMDE_FLOAT16_API_PORTABLE 1
/* _Float16, per C standard (TS 18661-3;
 * <http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1945.pdf>). */
#define SIMDE_FLOAT16_API_FLOAT16 2
/* clang >= 6.0 supports __fp16 as an interchange format on all
 * targets, but only allows you to use them for arguments and return
 * values on targets which have defined an ABI.  We get around the
 * restriction by wrapping the __fp16 in a struct, but we can't do
 * that on Arm since it would break compatibility with the NEON F16
 * functions. */
#define SIMDE_FLOAT16_API_FP16_NO_ABI 3
/* This is basically __fp16 as specified by Arm, where arugments and
 * return values are raw __fp16 values not structs. */
#define SIMDE_FLOAT16_API_FP16 4

/* Choosing an implementation.  This is a bit rough, but I don't have
 * any ideas on how to improve it.  If you do, patches are definitely
 * welcome. */
#if !defined(SIMDE_FLOAT16_API)
  #if 0 && !defined(__cplusplus)
    /* I haven't found a way to detect this.  It seems like defining
    * __STDC_WANT_IEC_60559_TYPES_EXT__, then including float.h, then
    * checking for defined(FLT16_MAX) should work, but both gcc and
    * clang will define the constants even if _Float16 is not
    * supported.  Ideas welcome. */
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FLOAT16
  #elif defined(__ARM_FP16_FORMAT_IEEE) && defined(SIMDE_ARM_NEON_FP16)
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FP16
  #elif defined(__FLT16_MIN__) && (defined(__clang__) && (!defined(SIMDE_ARCH_AARCH64) || SIMDE_DETECT_CLANG_VERSION_CHECK(7,0,0)))
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_FP16_NO_ABI
  #else
    #define SIMDE_FLOAT16_API SIMDE_FLOAT16_API_PORTABLE
  #endif
#endif

#if SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FLOAT16
  typedef _Float16 simde_float16;
  #define SIMDE_FLOAT16_C(value) value##f16
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16_NO_ABI
  typedef struct { __fp16 value; } simde_float16;
  #if defined(SIMDE_STATEMENT_EXPR_)
    #define SIMDE_FLOAT16_C(value) (__extension__({ ((simde_float16) { HEDLEY_DIAGNOSTIC_PUSH SIMDE_DIAGNOSTIC_DISABLE_C99_EXTENSIONS_ HEDLEY_STATIC_CAST(__fp16, (value)) }); HEDLEY_DIAGNOSTIC_POP }))
  #else
    #define SIMDE_FLOAT16_C(value) ((simde_float16) { HEDLEY_STATIC_CAST(__fp16, (value)) })
  #endif
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16
  typedef __fp16 simde_float16;
  #define SIMDE_FLOAT16_C(value) HEDLEY_STATIC_CAST(__fp16, (value))
#elif SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_PORTABLE
  typedef struct { uint16_t value; } simde_float16;
#else
  #error No 16-bit floating point API.
#endif

#if \
    defined(SIMDE_VECTOR_OPS) && \
    (SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_PORTABLE) && \
    (SIMDE_FLOAT16_API != SIMDE_FLOAT16_API_FP16_NO_ABI)
  #define SIMDE_FLOAT16_VECTOR
#endif

/* Reinterpret -- you *generally* shouldn't need these, they're really
 * intended for internal use.  However, on x86 half-precision floats
 * get stuffed into a __m128i/__m256i, so it may be useful. */

SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_float16_as_uint16,      uint16_t, simde_float16)
SIMDE_DEFINE_CONVERSION_FUNCTION_(simde_uint16_as_float16, simde_float16,      uint16_t)

#define SIMDE_NANHF simde_uint16_as_float16(0x7E00)
#define SIMDE_INFINITYHF simde_uint16_as_float16(0x7C00)

/* Conversion -- convert between single-precision and half-precision
 * floats. */

static HEDLEY_ALWAYS_INLINE HEDLEY_CONST
simde_float16
simde_float16_from_float32 (simde_float32 value) {
  simde_float16 res;

  #if \
      (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FLOAT16) || \
      (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16)
    res = HEDLEY_STATIC_CAST(simde_float16, value);
  #elif (SIMDE_FLOAT16_API == SIMDE_FLOAT16_API_FP16_NO_ABI)
    res.value = HEDLEY_STATIC_CAST(__fp16, value);
  #else
    /* This code is CC0, based heavily on code by Fabian Giesen. */
    uint32_t f32u = simde_float32_as_uint32(value);
    static const uint32_t f32u_infty = UINT32_C(255) << 23;
    static const uint32_t f16u_max = (UINT32_C(127) + UINT32_C(16)) << 23;
    static const uint32_t denorm_magic =
      ((UINT32_C(127) - UINT32_C(15)) + (UINT32_C(23) - UINT32_C(10)) + UINT32_C(1)) << 23;
    uint16_t f16u;

    uint32_t sign = f32u & (UINT32_C(1) << 31);
    f32u ^= sign;

   /* NOTE all the integer compares in this function cast the operands
    * to signed values to help compilers vectorize to SSE2, which lacks
    * unsigned comparison instructions.  This is fine since all
    * operands are below 0x80000000 (we clear the sign bit). */

    if (f32u > f16u_max) { /* result is Inf or NaN (all exponent bits set) */
      f16u = (f32u > f32u_infty) ?  UINT32_C(0x7e00) : UINT32_C(0x7c00); /* NaN->qNaN and Inf->Inf */
    } else { /* (De)normalized number or zero */
      if (f32u < (UINT32_C(113) << 23)) { /* resulting FP16 is subnormal or zero */
        /* use a magic value to align our 10 mantissa bits at the bottom of
        * the float. as long as FP addition is round-to-nearest-even this
        * just works. */
        f32u = simde_float32_as_uint32(simde_uint32_as_float32(f32u) + simde_uint32_as_float32(denorm_magic));

        /* and one integer subtract of the bias later, we have our final float! */
        f16u = HEDLEY_STATIC_CAST(uint16_t, f32u - denorm_magic);
      } else {
        uint32_t mant_odd = (f32u >> 13) & 1;

        /* update exponent, rounding bias part 1 */
        f32u += (HEDLEY_STATIC_CAST(uint32_t, 15 - 127) << 23) + UINT32_C(0xfff);
        /* rounding bias part 2 */
        f32u += mant_odd;
        /* take the bits! */
        f16u = HEDLEY_STATIC_CAST(uint16_t, f32u >> 13);
      }
    }

    f16u |= sign >> 16;
    res = simde_uint16_as_float16(f16u);
  #endif

  return res;
}

static HEDLEY_ALWAYS_INLINE HEDLEY_CONST
simde_float32
simde_float16_to_float32 (simde_float16 value) {
  simde_float32 res;

  #if defined(SIMDE_FLOAT16_FLOAT16) || defined(SIMDE_FLOAT16_FP16)
    res = HEDLEY_STATIC_CAST(simde_float32, value);
  #else
    /* This code is CC0, based heavily on code by Fabian Giesen. */
    uint16_t half = simde_float16_as_uint16(value);
    const simde_float32 denorm_magic = simde_uint32_as_float32((UINT32_C(113) << 23));
    const uint32_t shifted_exp = UINT32_C(0x7c00) << 13; /* exponent mask after shift */
    uint32_t f32u;

    f32u = (half & UINT32_C(0x7fff)) << 13; /* exponent/mantissa bits */
    uint32_t exp = shifted_exp & f32u; /* just the exponent */
    f32u += (UINT32_C(127) - UINT32_C(15)) << 23; /* exponent adjust */

    /* handle exponent special cases */
    if (exp == shifted_exp) /* Inf/NaN? */
      f32u += (UINT32_C(128) - UINT32_C(16)) << 23; /* extra exp adjust */
    else if (exp == 0) { /* Zero/Denormal? */
      f32u += (1) << 23; /* extra exp adjust */
      f32u = simde_float32_as_uint32(simde_uint32_as_float32(f32u) - denorm_magic); /* renormalize */
    }

    f32u |= (half & UINT32_C(0x8000)) << 16; /* sign bit */
    res = simde_uint32_as_float32(f32u);
  #endif

  return res;
}

#ifdef SIMDE_FLOAT16_C
  #define SIMDE_FLOAT16_VALUE(value) SIMDE_FLOAT16_C(value)
#else
  #define SIMDE_FLOAT16_VALUE(value) simde_float16_from_float32(SIMDE_FLOAT32_C(value))
#endif

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_FLOAT16_H) */
