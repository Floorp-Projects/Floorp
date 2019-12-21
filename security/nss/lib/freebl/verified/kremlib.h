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

#ifndef __KREMLIB_H
#define __KREMLIB_H

#include "kremlib_base.h"

/* For tests only: we might need this function to be forward-declared, because
 * the dependency on WasmSupport appears very late, after SimplifyWasm, and
 * sadly, after the topological order has been done. */
void WasmSupport_check_buffer_size(uint32_t s);

/******************************************************************************/
/* Stubs to ease compilation of non-Low* code                                 */
/******************************************************************************/

/* Some types that KreMLin has no special knowledge of; many of them appear in
 * signatures of ghost functions, meaning that it suffices to give them (any)
 * definition. */
typedef void *FStar_Seq_Base_seq, *Prims_prop, *FStar_HyperStack_mem,
    *FStar_Set_set, *Prims_st_pre_h, *FStar_Heap_heap, *Prims_all_pre_h,
    *FStar_TSet_set, *Prims_list, *FStar_Map_t, *FStar_UInt63_t_,
    *FStar_Int63_t_, *FStar_UInt63_t, *FStar_Int63_t, *FStar_UInt_uint_t,
    *FStar_Int_int_t, *FStar_HyperStack_stackref, *FStar_Bytes_bytes,
    *FStar_HyperHeap_rid, *FStar_Heap_aref, *FStar_Monotonic_Heap_heap,
    *FStar_Monotonic_Heap_aref, *FStar_Monotonic_HyperHeap_rid,
    *FStar_Monotonic_HyperStack_mem, *FStar_Char_char_;

typedef const char *Prims_string;

/* For "bare" targets that do not have a C stdlib, the user might want to use
 * [-add-include '"mydefinitions.h"'] and override these. */
#ifndef KRML_HOST_PRINTF
#define KRML_HOST_PRINTF printf
#endif

#ifndef KRML_HOST_EXIT
#define KRML_HOST_EXIT exit
#endif

#ifndef KRML_HOST_MALLOC
#define KRML_HOST_MALLOC malloc
#endif

/* In statement position, exiting is easy. */
#define KRML_EXIT                                                                  \
    do {                                                                           \
        KRML_HOST_PRINTF("Unimplemented function at %s:%d\n", __FILE__, __LINE__); \
        KRML_HOST_EXIT(254);                                                       \
    } while (0)

/* In expression position, use the comma-operator and a malloc to return an
 * expression of the right size. KreMLin passes t as the parameter to the macro.
 */
#define KRML_EABORT(t, msg)                                                     \
    (KRML_HOST_PRINTF("KreMLin abort at %s:%d\n%s\n", __FILE__, __LINE__, msg), \
     KRML_HOST_EXIT(255), *((t *)KRML_HOST_MALLOC(sizeof(t))))

/* In FStar.Buffer.fst, the size of arrays is uint32_t, but it's a number of
 * *elements*. Do an ugly, run-time check (some of which KreMLin can eliminate).
 */
#define KRML_CHECK_SIZE(elt, size)                                            \
    if (((size_t)size) > SIZE_MAX / sizeof(elt)) {                            \
        KRML_HOST_PRINTF(                                                     \
            "Maximum allocatable size exceeded, aborting before overflow at " \
            "%s:%d\n",                                                        \
            __FILE__, __LINE__);                                              \
        KRML_HOST_EXIT(253);                                                  \
    }

/* A series of GCC atrocities to trace function calls (kremlin's [-d c-calls]
 * option). Useful when trying to debug, say, Wasm, to compare traces. */
/* clang-format off */
#ifdef __GNUC__
#define KRML_FORMAT(X) _Generic((X),                                           \
  uint8_t : "0x%08" PRIx8,                                                     \
  uint16_t: "0x%08" PRIx16,                                                    \
  uint32_t: "0x%08" PRIx32,                                                    \
  uint64_t: "0x%08" PRIx64,                                                    \
  int8_t  : "0x%08" PRIx8,                                                     \
  int16_t : "0x%08" PRIx16,                                                    \
  int32_t : "0x%08" PRIx32,                                                    \
  int64_t : "0x%08" PRIx64,                                                    \
  default : "%s")

#define KRML_FORMAT_ARG(X) _Generic((X),                                       \
  uint8_t : X,                                                                 \
  uint16_t: X,                                                                 \
  uint32_t: X,                                                                 \
  uint64_t: X,                                                                 \
  int8_t  : X,                                                                 \
  int16_t : X,                                                                 \
  int32_t : X,                                                                 \
  int64_t : X,                                                                 \
  default : "unknown")
/* clang-format on */

#define KRML_DEBUG_RETURN(X)                                        \
    ({                                                              \
        __auto_type _ret = (X);                                     \
        KRML_HOST_PRINTF("returning: ");                            \
        KRML_HOST_PRINTF(KRML_FORMAT(_ret), KRML_FORMAT_ARG(_ret)); \
        KRML_HOST_PRINTF(" \n");                                    \
        _ret;                                                       \
    })
#endif

#define FStar_Buffer_eqb(b1, b2, n) \
    (memcmp((b1), (b2), (n) * sizeof((b1)[0])) == 0)

/* Stubs to make ST happy. Important note: you must generate a use of the macro
 * argument, otherwise, you may have FStar_ST_recall(f) as the only use of f;
 * KreMLin will think that this is a valid use, but then the C compiler, after
 * macro expansion, will error out. */
#define FStar_HyperHeap_root 0
#define FStar_Pervasives_Native_fst(x) (x).fst
#define FStar_Pervasives_Native_snd(x) (x).snd
#define FStar_Seq_Base_createEmpty(x) 0
#define FStar_Seq_Base_create(len, init) 0
#define FStar_Seq_Base_upd(s, i, e) 0
#define FStar_Seq_Base_eq(l1, l2) 0
#define FStar_Seq_Base_length(l1) 0
#define FStar_Seq_Base_append(x, y) 0
#define FStar_Seq_Base_slice(x, y, z) 0
#define FStar_Seq_Properties_snoc(x, y) 0
#define FStar_Seq_Properties_cons(x, y) 0
#define FStar_Seq_Base_index(x, y) 0
#define FStar_HyperStack_is_eternal_color(x) 0
#define FStar_Monotonic_HyperHeap_root 0
#define FStar_Buffer_to_seq_full(x) 0
#define FStar_Buffer_recall(x)
#define FStar_HyperStack_ST_op_Colon_Equals(x, v) KRML_EXIT
#define FStar_HyperStack_ST_op_Bang(x) 0
#define FStar_HyperStack_ST_salloc(x) 0
#define FStar_HyperStack_ST_ralloc(x, y) 0
#define FStar_HyperStack_ST_new_region(x) (0)
#define FStar_Monotonic_RRef_m_alloc(x) \
    {                                   \
        0                               \
    }

#define FStar_HyperStack_ST_recall(x) \
    do {                              \
        (void)(x);                    \
    } while (0)

#define FStar_HyperStack_ST_recall_region(x) \
    do {                                     \
        (void)(x);                           \
    } while (0)

#define FStar_Monotonic_RRef_m_recall(x1, x2) \
    do {                                      \
        (void)(x1);                           \
        (void)(x2);                           \
    } while (0)

#define FStar_Monotonic_RRef_m_write(x1, x2, x3, x4, x5) \
    do {                                                 \
        (void)(x1);                                      \
        (void)(x2);                                      \
        (void)(x3);                                      \
        (void)(x4);                                      \
        (void)(x5);                                      \
    } while (0)

/******************************************************************************/
/* Endian-ness macros that can only be implemented in C                       */
/******************************************************************************/

/* ... for Linux */
#if defined(__linux__) || defined(__CYGWIN__)
#include <endian.h>

/* ... for OSX */
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

/* ... for Solaris */
#elif defined(__sun__)
#include <sys/byteorder.h>
#define htole64(x) LE_64(x)
#define le64toh(x) LE_64(x)
#define htobe64(x) BE_64(x)
#define be64toh(x) BE_64(x)

#define htole16(x) LE_16(x)
#define le16toh(x) LE_16(x)
#define htobe16(x) BE_16(x)
#define be16toh(x) BE_16(x)

#define htole32(x) LE_32(x)
#define le32toh(x) LE_32(x)
#define htobe32(x) BE_32(x)
#define be32toh(x) BE_32(x)

/* ... for the BSDs */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/endian.h>
#elif defined(__OpenBSD__)
#include <endian.h>

/* ... for Windows (MSVC)... not targeting XBOX 360! */
#elif defined(_MSC_VER)

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

/* ... for Windows (GCC-like, e.g. mingw or clang) */
#elif (defined(_WIN32) || defined(_WIN64)) && \
    (defined(__GNUC__) || defined(__clang__))

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

/* ... generic big-endian fallback code */
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

/* byte swapping code inspired by:
 * https://github.com/rweather/arduinolibs/blob/master/libraries/Crypto/utility/EndianUtil.h
 * */

#define htobe32(x) (x)
#define be32toh(x) (x)
#define htole32(x)                                                      \
    (__extension__({                                                    \
        uint32_t _temp = (x);                                           \
        ((_temp >> 24) & 0x000000FF) | ((_temp >> 8) & 0x0000FF00) |    \
            ((_temp << 8) & 0x00FF0000) | ((_temp << 24) & 0xFF000000); \
    }))
#define le32toh(x) (htole32((x)))

#define htobe64(x) (x)
#define be64toh(x) (x)
#define htole64(x)                                           \
    (__extension__({                                         \
        uint64_t __temp = (x);                               \
        uint32_t __low = htobe32((uint32_t)__temp);          \
        uint32_t __high = htobe32((uint32_t)(__temp >> 32)); \
        (((uint64_t)__low) << 32) | __high;                  \
    }))
#define le64toh(x) (htole64((x)))

/* ... generic little-endian fallback code */
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define htole32(x) (x)
#define le32toh(x) (x)
#define htobe32(x)                                                      \
    (__extension__({                                                    \
        uint32_t _temp = (x);                                           \
        ((_temp >> 24) & 0x000000FF) | ((_temp >> 8) & 0x0000FF00) |    \
            ((_temp << 8) & 0x00FF0000) | ((_temp << 24) & 0xFF000000); \
    }))
#define be32toh(x) (htobe32((x)))

#define htole64(x) (x)
#define le64toh(x) (x)
#define htobe64(x)                                           \
    (__extension__({                                         \
        uint64_t __temp = (x);                               \
        uint32_t __low = htobe32((uint32_t)__temp);          \
        uint32_t __high = htobe32((uint32_t)(__temp >> 32)); \
        (((uint64_t)__low) << 32) | __high;                  \
    }))
#define be64toh(x) (htobe64((x)))

/* ... couldn't determine endian-ness of the target platform */
#else
#error "Please define __BYTE_ORDER__!"

#endif /* defined(__linux__) || ... */

/* Loads and stores. These avoid undefined behavior due to unaligned memory
 * accesses, via memcpy. */

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

/******************************************************************************/
/* Checked integers to ease the compilation of non-Low* code                  */
/******************************************************************************/

typedef int32_t Prims_pos, Prims_nat, Prims_nonzero, Prims_int,
    krml_checked_int_t;

inline static bool
Prims_op_GreaterThanOrEqual(int32_t x, int32_t y)
{
    return x >= y;
}

inline static bool
Prims_op_LessThanOrEqual(int32_t x, int32_t y)
{
    return x <= y;
}

inline static bool
Prims_op_GreaterThan(int32_t x, int32_t y)
{
    return x > y;
}

inline static bool
Prims_op_LessThan(int32_t x, int32_t y)
{
    return x < y;
}

#define RETURN_OR(x)                                                            \
    do {                                                                        \
        int64_t __ret = x;                                                      \
        if (__ret < INT32_MIN || INT32_MAX < __ret) {                           \
            KRML_HOST_PRINTF("Prims.{int,nat,pos} integer overflow at %s:%d\n", \
                             __FILE__, __LINE__);                               \
            KRML_HOST_EXIT(252);                                                \
        }                                                                       \
        return (int32_t)__ret;                                                  \
    } while (0)

inline static int32_t
Prims_pow2(int32_t x)
{
    RETURN_OR((int64_t)1 << (int64_t)x);
}

inline static int32_t
Prims_op_Multiply(int32_t x, int32_t y)
{
    RETURN_OR((int64_t)x * (int64_t)y);
}

inline static int32_t
Prims_op_Addition(int32_t x, int32_t y)
{
    RETURN_OR((int64_t)x + (int64_t)y);
}

inline static int32_t
Prims_op_Subtraction(int32_t x, int32_t y)
{
    RETURN_OR((int64_t)x - (int64_t)y);
}

inline static int32_t
Prims_op_Division(int32_t x, int32_t y)
{
    RETURN_OR((int64_t)x / (int64_t)y);
}

inline static int32_t
Prims_op_Modulus(int32_t x, int32_t y)
{
    RETURN_OR((int64_t)x % (int64_t)y);
}

inline static int8_t
FStar_UInt8_uint_to_t(int8_t x)
{
    return x;
}
inline static int16_t
FStar_UInt16_uint_to_t(int16_t x)
{
    return x;
}
inline static int32_t
FStar_UInt32_uint_to_t(int32_t x)
{
    return x;
}
inline static int64_t
FStar_UInt64_uint_to_t(int64_t x)
{
    return x;
}

inline static int8_t
FStar_UInt8_v(int8_t x)
{
    return x;
}
inline static int16_t
FStar_UInt16_v(int16_t x)
{
    return x;
}
inline static int32_t
FStar_UInt32_v(int32_t x)
{
    return x;
}
inline static int64_t
FStar_UInt64_v(int64_t x)
{
    return x;
}

/* Platform-specific 128-bit arithmetic. These are static functions in a header,
 * so that each translation unit gets its own copy and the C compiler can
 * optimize. */
#ifndef KRML_NOUINT128
typedef unsigned __int128 FStar_UInt128_t, FStar_UInt128_t_, uint128_t;

static inline void
print128(const char *where, uint128_t n)
{
    KRML_HOST_PRINTF("%s: [%" PRIu64 ",%" PRIu64 "]\n", where,
                     (uint64_t)(n >> 64), (uint64_t)n);
}

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

#else /* !defined(KRML_NOUINT128) */

/* This is a bad circular dependency... should fix it properly. */
#include "FStar.h"

typedef FStar_UInt128_uint128 FStar_UInt128_t_, uint128_t;

/* A series of definitions written using pointers. */
static inline void
print128_(const char *where, uint128_t *n)
{
    KRML_HOST_PRINTF("%s: [0x%08" PRIx64 ",0x%08" PRIx64 "]\n", where, n->high, n->low);
}

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

#ifndef KRML_NOSTRUCT_PASSING

static inline void
print128(const char *where, uint128_t n)
{
    print128_(where, &n);
}

static inline uint128_t
load128_le(uint8_t *b)
{
    uint128_t r;
    load128_le_(b, &r);
    return r;
}

static inline void
store128_le(uint8_t *b, uint128_t n)
{
    store128_le_(b, &n);
}

static inline uint128_t
load128_be(uint8_t *b)
{
    uint128_t r;
    load128_be_(b, &r);
    return r;
}

static inline void
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
#endif /* KRML_UINT128 */
#endif /* __KREMLIB_H */
