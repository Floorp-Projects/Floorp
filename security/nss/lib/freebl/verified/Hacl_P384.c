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

#include "Hacl_P384.h"

#include "internal/Hacl_Krmllib.h"
#include "internal/Hacl_Bignum_Base.h"

static inline uint64_t
bn_is_eq_mask(uint64_t *x, uint64_t *y)
{
    uint64_t mask = (uint64_t)0xFFFFFFFFFFFFFFFFU;
    KRML_MAYBE_FOR6(i,
                    (uint32_t)0U,
                    (uint32_t)6U,
                    (uint32_t)1U,
                    uint64_t uu____0 = FStar_UInt64_eq_mask(x[i], y[i]);
                    mask = uu____0 & mask;);
    uint64_t mask1 = mask;
    return mask1;
}

static inline uint64_t
bn_sub(uint64_t *a, uint64_t *b, uint64_t *c)
{
    uint64_t c1 = (uint64_t)0U;
    {
        uint64_t t1 = b[(uint32_t)4U * (uint32_t)0U];
        uint64_t t20 = c[(uint32_t)4U * (uint32_t)0U];
        uint64_t *res_i0 = a + (uint32_t)4U * (uint32_t)0U;
        c1 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c1, t1, t20, res_i0);
        uint64_t t10 = b[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t t21 = c[(uint32_t)4U * (uint32_t)0U + (uint32_t)1U];
        uint64_t *res_i1 = a + (uint32_t)4U * (uint32_t)0U + (uint32_t)1U;
        c1 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c1, t10, t21, res_i1);
        uint64_t t11 = b[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t t22 = c[(uint32_t)4U * (uint32_t)0U + (uint32_t)2U];
        uint64_t *res_i2 = a + (uint32_t)4U * (uint32_t)0U + (uint32_t)2U;
        c1 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c1, t11, t22, res_i2);
        uint64_t t12 = b[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t t2 = c[(uint32_t)4U * (uint32_t)0U + (uint32_t)3U];
        uint64_t *res_i = a + (uint32_t)4U * (uint32_t)0U + (uint32_t)3U;
        c1 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c1, t12, t2, res_i);
    }
    KRML_MAYBE_FOR2(i,
                    (uint32_t)4U,
                    (uint32_t)6U,
                    (uint32_t)1U,
                    uint64_t t1 = b[i];
                    uint64_t t2 = c[i];
                    uint64_t *res_i = a + i;
                    c1 = Lib_IntTypes_Intrinsics_sub_borrow_u64(c1, t1, t2, res_i););
    uint64_t c10 = c1;
    return c10;
}

static inline void
bn_from_bytes_be(uint64_t *a, uint8_t *b)
{
    KRML_MAYBE_FOR6(i,
                    (uint32_t)0U,
                    (uint32_t)6U,
                    (uint32_t)1U,
                    uint64_t *os = a;
                    uint64_t u = load64_be(b + ((uint32_t)6U - i - (uint32_t)1U) * (uint32_t)8U);
                    uint64_t x = u;
                    os[i] = x;);
}

static inline void
p384_make_order(uint64_t *n)
{
    n[0U] = (uint64_t)0xecec196accc52973U;
    n[1U] = (uint64_t)0x581a0db248b0a77aU;
    n[2U] = (uint64_t)0xc7634d81f4372ddfU;
    n[3U] = (uint64_t)0xffffffffffffffffU;
    n[4U] = (uint64_t)0xffffffffffffffffU;
    n[5U] = (uint64_t)0xffffffffffffffffU;
}

/**
Private key validation.

  The function returns `true` if a private key is valid and `false` otherwise.

  The argument `private_key` points to 48 bytes of valid memory, i.e., uint8_t[48].

  The private key is valid:
    • 0 < `private_key` < the order of the curve
*/
bool
Hacl_P384_validate_private_key(uint8_t *private_key)
{
    uint64_t bn_sk[6U] = { 0U };
    bn_from_bytes_be(bn_sk, private_key);
    uint64_t tmp[6U] = { 0U };
    p384_make_order(tmp);
    uint64_t c = bn_sub(tmp, bn_sk, tmp);
    uint64_t is_lt_order = (uint64_t)0U - c;
    uint64_t bn_zero[6U] = { 0U };
    uint64_t res = bn_is_eq_mask(bn_sk, bn_zero);
    uint64_t is_eq_zero = res;
    uint64_t res0 = is_lt_order & ~is_eq_zero;
    return res0 == (uint64_t)0xFFFFFFFFFFFFFFFFU;
}
