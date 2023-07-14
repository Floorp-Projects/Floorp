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

#if !defined(SIMDE_MIPS_MSA_LD_H)
#define SIMDE_MIPS_MSA_LD_H

#include "types.h"

HEDLEY_DIAGNOSTIC_PUSH
SIMDE_DISABLE_UNWANTED_DIAGNOSTICS
SIMDE_BEGIN_DECLS_

SIMDE_FUNCTION_ATTRIBUTES
simde_v16i8
simde_msa_ld_b(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_ld_b(rs, s10);
  #else
    simde_v16i8 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_ld_b
  #define __msa_ld_b(rs, s10) simde_msa_ld_b((rs), (s10))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v8i16
simde_msa_ld_h(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int16_t)) == 0, "`s10' must be a multiple of sizeof(int16_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_ld_h(rs, s10);
  #else
    simde_v8i16 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_ld_h
  #define __msa_ld_h(rs, s10) simde_msa_ld_h((rs), (s10))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v4i32
simde_msa_ld_w(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int32_t)) == 0, "`s10' must be a multiple of sizeof(int32_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_ld_w(rs, s10);
  #else
    simde_v4i32 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_ld_w
  #define __msa_ld_w(rs, s10) simde_msa_ld_w((rs), (s10))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v2i64
simde_msa_ld_d(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int64_t)) == 0, "`s10' must be a multiple of sizeof(int64_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return __msa_ld_d(rs, s10);
  #else
    simde_v2i64 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}
#if defined(SIMDE_MIPS_MSA_ENABLE_NATIVE_ALIASES)
  #undef __msa_ld_d
  #define __msa_ld_d(rs, s10) simde_msa_ld_d((rs), (s10))
#endif

SIMDE_FUNCTION_ATTRIBUTES
simde_v16u8
simde_x_msa_ld_u_b(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023) {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v16u8, __msa_ld_b(rs, s10));
  #else
    simde_v16u8 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_v8u16
simde_x_msa_ld_u_h(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int16_t)) == 0, "`s10' must be a multiple of sizeof(int16_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v8u16, __msa_ld_b(rs, s10));
  #else
    simde_v8u16 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_v4u32
simde_x_msa_ld_u_w(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int32_t)) == 0, "`s10' must be a multiple of sizeof(int32_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v4u32, __msa_ld_b(rs, s10));
  #else
    simde_v4u32 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_v2u64
simde_x_msa_ld_u_d(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int64_t)) == 0, "`s10' must be a multiple of sizeof(int64_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v2u64, __msa_ld_b(rs, s10));
  #else
    simde_v2u64 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_v4f32
simde_x_msa_fld_w(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int32_t)) == 0, "`s10' must be a multiple of sizeof(int32_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v4f32, __msa_ld_b(rs, s10));
  #else
    simde_v4f32 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_FUNCTION_ATTRIBUTES
simde_v2f64
simde_x_msa_fld_d(const void * rs, const int s10)
    SIMDE_REQUIRE_CONSTANT_RANGE(s10, 0, 1023)
    HEDLEY_REQUIRE_MSG((s10 % sizeof(int64_t)) == 0, "`s10' must be a multiple of sizeof(int64_t)") {
  #if defined(SIMDE_MIPS_MSA_NATIVE)
    return HEDLEY_REINTERPRET_CAST(simde_v2f64, __msa_ld_b(rs, s10));
  #else
    simde_v2f64 r;

    simde_memcpy(&r, &(HEDLEY_REINTERPRET_CAST(const int8_t*, rs)[s10]), sizeof(r));

    return r;
  #endif
}

SIMDE_END_DECLS_
HEDLEY_DIAGNOSTIC_POP

#endif /* !defined(SIMDE_MIPS_MSA_LD_H) */
