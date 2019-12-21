/* Copyright 2016-2018 INRIA and Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __KREMLIB_BASE_H
#define __KREMLIB_BASE_H

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/******************************************************************************/
/* Some macros to ease compatibility                                          */
/******************************************************************************/

/* Define __cdecl and friends when using GCC, so that we can safely compile code
 * that contains __cdecl on all platforms. Note that this is in a separate
 * header so that Dafny-generated code can include just this file. */
#ifndef _MSC_VER
/* Use the gcc predefined macros if on a platform/architectures that set them.
 * Otherwise define them to be empty. */
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#endif

#ifdef __GNUC__
#define inline __inline__
#endif

/* GCC-specific attribute syntax; everyone else gets the standard C inline
 * attribute. */
#ifdef __GNU_C__
#ifndef __clang__
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif
#else
#define force_inline inline
#endif

/******************************************************************************/
/* Implementing C.fst                                                         */
/******************************************************************************/

/* Uppercase issue; we have to define lowercase versions of the C macros (as we
 * have no way to refer to an uppercase *variable* in F*). */
extern int exit_success;
extern int exit_failure;

/* This one allows the user to write C.EXIT_SUCCESS. */
typedef int exit_code;

void print_string(const char *s);
void print_bytes(uint8_t *b, uint32_t len);

/* The universal null pointer defined in C.Nullity.fst */
#define C_Nullity_null(X) 0

/* If some globals need to be initialized before the main, then kremlin will
 * generate and try to link last a function with this type: */
void kremlinit_globals(void);

/******************************************************************************/
/* Implementation of machine integers (possibly of 128-bit integers)          */
/******************************************************************************/

/* Integer types */
typedef uint64_t FStar_UInt64_t, FStar_UInt64_t_;
typedef int64_t FStar_Int64_t, FStar_Int64_t_;
typedef uint32_t FStar_UInt32_t, FStar_UInt32_t_;
typedef int32_t FStar_Int32_t, FStar_Int32_t_;
typedef uint16_t FStar_UInt16_t, FStar_UInt16_t_;
typedef int16_t FStar_Int16_t, FStar_Int16_t_;
typedef uint8_t FStar_UInt8_t, FStar_UInt8_t_;
typedef int8_t FStar_Int8_t, FStar_Int8_t_;

static inline uint32_t
rotate32_left(uint32_t x, uint32_t n)
{
    /*  assert (n<32); */
    return (x << n) | (x >> (32 - n));
}
static inline uint32_t
rotate32_right(uint32_t x, uint32_t n)
{
    /*  assert (n<32); */
    return (x >> n) | (x << (32 - n));
}

/* Constant time comparisons */
static inline uint8_t
FStar_UInt8_eq_mask(uint8_t x, uint8_t y)
{
    x = ~(x ^ y);
    x &= x << 4;
    x &= x << 2;
    x &= x << 1;
    return (int8_t)x >> 7;
}

static inline uint8_t
FStar_UInt8_gte_mask(uint8_t x, uint8_t y)
{
    return ~(uint8_t)(((int32_t)x - y) >> 31);
}

static inline uint16_t
FStar_UInt16_eq_mask(uint16_t x, uint16_t y)
{
    x = ~(x ^ y);
    x &= x << 8;
    x &= x << 4;
    x &= x << 2;
    x &= x << 1;
    return (int16_t)x >> 15;
}

static inline uint16_t
FStar_UInt16_gte_mask(uint16_t x, uint16_t y)
{
    return ~(uint16_t)(((int32_t)x - y) >> 31);
}

static inline uint32_t
FStar_UInt32_eq_mask(uint32_t x, uint32_t y)
{
    x = ~(x ^ y);
    x &= x << 16;
    x &= x << 8;
    x &= x << 4;
    x &= x << 2;
    x &= x << 1;
    return ((int32_t)x) >> 31;
}

static inline uint32_t
FStar_UInt32_gte_mask(uint32_t x, uint32_t y)
{
    return ~((uint32_t)(((int64_t)x - y) >> 63));
}

static inline uint64_t
FStar_UInt64_eq_mask(uint64_t x, uint64_t y)
{
    x = ~(x ^ y);
    x &= x << 32;
    x &= x << 16;
    x &= x << 8;
    x &= x << 4;
    x &= x << 2;
    x &= x << 1;
    return ((int64_t)x) >> 63;
}

static inline uint64_t
FStar_UInt64_gte_mask(uint64_t x, uint64_t y)
{
    uint64_t low63 =
        ~((uint64_t)((int64_t)((int64_t)(x & UINT64_C(0x7fffffffffffffff)) -
                               (int64_t)(y & UINT64_C(0x7fffffffffffffff))) >>
                     63));
    uint64_t high_bit =
        ~((uint64_t)((int64_t)((int64_t)(x & UINT64_C(0x8000000000000000)) -
                               (int64_t)(y & UINT64_C(0x8000000000000000))) >>
                     63));
    return low63 & high_bit;
}

#endif
