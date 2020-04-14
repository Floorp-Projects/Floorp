/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 License. */

/******************************************************************************/
/* Machine integers (128-bit arithmetic)                                      */
/******************************************************************************/

/* This header contains two things.
 *
 * First, an implementation of 128-bit arithmetic suitable for 64-bit GCC and
 * Clang, i.e. all the operations from FStar.UInt128.
 *
 * Second, 128-bit operations from C.Endianness (or LowStar.Endianness),
 * suitable for any compiler and platform (via a series of ifdefs). This second
 * part is unfortunate, and should be fixed by moving {load,store}128_{be,le} to
 * FStar.UInt128 to avoid a maze of preprocessor guards and hand-written code.
 * */

/* This file is used for both the minimal and generic kremlib distributions. As
 * such, it assumes that the machine integers have been bundled the exact same
 * way in both cases. */

#include "FStar_UInt128.h"
#include "FStar_UInt_8_16_32_64.h"
#include "LowStar_Endianness.h"

#if !defined(KRML_VERIFIED_UINT128) && (!defined(_MSC_VER) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__x86_64) || defined(__aarch64__) ||             \
     (defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)))

/* GCC + using native unsigned __int128 support */

inline static uint128_t
load128_le(uint8_t *b)
{
    uint128_t l = (uint128_t)load64_le(b);
    uint128_t h = (uint128_t)load64_le(b + 8);
    return (h << 64 | l);
}

inline static void
store128_le(uint8_t *b, uint128_t n)
{
    store64_le(b, (uint64_t)n);
    store64_le(b + 8, (uint64_t)(n >> 64));
}

inline static uint128_t
load128_be(uint8_t *b)
{
    uint128_t h = (uint128_t)load64_be(b);
    uint128_t l = (uint128_t)load64_be(b + 8);
    return (h << 64 | l);
}

inline static void
store128_be(uint8_t *b, uint128_t n)
{
    store64_be(b, (uint64_t)(n >> 64));
    store64_be(b + 8, (uint64_t)n);
}

inline static uint128_t
FStar_UInt128_add(uint128_t x, uint128_t y)
{
    return x + y;
}

inline static uint128_t
FStar_UInt128_mul(uint128_t x, uint128_t y)
{
    return x * y;
}

inline static uint128_t
FStar_UInt128_add_mod(uint128_t x, uint128_t y)
{
    return x + y;
}

inline static uint128_t
FStar_UInt128_sub(uint128_t x, uint128_t y)
{
    return x - y;
}

inline static uint128_t
FStar_UInt128_sub_mod(uint128_t x, uint128_t y)
{
    return x - y;
}

inline static uint128_t
FStar_UInt128_logand(uint128_t x, uint128_t y)
{
    return x & y;
}

inline static uint128_t
FStar_UInt128_logor(uint128_t x, uint128_t y)
{
    return x | y;
}

inline static uint128_t
FStar_UInt128_logxor(uint128_t x, uint128_t y)
{
    return x ^ y;
}

inline static uint128_t
FStar_UInt128_lognot(uint128_t x)
{
    return ~x;
}

inline static uint128_t
FStar_UInt128_shift_left(uint128_t x, uint32_t y)
{
    return x << y;
}

inline static uint128_t
FStar_UInt128_shift_right(uint128_t x, uint32_t y)
{
    return x >> y;
}

inline static uint128_t
FStar_UInt128_uint64_to_uint128(uint64_t x)
{
    return (uint128_t)x;
}

inline static uint64_t
FStar_UInt128_uint128_to_uint64(uint128_t x)
{
    return (uint64_t)x;
}

inline static uint128_t
FStar_UInt128_mul_wide(uint64_t x, uint64_t y)
{
    return ((uint128_t)x) * y;
}

inline static uint128_t
FStar_UInt128_eq_mask(uint128_t x, uint128_t y)
{
    uint64_t mask =
        FStar_UInt64_eq_mask((uint64_t)(x >> 64), (uint64_t)(y >> 64)) &
        FStar_UInt64_eq_mask(x, y);
    return ((uint128_t)mask) << 64 | mask;
}

inline static uint128_t
FStar_UInt128_gte_mask(uint128_t x, uint128_t y)
{
    uint64_t mask =
        (FStar_UInt64_gte_mask(x >> 64, y >> 64) &
         ~(FStar_UInt64_eq_mask(x >> 64, y >> 64))) |
        (FStar_UInt64_eq_mask(x >> 64, y >> 64) & FStar_UInt64_gte_mask(x, y));
    return ((uint128_t)mask) << 64 | mask;
}

inline static uint64_t
FStar_UInt128___proj__Mkuint128__item__low(uint128_t x)
{
    return (uint64_t)x;
}

inline static uint64_t
FStar_UInt128___proj__Mkuint128__item__high(uint128_t x)
{
    return (uint64_t)(x >> 64);
}

inline static uint128_t
FStar_UInt128_add_underspec(uint128_t x, uint128_t y)
{
    return x + y;
}

inline static uint128_t
FStar_UInt128_sub_underspec(uint128_t x, uint128_t y)
{
    return x - y;
}

inline static bool
FStar_UInt128_eq(uint128_t x, uint128_t y)
{
    return x == y;
}

inline static bool
FStar_UInt128_gt(uint128_t x, uint128_t y)
{
    return x > y;
}

inline static bool
FStar_UInt128_lt(uint128_t x, uint128_t y)
{
    return x < y;
}

inline static bool
FStar_UInt128_gte(uint128_t x, uint128_t y)
{
    return x >= y;
}

inline static bool
FStar_UInt128_lte(uint128_t x, uint128_t y)
{
    return x <= y;
}

inline static uint128_t
FStar_UInt128_mul32(uint64_t x, uint32_t y)
{
    return (uint128_t)x * (uint128_t)y;
}

#elif !defined(_MSC_VER) && defined(KRML_VERIFIED_UINT128)

/* Verified uint128 implementation. */

/* Access 64-bit fields within the int128. */
#define HIGH64_OF(x) ((x)->high)
#define LOW64_OF(x) ((x)->low)

/* A series of definitions written using pointers. */

inline static void
load128_le_(uint8_t *b, uint128_t *r)
{
    LOW64_OF(r) = load64_le(b);
    HIGH64_OF(r) = load64_le(b + 8);
}

inline static void
store128_le_(uint8_t *b, uint128_t *n)
{
    store64_le(b, LOW64_OF(n));
    store64_le(b + 8, HIGH64_OF(n));
}

inline static void
load128_be_(uint8_t *b, uint128_t *r)
{
    HIGH64_OF(r) = load64_be(b);
    LOW64_OF(r) = load64_be(b + 8);
}

inline static void
store128_be_(uint8_t *b, uint128_t *n)
{
    store64_be(b, HIGH64_OF(n));
    store64_be(b + 8, LOW64_OF(n));
}

#ifndef KRML_NOSTRUCT_PASSING

inline static uint128_t
load128_le(uint8_t *b)
{
    uint128_t r;
    load128_le_(b, &r);
    return r;
}

inline static void
store128_le(uint8_t *b, uint128_t n)
{
    store128_le_(b, &n);
}

inline static uint128_t
load128_be(uint8_t *b)
{
    uint128_t r;
    load128_be_(b, &r);
    return r;
}

inline static void
store128_be(uint8_t *b, uint128_t n)
{
    store128_be_(b, &n);
}

#else /* !defined(KRML_STRUCT_PASSING) */

#define print128 print128_
#define load128_le load128_le_
#define store128_le store128_le_
#define load128_be load128_be_
#define store128_be store128_be_

#endif /* KRML_STRUCT_PASSING */

#endif
