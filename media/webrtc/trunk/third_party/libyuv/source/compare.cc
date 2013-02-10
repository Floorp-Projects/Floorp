/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/compare.h"

#include <float.h>
#include <math.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// hash seed of 5381 recommended.
// Internal C version of HashDjb2 with int sized count for efficiency.
static uint32 HashDjb2_C(const uint8* src, int count, uint32 seed) {
  uint32 hash = seed;
  for (int i = 0; i < count; ++i) {
    hash += (hash << 5) + src[i];
  }
  return hash;
}

// This module is for Visual C x86
#if !defined(YUV_DISABLE_ASM) && defined(_M_IX86)
#define HAS_HASHDJB2_SSE41
static const uvec32 kHash16x33 = { 0x92d9e201, 0, 0, 0 };  // 33 ^ 16
static const uvec32 kHashMul0 = {
  0x0c3525e1,  // 33 ^ 15
  0xa3476dc1,  // 33 ^ 14
  0x3b4039a1,  // 33 ^ 13
  0x4f5f0981,  // 33 ^ 12
};
static const uvec32 kHashMul1 = {
  0x30f35d61,  // 33 ^ 11
  0x855cb541,  // 33 ^ 10
  0x040a9121,  // 33 ^ 9
  0x747c7101,  // 33 ^ 8
};
static const uvec32 kHashMul2 = {
  0xec41d4e1,  // 33 ^ 7
  0x4cfa3cc1,  // 33 ^ 6
  0x025528a1,  // 33 ^ 5
  0x00121881,  // 33 ^ 4
};
static const uvec32 kHashMul3 = {
  0x00008c61,  // 33 ^ 3
  0x00000441,  // 33 ^ 2
  0x00000021,  // 33 ^ 1
  0x00000001,  // 33 ^ 0
};

// 27: 66 0F 38 40 C6     pmulld      xmm0,xmm6
// 44: 66 0F 38 40 DD     pmulld      xmm3,xmm5
// 59: 66 0F 38 40 E5     pmulld      xmm4,xmm5
// 72: 66 0F 38 40 D5     pmulld      xmm2,xmm5
// 83: 66 0F 38 40 CD     pmulld      xmm1,xmm5
#define pmulld(reg) _asm _emit 0x66 _asm _emit 0x0F _asm _emit 0x38 \
    _asm _emit 0x40 _asm _emit reg

__declspec(naked) __declspec(align(16))
static uint32 HashDjb2_SSE41(const uint8* src, int count, uint32 seed) {
  __asm {
    mov        eax, [esp + 4]    // src
    mov        ecx, [esp + 8]    // count
    movd       xmm0, [esp + 12]  // seed

    pxor       xmm7, xmm7        // constant 0 for unpck
    movdqa     xmm6, kHash16x33

    align      16
  wloop:
    movdqu     xmm1, [eax]       // src[0-15]
    lea        eax, [eax + 16]
    pmulld(0xc6)                 // pmulld      xmm0,xmm6  hash *= 33 ^ 16
    movdqa     xmm5, kHashMul0
    movdqa     xmm2, xmm1
    punpcklbw  xmm2, xmm7        // src[0-7]
    movdqa     xmm3, xmm2
    punpcklwd  xmm3, xmm7        // src[0-3]
    pmulld(0xdd)                 // pmulld     xmm3, xmm5
    movdqa     xmm5, kHashMul1
    movdqa     xmm4, xmm2
    punpckhwd  xmm4, xmm7        // src[4-7]
    pmulld(0xe5)                 // pmulld     xmm4, xmm5
    movdqa     xmm5, kHashMul2
    punpckhbw  xmm1, xmm7        // src[8-15]
    movdqa     xmm2, xmm1
    punpcklwd  xmm2, xmm7        // src[8-11]
    pmulld(0xd5)                 // pmulld     xmm2, xmm5
    movdqa     xmm5, kHashMul3
    punpckhwd  xmm1, xmm7        // src[12-15]
    pmulld(0xcd)                 // pmulld     xmm1, xmm5
    paddd      xmm3, xmm4        // add 16 results
    paddd      xmm1, xmm2
    sub        ecx, 16
    paddd      xmm1, xmm3

    pshufd     xmm2, xmm1, 14    // upper 2 dwords
    paddd      xmm1, xmm2
    pshufd     xmm2, xmm1, 1
    paddd      xmm1, xmm2
    paddd      xmm0, xmm1
    jg         wloop

    movd       eax, xmm0        // return hash
    ret
  }
}

#elif !defined(YUV_DISABLE_ASM) && \
    (defined(__x86_64__) || (defined(__i386__) && !defined(__pic__)))
// GCC 4.2 on OSX has link error when passing static or const to inline.
// TODO(fbarchard): Use static const when gcc 4.2 support is dropped.
#ifdef __APPLE__
#define CONST
#else
#define CONST static const
#endif
#define HAS_HASHDJB2_SSE41
CONST uvec32 kHash16x33 = { 0x92d9e201, 0, 0, 0 };  // 33 ^ 16
CONST uvec32 kHashMul0 = {
  0x0c3525e1,  // 33 ^ 15
  0xa3476dc1,  // 33 ^ 14
  0x3b4039a1,  // 33 ^ 13
  0x4f5f0981,  // 33 ^ 12
};
CONST uvec32 kHashMul1 = {
  0x30f35d61,  // 33 ^ 11
  0x855cb541,  // 33 ^ 10
  0x040a9121,  // 33 ^ 9
  0x747c7101,  // 33 ^ 8
};
CONST uvec32 kHashMul2 = {
  0xec41d4e1,  // 33 ^ 7
  0x4cfa3cc1,  // 33 ^ 6
  0x025528a1,  // 33 ^ 5
  0x00121881,  // 33 ^ 4
};
CONST uvec32 kHashMul3 = {
  0x00008c61,  // 33 ^ 3
  0x00000441,  // 33 ^ 2
  0x00000021,  // 33 ^ 1
  0x00000001,  // 33 ^ 0
};
static uint32 HashDjb2_SSE41(const uint8* src, int count, uint32 seed) {
  uint32 hash;
  asm volatile (
    "movd      %2,%%xmm0                       \n"
    "pxor      %%xmm7,%%xmm7                   \n"
    "movdqa    %4,%%xmm6                       \n"
    ".p2align  4                               \n"
  "1:                                          \n"
    "movdqu    (%0),%%xmm1                     \n"
    "lea       0x10(%0),%0                     \n"
    "pmulld    %%xmm6,%%xmm0                   \n"
    "movdqa    %5,%%xmm5                       \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm7,%%xmm2                   \n"
    "movdqa    %%xmm2,%%xmm3                   \n"
    "punpcklwd %%xmm7,%%xmm3                   \n"
    "pmulld    %%xmm5,%%xmm3                   \n"
    "movdqa    %6,%%xmm5                       \n"
    "movdqa    %%xmm2,%%xmm4                   \n"
    "punpckhwd %%xmm7,%%xmm4                   \n"
    "pmulld    %%xmm5,%%xmm4                   \n"
    "movdqa    %7,%%xmm5                       \n"
    "punpckhbw %%xmm7,%%xmm1                   \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "punpcklwd %%xmm7,%%xmm2                   \n"
    "pmulld    %%xmm5,%%xmm2                   \n"
    "movdqa    %8,%%xmm5                       \n"
    "punpckhwd %%xmm7,%%xmm1                   \n"
    "pmulld    %%xmm5,%%xmm1                   \n"
    "paddd     %%xmm4,%%xmm3                   \n"
    "paddd     %%xmm2,%%xmm1                   \n"
    "sub       $0x10,%1                        \n"
    "paddd     %%xmm3,%%xmm1                   \n"
    "pshufd    $0xe,%%xmm1,%%xmm2              \n"
    "paddd     %%xmm2,%%xmm1                   \n"
    "pshufd    $0x1,%%xmm1,%%xmm2              \n"
    "paddd     %%xmm2,%%xmm1                   \n"
    "paddd     %%xmm1,%%xmm0                   \n"
    "jg        1b                              \n"
    "movd      %%xmm0,%3                       \n"
  : "+r"(src),        // %0
    "+r"(count),      // %1
    "+rm"(seed),      // %2
    "=g"(hash)        // %3
  : "m"(kHash16x33),  // %4
    "m"(kHashMul0),   // %5
    "m"(kHashMul1),   // %6
    "m"(kHashMul2),   // %7
    "m"(kHashMul3)    // %8
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
#endif
  );
  return hash;
}
#endif  // HAS_HASHDJB2_SSE41

// hash seed of 5381 recommended.
LIBYUV_API
uint32 HashDjb2(const uint8* src, uint64 count, uint32 seed) {
  uint32 (*HashDjb2_SSE)(const uint8* src, int count, uint32 seed) = HashDjb2_C;
#if defined(HAS_HASHDJB2_SSE41)
  if (TestCpuFlag(kCpuHasSSE41)) {
    HashDjb2_SSE = HashDjb2_SSE41;
  }
#endif

  const int kBlockSize = 1 << 15;  // 32768;
  while (count >= static_cast<uint64>(kBlockSize)) {
    seed = HashDjb2_SSE(src, kBlockSize, seed);
    src += kBlockSize;
    count -= kBlockSize;
  }
  int remainder = static_cast<int>(count) & ~15;
  if (remainder) {
    seed = HashDjb2_SSE(src, remainder, seed);
    src += remainder;
    count -= remainder;
  }
  remainder = static_cast<int>(count) & 15;
  if (remainder) {
    seed = HashDjb2_C(src, remainder, seed);
  }
  return seed;
}

#if !defined(YUV_DISABLE_ASM) && defined(__ARM_NEON__)
#define HAS_SUMSQUAREERROR_NEON

static uint32 SumSquareError_NEON(const uint8* src_a, const uint8* src_b,
                                  int count) {
  volatile uint32 sse;
  asm volatile (
    "vmov.u8    q7, #0                         \n"
    "vmov.u8    q9, #0                         \n"
    "vmov.u8    q8, #0                         \n"
    "vmov.u8    q10, #0                        \n"

    "1:                                        \n"
    "vld1.u8    {q0}, [%0]!                    \n"
    "vld1.u8    {q1}, [%1]!                    \n"
    "vsubl.u8   q2, d0, d2                     \n"
    "vsubl.u8   q3, d1, d3                     \n"
    "vmlal.s16  q7, d4, d4                     \n"
    "vmlal.s16  q8, d6, d6                     \n"
    "vmlal.s16  q8, d5, d5                     \n"
    "vmlal.s16  q10, d7, d7                    \n"
    "subs       %2, %2, #16                    \n"
    "bgt        1b                             \n"

    "vadd.u32   q7, q7, q8                     \n"
    "vadd.u32   q9, q9, q10                    \n"
    "vadd.u32   q10, q7, q9                    \n"
    "vpaddl.u32 q1, q10                        \n"
    "vadd.u64   d0, d2, d3                     \n"
    "vmov.32    %3, d0[0]                      \n"
    : "+r"(src_a),
      "+r"(src_b),
      "+r"(count),
      "=r"(sse)
    :
    : "memory", "cc", "q0", "q1", "q2", "q3", "q7", "q8", "q9", "q10");
  return sse;
}

#elif !defined(YUV_DISABLE_ASM) && defined(_M_IX86)
#define HAS_SUMSQUAREERROR_SSE2
__declspec(naked) __declspec(align(16))
static uint32 SumSquareError_SSE2(const uint8* src_a, const uint8* src_b,
                                  int count) {
  __asm {
    mov        eax, [esp + 4]    // src_a
    mov        edx, [esp + 8]    // src_b
    mov        ecx, [esp + 12]   // count
    pxor       xmm0, xmm0
    pxor       xmm5, xmm5
    sub        edx, eax

    align      16
  wloop:
    movdqa     xmm1, [eax]
    movdqa     xmm2, [eax + edx]
    lea        eax,  [eax + 16]
    sub        ecx, 16
    movdqa     xmm3, xmm1  // abs trick
    psubusb    xmm1, xmm2
    psubusb    xmm2, xmm3
    por        xmm1, xmm2
    movdqa     xmm2, xmm1
    punpcklbw  xmm1, xmm5
    punpckhbw  xmm2, xmm5
    pmaddwd    xmm1, xmm1
    pmaddwd    xmm2, xmm2
    paddd      xmm0, xmm1
    paddd      xmm0, xmm2
    jg         wloop

    pshufd     xmm1, xmm0, 0EEh
    paddd      xmm0, xmm1
    pshufd     xmm1, xmm0, 01h
    paddd      xmm0, xmm1
    movd       eax, xmm0
    ret
  }
}

#elif !defined(YUV_DISABLE_ASM) && (defined(__x86_64__) || defined(__i386__))
#define HAS_SUMSQUAREERROR_SSE2
static uint32 SumSquareError_SSE2(const uint8* src_a, const uint8* src_b,
                                  int count) {
  uint32 sse;
  asm volatile (
    "pxor      %%xmm0,%%xmm0                   \n"
    "pxor      %%xmm5,%%xmm5                   \n"
    "sub       %0,%1                           \n"
    ".p2align  4                               \n"
    "1:                                        \n"
    "movdqa    (%0),%%xmm1                     \n"
    "movdqa    (%0,%1,1),%%xmm2                \n"
    "lea       0x10(%0),%0                     \n"
    "sub       $0x10,%2                        \n"
    "movdqa    %%xmm1,%%xmm3                   \n"
    "psubusb   %%xmm2,%%xmm1                   \n"
    "psubusb   %%xmm3,%%xmm2                   \n"
    "por       %%xmm2,%%xmm1                   \n"
    "movdqa    %%xmm1,%%xmm2                   \n"
    "punpcklbw %%xmm5,%%xmm1                   \n"
    "punpckhbw %%xmm5,%%xmm2                   \n"
    "pmaddwd   %%xmm1,%%xmm1                   \n"
    "pmaddwd   %%xmm2,%%xmm2                   \n"
    "paddd     %%xmm1,%%xmm0                   \n"
    "paddd     %%xmm2,%%xmm0                   \n"
    "jg        1b                              \n"

    "pshufd    $0xee,%%xmm0,%%xmm1             \n"
    "paddd     %%xmm1,%%xmm0                   \n"
    "pshufd    $0x1,%%xmm0,%%xmm1              \n"
    "paddd     %%xmm1,%%xmm0                   \n"
    "movd      %%xmm0,%3                       \n"

  : "+r"(src_a),      // %0
    "+r"(src_b),      // %1
    "+r"(count),      // %2
    "=g"(sse)         // %3
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm5"
#endif
  );
  return sse;
}
#endif

static uint32 SumSquareError_C(const uint8* src_a, const uint8* src_b,
                               int count) {
  uint32 sse = 0u;
  for (int i = 0; i < count; ++i) {
    int diff = src_a[i] - src_b[i];
    sse += static_cast<uint32>(diff * diff);
  }
  return sse;
}

LIBYUV_API
uint64 ComputeSumSquareError(const uint8* src_a, const uint8* src_b,
                             int count) {
  uint32 (*SumSquareError)(const uint8* src_a, const uint8* src_b, int count) =
      SumSquareError_C;
#if defined(HAS_SUMSQUAREERROR_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SumSquareError = SumSquareError_NEON;
  }
#elif defined(HAS_SUMSQUAREERROR_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_a, 16) && IS_ALIGNED(src_b, 16)) {
    // Note only used for multiples of 16 so count is not checked.
    SumSquareError = SumSquareError_SSE2;
  }
#endif
  // 32K values will fit a 32bit int return value from SumSquareError.
  // After each block of 32K, accumulate into 64 bit int.
  const int kBlockSize = 1 << 15;  // 32768;
  uint64 sse = 0;
#ifdef _OPENMP
#pragma omp parallel for reduction(+: sse)
#endif
  for (int i = 0; i < (count - (kBlockSize - 1)); i += kBlockSize) {
    sse += SumSquareError(src_a + i, src_b + i, kBlockSize);
  }
  src_a += count & ~(kBlockSize - 1);
  src_b += count & ~(kBlockSize - 1);
  int remainder = count & (kBlockSize - 1) & ~15;
  if (remainder) {
    sse += SumSquareError(src_a, src_b, remainder);
    src_a += remainder;
    src_b += remainder;
  }
  remainder = count & 15;
  if (remainder) {
    sse += SumSquareError_C(src_a, src_b, remainder);
  }
  return sse;
}

LIBYUV_API
uint64 ComputeSumSquareErrorPlane(const uint8* src_a, int stride_a,
                                  const uint8* src_b, int stride_b,
                                  int width, int height) {
  uint32 (*SumSquareError)(const uint8* src_a, const uint8* src_b, int count) =
      SumSquareError_C;
#if defined(HAS_SUMSQUAREERROR_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SumSquareError = SumSquareError_NEON;
  }
#elif defined(HAS_SUMSQUAREERROR_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) && IS_ALIGNED(width, 16) &&
      IS_ALIGNED(src_a, 16) && IS_ALIGNED(stride_a, 16) &&
      IS_ALIGNED(src_b, 16) && IS_ALIGNED(stride_b, 16)) {
    SumSquareError = SumSquareError_SSE2;
  }
#endif

  uint64 sse = 0;
  for (int h = 0; h < height; ++h) {
    sse += SumSquareError(src_a, src_b, width);
    src_a += stride_a;
    src_b += stride_b;
  }

  return sse;
}

LIBYUV_API
double SumSquareErrorToPsnr(uint64 sse, uint64 count) {
  double psnr;
  if (sse > 0) {
    double mse = static_cast<double>(count) / static_cast<double>(sse);
    psnr = 10.0 * log10(255.0 * 255.0 * mse);
  } else {
    psnr = kMaxPsnr;      // Limit to prevent divide by 0
  }

  if (psnr > kMaxPsnr)
    psnr = kMaxPsnr;

  return psnr;
}

LIBYUV_API
double CalcFramePsnr(const uint8* src_a, int stride_a,
                     const uint8* src_b, int stride_b,
                     int width, int height) {
  const uint64 samples = width * height;
  const uint64 sse = ComputeSumSquareErrorPlane(src_a, stride_a,
                                                src_b, stride_b,
                                                width, height);
  return SumSquareErrorToPsnr(sse, samples);
}

LIBYUV_API
double I420Psnr(const uint8* src_y_a, int stride_y_a,
                const uint8* src_u_a, int stride_u_a,
                const uint8* src_v_a, int stride_v_a,
                const uint8* src_y_b, int stride_y_b,
                const uint8* src_u_b, int stride_u_b,
                const uint8* src_v_b, int stride_v_b,
                int width, int height) {
  const uint64 sse_y = ComputeSumSquareErrorPlane(src_y_a, stride_y_a,
                                                  src_y_b, stride_y_b,
                                                  width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const uint64 sse_u = ComputeSumSquareErrorPlane(src_u_a, stride_u_a,
                                                  src_u_b, stride_u_b,
                                                  width_uv, height_uv);
  const uint64 sse_v = ComputeSumSquareErrorPlane(src_v_a, stride_v_a,
                                                  src_v_b, stride_v_b,
                                                  width_uv, height_uv);
  const uint64 samples = width * height + 2 * (width_uv * height_uv);
  const uint64 sse = sse_y + sse_u + sse_v;
  return SumSquareErrorToPsnr(sse, samples);
}

static const int64 cc1 =  26634;  // (64^2*(.01*255)^2
static const int64 cc2 = 239708;  // (64^2*(.03*255)^2

static double Ssim8x8_C(const uint8* src_a, int stride_a,
                        const uint8* src_b, int stride_b) {
  int64 sum_a = 0;
  int64 sum_b = 0;
  int64 sum_sq_a = 0;
  int64 sum_sq_b = 0;
  int64 sum_axb = 0;

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      sum_a += src_a[j];
      sum_b += src_b[j];
      sum_sq_a += src_a[j] * src_a[j];
      sum_sq_b += src_b[j] * src_b[j];
      sum_axb += src_a[j] * src_b[j];
    }

    src_a += stride_a;
    src_b += stride_b;
  }

  const int64 count = 64;
  // scale the constants by number of pixels
  const int64 c1 = (cc1 * count * count) >> 12;
  const int64 c2 = (cc2 * count * count) >> 12;

  const int64 sum_a_x_sum_b = sum_a * sum_b;

  const int64 ssim_n = (2 * sum_a_x_sum_b + c1) *
                       (2 * count * sum_axb - 2 * sum_a_x_sum_b + c2);

  const int64 sum_a_sq = sum_a*sum_a;
  const int64 sum_b_sq = sum_b*sum_b;

  const int64 ssim_d = (sum_a_sq + sum_b_sq + c1) *
                       (count * sum_sq_a - sum_a_sq +
                        count * sum_sq_b - sum_b_sq + c2);

  if (ssim_d == 0.0)
    return DBL_MAX;
  return ssim_n * 1.0 / ssim_d;
}

// We are using a 8x8 moving window with starting location of each 8x8 window
// on the 4x4 pixel grid. Such arrangement allows the windows to overlap
// block boundaries to penalize blocking artifacts.
LIBYUV_API
double CalcFrameSsim(const uint8* src_a, int stride_a,
                     const uint8* src_b, int stride_b,
                     int width, int height) {
  int samples = 0;
  double ssim_total = 0;

  double (*Ssim8x8)(const uint8* src_a, int stride_a,
                    const uint8* src_b, int stride_b);

  Ssim8x8 = Ssim8x8_C;

  // sample point start with each 4x4 location
  for (int i = 0; i < height - 8; i += 4) {
    for (int j = 0; j < width - 8; j += 4) {
      ssim_total += Ssim8x8(src_a + j, stride_a, src_b + j, stride_b);
      samples++;
    }

    src_a += stride_a * 4;
    src_b += stride_b * 4;
  }

  ssim_total /= samples;
  return ssim_total;
}

LIBYUV_API
double I420Ssim(const uint8* src_y_a, int stride_y_a,
                const uint8* src_u_a, int stride_u_a,
                const uint8* src_v_a, int stride_v_a,
                const uint8* src_y_b, int stride_y_b,
                const uint8* src_u_b, int stride_u_b,
                const uint8* src_v_b, int stride_v_b,
                int width, int height) {
  const double ssim_y = CalcFrameSsim(src_y_a, stride_y_a,
                                      src_y_b, stride_y_b, width, height);
  const int width_uv = (width + 1) >> 1;
  const int height_uv = (height + 1) >> 1;
  const double ssim_u = CalcFrameSsim(src_u_a, stride_u_a,
                                      src_u_b, stride_u_b,
                                      width_uv, height_uv);
  const double ssim_v = CalcFrameSsim(src_v_a, stride_v_a,
                                      src_v_b, stride_v_b,
                                      width_uv, height_uv);
  return ssim_y * 0.8 + 0.1 * (ssim_u + ssim_v);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
