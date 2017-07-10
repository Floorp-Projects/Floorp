/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#ifndef AV1_COMMON_ODINTRIN_H_
#define AV1_COMMON_ODINTRIN_H_

#if defined(_MSC_VER)
# define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/bitops.h"
#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

# if !defined(M_PI)
#  define M_PI      (3.1415926535897932384626433832795)
# endif

# if !defined(M_SQRT2)
#  define M_SQRT2 (1.41421356237309504880168872420970)
# endif

# if !defined(M_SQRT1_2)
#  define M_SQRT1_2 (0.70710678118654752440084436210485)
# endif

# if !defined(M_LOG2E)
#  define M_LOG2E (1.4426950408889634073599246810019)
# endif

# if !defined(M_LN2)
#  define M_LN2 (0.69314718055994530941723212145818)
# endif

/*Smallest blocks are 4x4*/
#define OD_LOG_BSIZE0 (2)
/*There are 5 block sizes total (4x4, 8x8, 16x16, 32x32 and 64x64).*/
#define OD_NBSIZES (5)

/*There are 4 transform sizes total in AV1 (4x4, 8x8, 16x16 and 32x32).*/
#define OD_TXSIZES TX_SIZES
/*The log of the maximum length of the side of a transform.*/
#define OD_LOG_TXSIZE_MAX (OD_LOG_BSIZE0 + OD_TXSIZES - 1)
/*The maximum length of the side of a transform.*/
#define OD_TXSIZE_MAX (1 << OD_LOG_TXSIZE_MAX)

/**The maximum number of color planes allowed in a single frame.*/
# define OD_NPLANES_MAX (3)

# define OD_COEFF_SHIFT (4)

# define OD_DISABLE_CFL (1)
# define OD_DISABLE_FILTER (1)

#if !defined(NDEBUG)
# define OD_ENABLE_ASSERTIONS (1)
#endif

# define OD_LOG(a)
# define OD_LOG_PARTIAL(a)

/*Possible block sizes, note that OD_BLOCK_NXN = log2(N) - 2.*/
#define OD_BLOCK_4X4 (0)
#define OD_BLOCK_8X8 (1)
#define OD_BLOCK_16X16 (2)
#define OD_BLOCK_32X32 (3)
#define OD_BLOCK_SIZES (OD_BLOCK_32X32 + 1)

# define OD_LIMIT_BSIZE_MIN (OD_BLOCK_4X4)
# define OD_LIMIT_BSIZE_MAX (OD_BLOCK_32X32)

typedef int od_coeff;

/*This is the strength reduced version of ((_a)/(1 << (_b))).
  This will not work for _b == 0, however currently this is only used for
   b == 1 anyway.*/
# define OD_UNBIASED_RSHIFT32(_a, _b) \
  (((int32_t)(((uint32_t)(_a) >> (32 - (_b))) + (_a))) >> (_b))

#define OD_DIVU_DMAX (1024)

extern uint32_t OD_DIVU_SMALL_CONSTS[OD_DIVU_DMAX][2];

/*Translate unsigned division by small divisors into multiplications.*/
#define OD_DIVU_SMALL(_x, _d)                                     \
  ((uint32_t)((OD_DIVU_SMALL_CONSTS[(_d)-1][0] * (uint64_t)(_x) + \
               OD_DIVU_SMALL_CONSTS[(_d)-1][1]) >>                \
              32) >>                                              \
   (OD_ILOG_NZ(_d) - 1))

#define OD_DIVU(_x, _d) \
  (((_d) < OD_DIVU_DMAX) ? (OD_DIVU_SMALL((_x), (_d))) : ((_x) / (_d)))

#define OD_MINI AOMMIN
#define OD_MAXI AOMMAX
#define OD_CLAMPI(min, val, max) (OD_MAXI(min, OD_MINI(val, max)))

#define OD_CLZ0 (1)
#define OD_CLZ(x) (-get_msb(x))
#define OD_ILOG_NZ(x) (OD_CLZ0 - OD_CLZ(x))
/*Note that __builtin_clz is not defined when x == 0, according to the gcc
   documentation (and that of the x86 BSR instruction that implements it), so
   we have to special-case it.
  We define a special version of the macro to use when x can be zero.*/
#define OD_ILOG(x) ((x) ? OD_ILOG_NZ(x) : 0)

#define OD_LOG2(x) (M_LOG2E*log(x))
#define OD_EXP2(x) (exp(M_LN2*(x)))

/*Enable special features for gcc and compatible compilers.*/
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define OD_GNUC_PREREQ(maj, min, pat)                                \
  ((__GNUC__ << 16) + (__GNUC_MINOR__ << 8) + __GNUC_PATCHLEVEL__ >= \
   ((maj) << 16) + ((min) << 8) + pat)  // NOLINT
#else
#define OD_GNUC_PREREQ(maj, min, pat) (0)
#endif

#if OD_GNUC_PREREQ(3, 4, 0)
#define OD_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#else
#define OD_WARN_UNUSED_RESULT
#endif

#if OD_GNUC_PREREQ(3, 4, 0)
#define OD_ARG_NONNULL(x) __attribute__((__nonnull__(x)))
#else
#define OD_ARG_NONNULL(x)
#endif

#if defined(OD_ENABLE_ASSERTIONS)
#if OD_GNUC_PREREQ(2, 5, 0)
__attribute__((noreturn))
#endif
void od_fatal_impl(const char *_str, const char *_file, int _line);

#define OD_FATAL(_str) (od_fatal_impl(_str, __FILE__, __LINE__))

#define OD_ASSERT(_cond)                     \
  do {                                       \
    if (!(_cond)) {                          \
      OD_FATAL("assertion failed: " #_cond); \
    }                                        \
  } while (0)

#define OD_ASSERT2(_cond, _message)                        \
  do {                                                     \
    if (!(_cond)) {                                        \
      OD_FATAL("assertion failed: " #_cond "\n" _message); \
    }                                                      \
  } while (0)

#define OD_ALWAYS_TRUE(_cond) OD_ASSERT(_cond)

#else
#define OD_ASSERT(_cond)
#define OD_ASSERT2(_cond, _message)
#define OD_ALWAYS_TRUE(_cond) ((void)(_cond))
#endif

/** Copy n elements of memory from src to dst. The 0* term provides
    compile-time type checking  */
#if !defined(OVERRIDE_OD_COPY)
#define OD_COPY(dst, src, n) \
  (memcpy((dst), (src), sizeof(*(dst)) * (n) + 0 * ((dst) - (src))))
#endif

/** Copy n elements of memory from src to dst, allowing overlapping regions.
    The 0* term provides compile-time type checking */
#if !defined(OVERRIDE_OD_MOVE)
# define OD_MOVE(dst, src, n) \
 (memmove((dst), (src), sizeof(*(dst))*(n) + 0*((dst) - (src)) ))
#endif

/** Linkage will break without this if using a C++ compiler, and will issue
 * warnings without this for a C compiler*/
#if defined(__cplusplus)
# define OD_EXTERN extern
#else
# define OD_EXTERN
#endif

/** Set n elements of dst to zero */
#if !defined(OVERRIDE_OD_CLEAR)
# define OD_CLEAR(dst, n) (memset((dst), 0, sizeof(*(dst))*(n)))
#endif

/** Silence unused parameter/variable warnings */
# define OD_UNUSED(expr) (void)(expr)

#if defined(OD_FLOAT_PVQ)
typedef double od_val16;
typedef double od_val32;
# define OD_QCONST32(x, bits) (x)
# define OD_ROUND16(x) (x)
# define OD_ROUND32(x) (x)
# define OD_SHL(x, shift) (x)
# define OD_SHR(x, shift) (x)
# define OD_SHR_ROUND(x, shift) (x)
# define OD_ABS(x) (fabs(x))
# define OD_MULT16_16(a, b) ((a)*(b))
# define OD_MULT16_32_Q16(a, b) ((a)*(b))
#else
typedef int16_t od_val16;
typedef int32_t od_val32;
/** Compile-time conversion of float constant to 32-bit value */
# define OD_QCONST32(x, bits) ((od_val32)(.5 + (x)*(((od_val32)1) << (bits))))
# define OD_ROUND16(x) (int16_t)(floor(.5 + (x)))
# define OD_ROUND32(x) (int32_t)(floor(.5 + (x)))
/*Shift x left by shift*/
# define OD_SHL(a, shift) ((int32_t)((uint32_t)(a) << (shift)))
/*Shift x right by shift (without rounding)*/
# define OD_SHR(x, shift) \
  ((int32_t)((x) >> (shift)))
/*Shift x right by shift (with rounding)*/
# define OD_SHR_ROUND(x, shift) \
  ((int32_t)(((x) + (1 << (shift) >> 1)) >> (shift)))
/*Shift x right by shift (without rounding) or left by -shift if shift
  is negative.*/
# define OD_VSHR(x, shift) \
  (((shift) > 0) ? OD_SHR(x, shift) : OD_SHL(x, -(shift)))
/*Shift x right by shift (with rounding) or left by -shift if shift
  is negative.*/
# define OD_VSHR_ROUND(x, shift) \
  (((shift) > 0) ? OD_SHR_ROUND(x, shift) : OD_SHL(x, -(shift)))
# define OD_ABS(x) (abs(x))
/* (od_val32)(od_val16) gives TI compiler a hint that it's 16x16->32 multiply */
/** 16x16 multiplication where the result fits in 32 bits */
# define OD_MULT16_16(a, b) \
 (((od_val32)(od_val16)(a))*((od_val32)(od_val16)(b)))
/* Multiplies 16-bit a by 32-bit b and keeps bits [16:47]. */
# define OD_MULT16_32_Q16(a, b) ((int16_t)(a)*(int64_t)(int32_t)(b) >> 16)
/*16x16 multiplication where the result fits in 16 bits, without rounding.*/
# define OD_MULT16_16_Q15(a, b) \
  (((int16_t)(a)*((int32_t)(int16_t)(b))) >> 15)
/*16x16 multiplication where the result fits in 16 bits, without rounding.*/
# define OD_MULT16_16_Q16(a, b) \
  ((((int16_t)(a))*((int32_t)(int16_t)(b))) >> 16)
#endif

/*All of these macros should expect floats as arguments.*/
/*These two should compile as a single SSE instruction.*/
# define OD_MINF(a, b) ((a) < (b) ? (a) : (b))
# define OD_MAXF(a, b) ((a) > (b) ? (a) : (b))

# define OD_DIV_R0(x, y) (((x) + OD_FLIPSIGNI((((y) + 1) >> 1) - 1, (x)))/(y))

# define OD_SIGNMASK(a) (-((a) < 0))
# define OD_FLIPSIGNI(a, b) (((a) + OD_SIGNMASK(b)) ^ OD_SIGNMASK(b))

# define OD_MULT16_16_Q15(a, b) \
  (((int16_t)(a)*((int32_t)(int16_t)(b))) >> 15)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_ODINTRIN_H_
