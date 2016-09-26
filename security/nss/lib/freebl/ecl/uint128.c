/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "uint128.h"

/* helper functions */
uint64_t
mask51(uint128_t x)
{
    return x.lo & MASK51;
}

uint64_t
mask_lower(uint128_t x)
{
    return x.lo;
}

uint128_t
mask51full(uint128_t x)
{
    uint128_t ret = { x.lo & MASK51, 0 };
    return ret;
}

uint128_t
init128x(uint64_t x)
{
    uint128_t ret = { x, 0 };
    return ret;
}

/* arithmetic */

uint128_t
add128(uint128_t a, uint128_t b)
{
    uint128_t ret;
    ret.lo = a.lo + b.lo;
    ret.hi = a.hi + b.hi + (ret.lo < b.lo);
    return ret;
}

/* out = 19 * a */
uint128_t
mul12819(uint128_t a)
{
    uint128_t ret = lshift128(a, 4);
    ret = add128(ret, a);
    ret = add128(ret, a);
    ret = add128(ret, a);
    return ret;
}

uint128_t
mul6464(uint64_t a, uint64_t b)
{
    uint128_t ret;
    uint64_t t0 = ((uint64_t)(uint32_t)a) * ((uint64_t)(uint32_t)b);
    uint64_t t1 = (a >> 32) * ((uint64_t)(uint32_t)b) + (t0 >> 32);
    uint64_t t2 = (b >> 32) * ((uint64_t)(uint32_t)a) + ((uint32_t)t1);
    ret.lo = (((uint64_t)((uint32_t)t2)) << 32) + ((uint32_t)t0);
    ret.hi = (a >> 32) * (b >> 32);
    ret.hi += (t2 >> 32) + (t1 >> 32);
    return ret;
}

/* only defined for n < 64 */
uint128_t
rshift128(uint128_t x, uint8_t n)
{
    uint128_t ret;
    ret.lo = (x.lo >> n) + (x.hi << (64 - n));
    ret.hi = x.hi >> n;
    return ret;
}

/* only defined for n < 64 */
uint128_t
lshift128(uint128_t x, uint8_t n)
{
    uint128_t ret;
    ret.hi = (x.hi << n) + (x.lo >> (64 - n));
    ret.lo = x.lo << n;
    return ret;
}
