// Copyright 2016-2017 Microsoft Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __KREMLIB_H
#define __KREMLIB_H

#include <inttypes.h>
#include <stdlib.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Define __cdecl and friends when using GCC, so that we can safely compile code
// that contains __cdecl on all platforms.
#ifndef _MSC_VER
// Use the gcc predefined macros if on a platform/architectures that set them.
// Otherwise define them to be empty.
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

// GCC-specific attribute syntax; everyone else gets the standard C inline
// attribute.
#ifdef __GNU_C__
#ifndef __clang__
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif
#else
#define force_inline inline
#endif

// Uppercase issue; we have to define lowercase version of the C macros (as we
// have no way to refer to an uppercase *variable* in F*).
extern int exit_success;
extern int exit_failure;

// Some types that KreMLin has no special knowledge of; many of them appear in
// signatures of ghost functions, meaning that it suffices to give them (any)
// definition.
typedef void *Prims_pos, *Prims_nat, *Prims_nonzero, *FStar_Seq_Base_seq,
    *Prims_int, *Prims_prop, *FStar_HyperStack_mem, *FStar_Set_set,
    *Prims_st_pre_h, *FStar_Heap_heap, *Prims_all_pre_h, *FStar_TSet_set,
    *Prims_string, *Prims_list, *FStar_Map_t, *FStar_UInt63_t_, *FStar_Int63_t_,
    *FStar_UInt63_t, *FStar_Int63_t, *FStar_UInt_uint_t, *FStar_Int_int_t,
    *FStar_HyperStack_stackref, *FStar_Bytes_bytes, *FStar_HyperHeap_rid,
    *FStar_Heap_aref, *FStar_Monotonic_Heap_heap,
    *FStar_Monotonic_HyperHeap_rid, *FStar_Monotonic_HyperStack_mem;

#define KRML_CHECK_SIZE(elt, size)                                               \
    if (((size_t)size) > SIZE_MAX / sizeof(elt)) {                               \
        printf("Maximum allocatable size exceeded, aborting before overflow at " \
               "%s:%d\n",                                                        \
               __FILE__, __LINE__);                                              \
        exit(253);                                                               \
    }

// Endian-ness

// ... for Linux
#if defined(__linux__) || defined(__CYGWIN__)
#include <endian.h>

// ... for OSX
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htole64(x) OSSwapHostToLittleInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)

#define htole16(x) OSSwapHostToLittleInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define htobe16(x) OSSwapHostToBigInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)

#define htole32(x) OSSwapHostToLittleInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)

// ... for Windows
#elif (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && \
    !defined(__WINDOWS__)
#include <windows.h>

#if BYTE_ORDER == LITTLE_ENDIAN

#if defined(_MSC_VER)
#include <stdlib.h>
#define htobe16(x) _byteswap_ushort(x)
#define htole16(x) (x)
#define be16toh(x) _byteswap_ushort(x)
#define le16toh(x) (x)

#define htobe32(x) _byteswap_ulong(x)
#define htole32(x) (x)
#define be32toh(x) _byteswap_ulong(x)
#define le32toh(x) (x)

#define htobe64(x) _byteswap_uint64(x)
#define htole64(x) (x)
#define be64toh(x) _byteswap_uint64(x)
#define le64toh(x) (x)

#elif defined(__GNUC__) || defined(__clang__)

#define htobe16(x) __builtin_bswap16(x)
#define htole16(x) (x)
#define be16toh(x) __builtin_bswap16(x)
#define le16toh(x) (x)

#define htobe32(x) __builtin_bswap32(x)
#define htole32(x) (x)
#define be32toh(x) __builtin_bswap32(x)
#define le32toh(x) (x)

#define htobe64(x) __builtin_bswap64(x)
#define htole64(x) (x)
#define be64toh(x) __builtin_bswap64(x)
#define le64toh(x) (x)
#endif

#endif

#endif

// Loads and stores. These avoid undefined behavior due to unaligned memory
// accesses, via memcpy.

inline static uint16_t
load16(uint8_t *b)
{
    uint16_t x;
    memcpy(&x, b, 2);
    return x;
}

inline static uint32_t
load32(uint8_t *b)
{
    uint32_t x;
    memcpy(&x, b, 4);
    return x;
}

inline static uint64_t
load64(uint8_t *b)
{
    uint64_t x;
    memcpy(&x, b, 8);
    return x;
}

inline static void
store16(uint8_t *b, uint16_t i)
{
    memcpy(b, &i, 2);
}

inline static void
store32(uint8_t *b, uint32_t i)
{
    memcpy(b, &i, 4);
}

inline static void
store64(uint8_t *b, uint64_t i)
{
    memcpy(b, &i, 8);
}

#define load16_le(b) (le16toh(load16(b)))
#define store16_le(b, i) (store16(b, htole16(i)))
#define load16_be(b) (be16toh(load16(b)))
#define store16_be(b, i) (store16(b, htobe16(i)))

#define load32_le(b) (le32toh(load32(b)))
#define store32_le(b, i) (store32(b, htole32(i)))
#define load32_be(b) (be32toh(load32(b)))
#define store32_be(b, i) (store32(b, htobe32(i)))

#define load64_le(b) (le64toh(load64(b)))
#define store64_le(b, i) (store64(b, htole64(i)))
#define load64_be(b) (be64toh(load64(b)))
#define store64_be(b, i) (store64(b, htobe64(i)))

// Integer types
typedef uint64_t FStar_UInt64_t, FStar_UInt64_t_;
typedef int64_t FStar_Int64_t, FStar_Int64_t_;
typedef uint32_t FStar_UInt32_t, FStar_UInt32_t_;
typedef int32_t FStar_Int32_t, FStar_Int32_t_;
typedef uint16_t FStar_UInt16_t, FStar_UInt16_t_;
typedef int16_t FStar_Int16_t, FStar_Int16_t_;
typedef uint8_t FStar_UInt8_t, FStar_UInt8_t_;
typedef int8_t FStar_Int8_t, FStar_Int8_t_;

// Constant time comparisons
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

// Platform-specific 128-bit arithmetic. These are static functions in a header,
// so that each translation unit gets its own copy and the C compiler can
// optimize.
#ifdef HAVE_INT128_SUPPORT
typedef unsigned __int128 FStar_UInt128_t, FStar_UInt128_t_, uint128_t;

static inline uint128_t
load128_le(uint8_t *b)
{
    uint128_t l = (uint128_t)load64_le(b);
    uint128_t h = (uint128_t)load64_le(b + 8);
    return (h << 64 | l);
}

static inline void
store128_le(uint8_t *b, uint128_t n)
{
    store64_le(b, (uint64_t)n);
    store64_le(b + 8, (uint64_t)(n >> 64));
}

static inline uint128_t
load128_be(uint8_t *b)
{
    uint128_t h = (uint128_t)load64_be(b);
    uint128_t l = (uint128_t)load64_be(b + 8);
    return (h << 64 | l);
}

static inline void
store128_be(uint8_t *b, uint128_t n)
{
    store64_be(b, (uint64_t)(n >> 64));
    store64_be(b + 8, (uint64_t)n);
}

#define FStar_UInt128_add(x, y) ((x) + (y))
#define FStar_UInt128_mul(x, y) ((x) * (y))
#define FStar_UInt128_add_mod(x, y) ((x) + (y))
#define FStar_UInt128_sub(x, y) ((x) - (y))
#define FStar_UInt128_sub_mod(x, y) ((x) - (y))
#define FStar_UInt128_logand(x, y) ((x) & (y))
#define FStar_UInt128_logor(x, y) ((x) | (y))
#define FStar_UInt128_logxor(x, y) ((x) ^ (y))
#define FStar_UInt128_lognot(x) (~(x))
#define FStar_UInt128_shift_left(x, y) ((x) << (y))
#define FStar_UInt128_shift_right(x, y) ((x) >> (y))
#define FStar_UInt128_uint64_to_uint128(x) ((uint128_t)(x))
#define FStar_UInt128_uint128_to_uint64(x) ((uint64_t)(x))
#define FStar_Int_Cast_Full_uint64_to_uint128(x) ((uint128_t)(x))
#define FStar_Int_Cast_Full_uint128_to_uint64(x) ((uint64_t)(x))
#define FStar_UInt128_mul_wide(x, y) ((uint128_t)(x) * (y))
#define FStar_UInt128_op_Hat_Hat(x, y) ((x) ^ (y))

static inline uint128_t
FStar_UInt128_eq_mask(uint128_t x, uint128_t y)
{
    uint64_t mask =
        FStar_UInt64_eq_mask((uint64_t)(x >> 64), (uint64_t)(y >> 64)) &
        FStar_UInt64_eq_mask(x, y);
    return ((uint128_t)mask) << 64 | mask;
}

static inline uint128_t
FStar_UInt128_gte_mask(uint128_t x, uint128_t y)
{
    uint64_t mask =
        (FStar_UInt64_gte_mask(x >> 64, y >> 64) &
         ~(FStar_UInt64_eq_mask(x >> 64, y >> 64))) |
        (FStar_UInt64_eq_mask(x >> 64, y >> 64) & FStar_UInt64_gte_mask(x, y));
    return ((uint128_t)mask) << 64 | mask;
}

#else // defined(HAVE_INT128_SUPPORT)

#include "fstar_uint128.h"

typedef FStar_UInt128_uint128 FStar_UInt128_t_, uint128_t;

static inline void
load128_le_(uint8_t *b, uint128_t *r)
{
    r->low = load64_le(b);
    r->high = load64_le(b + 8);
}

static inline void
store128_le_(uint8_t *b, uint128_t *n)
{
    store64_le(b, n->low);
    store64_le(b + 8, n->high);
}

static inline void
load128_be_(uint8_t *b, uint128_t *r)
{
    r->high = load64_be(b);
    r->low = load64_be(b + 8);
}

static inline void
store128_be_(uint8_t *b, uint128_t *n)
{
    store64_be(b, n->high);
    store64_be(b + 8, n->low);
}

/* #define print128 print128_ */
#define load128_le load128_le_
#define store128_le store128_le_
#define load128_be load128_be_
#define store128_be store128_be_

#endif // HAVE_INT128_SUPPORT
#endif // __KREMLIB_H
