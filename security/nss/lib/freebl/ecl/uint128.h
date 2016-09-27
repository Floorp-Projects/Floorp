/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#define MASK51 0x7ffffffffffffULL

#ifdef HAVE_INT128_SUPPORT
typedef unsigned __int128 uint128_t;
#define add128(a, b) (a) + (b)
#define mul6464(a, b) (uint128_t)(a) * (uint128_t)(b)
#define mul12819(a) (uint128_t)(a) * 19
#define rshift128(x, n) (x) >> (n)
#define lshift128(x, n) (x) << (n)
#define mask51(x) (x) & 0x7ffffffffffff
#define mask_lower(x) (uint64_t)(x)
#define mask51full(x) (x) & 0x7ffffffffffff
#define init128x(x) (x)
#else /* uint128_t for Windows and 32 bit intel systems */
struct uint128_t_str {
    uint64_t lo;
    uint64_t hi;
};
typedef struct uint128_t_str uint128_t;
uint128_t add128(uint128_t a, uint128_t b);
uint128_t mul6464(uint64_t a, uint64_t b);
uint128_t mul12819(uint128_t a);
uint128_t rshift128(uint128_t x, uint8_t n);
uint128_t lshift128(uint128_t x, uint8_t n);
uint64_t mask51(uint128_t x);
uint64_t mask_lower(uint128_t x);
uint128_t mask51full(uint128_t x);
uint128_t init128x(uint64_t x);
#endif
