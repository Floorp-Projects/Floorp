/* MIT License
 *
 * Copyright (c) 2016-2022 INRIA, CMU and Microsoft Corporation
 * Copyright (c) 2022-2023 HACL* Contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __internal_Hacl_Bignum_Base_H
#define __internal_Hacl_Bignum_Base_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>
#include "krml/internal/types.h"
#include "krml/lowstar_endianness.h"
#include "krml/internal/target.h"

#include "internal/Hacl_Krmllib.h"
#include "Hacl_Krmllib.h"
#include "internal/lib_intrinsics.h"

static inline uint32_t
Hacl_Bignum_Base_mul_wide_add2_u32(uint32_t a, uint32_t b, uint32_t c_in, uint32_t *out)
{
    uint32_t out0 = out[0U];
    uint64_t res = (uint64_t)a * (uint64_t)b + (uint64_t)c_in + (uint64_t)out0;
    out[0U] = (uint32_t)res;
    return (uint32_t)(res >> (uint32_t)32U);
}

static inline uint64_t
Hacl_Bignum_Base_mul_wide_add2_u64(uint64_t a, uint64_t b, uint64_t c_in, uint64_t *out)
{
    uint64_t out0 = out[0U];
    FStar_UInt128_uint128
        res =
            FStar_UInt128_add(FStar_UInt128_add(FStar_UInt128_mul_wide(a, b),
                                                FStar_UInt128_uint64_to_uint128(c_in)),
                              FStar_UInt128_uint64_to_uint128(out0));
    out[0U] = FStar_UInt128_uint128_to_uint64(res);
    return FStar_UInt128_uint128_to_uint64(FStar_UInt128_shift_right(res, (uint32_t)64U));
}

static inline uint64_t
Hacl_Bignum_Lib_bn_get_bits_u64(uint32_t len, uint64_t *b, uint32_t i, uint32_t l)
{
    uint32_t i1 = i / (uint32_t)64U;
    uint32_t j = i % (uint32_t)64U;
    uint64_t p1 = b[i1] >> j;
    uint64_t ite;
    if (i1 + (uint32_t)1U < len && (uint32_t)0U < j) {
        ite = p1 | b[i1 + (uint32_t)1U] << ((uint32_t)64U - j);
    } else {
        ite = p1;
    }
    return ite & (((uint64_t)1U << l) - (uint64_t)1U);
}

static inline uint64_t
Hacl_Bignum_Addition_bn_add_eq_len_u64(uint32_t aLen, uint64_t *a, uint64_t *b, uint64_t *res)
{
    uint64_t c = (uint64_t)0U;
    for (uint32_t i = (uint32_t)0U; i < aLen / (uint32_t)4U; i++) {
        uint64_t t1 = a[(uint32_t)4U * i];
        uint64_t t20 = b[(uint32_t)4U * i];
        uint64_t *res_i0 = res + (uint32_t)4U * i;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t1, t20, res_i0);
        uint64_t t10 = a[(uint32_t)4U * i + (uint32_t)1U];
        uint64_t t21 = b[(uint32_t)4U * i + (uint32_t)1U];
        uint64_t *res_i1 = res + (uint32_t)4U * i + (uint32_t)1U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t10, t21, res_i1);
        uint64_t t11 = a[(uint32_t)4U * i + (uint32_t)2U];
        uint64_t t22 = b[(uint32_t)4U * i + (uint32_t)2U];
        uint64_t *res_i2 = res + (uint32_t)4U * i + (uint32_t)2U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t11, t22, res_i2);
        uint64_t t12 = a[(uint32_t)4U * i + (uint32_t)3U];
        uint64_t t2 = b[(uint32_t)4U * i + (uint32_t)3U];
        uint64_t *res_i = res + (uint32_t)4U * i + (uint32_t)3U;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t12, t2, res_i);
    }
    for (uint32_t i = aLen / (uint32_t)4U * (uint32_t)4U; i < aLen; i++) {
        uint64_t t1 = a[i];
        uint64_t t2 = b[i];
        uint64_t *res_i = res + i;
        c = Lib_IntTypes_Intrinsics_add_carry_u64(c, t1, t2, res_i);
    }
    return c;
}

#if defined(__cplusplus)
}
#endif

#define __internal_Hacl_Bignum_Base_H_DEFINED
#endif
