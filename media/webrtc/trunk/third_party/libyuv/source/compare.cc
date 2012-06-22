/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
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

#include "libyuv/basic_types.h"
#include "libyuv/cpu_id.h"
#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_SUMSQUAREERROR_NEON

static uint32 SumSquareError_NEON(const uint8* src_a,
                                  const uint8* src_b, int count) {
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
    "bhi        1b                             \n"

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
    : "memory", "cc", "q0", "q1", "q2", "q3", "q7", "q8", "q9", "q10"
  );
  return sse;
}

#elif defined(_M_IX86) && !defined(YUV_DISABLE_ASM)
#define HAS_SUMSQUAREERROR_SSE2
__declspec(naked)
static uint32 SumSquareError_SSE2(const uint8* src_a,
                                  const uint8* src_b, int count) {
  __asm {
    mov        eax, [esp + 4]    // src_a
    mov        edx, [esp + 8]    // src_b
    mov        ecx, [esp + 12]   // count
    pxor       xmm0, xmm0
    pxor       xmm5, xmm5
    sub        edx, eax

  wloop:
    movdqa     xmm1, [eax]
    movdqa     xmm2, [eax + edx]
    lea        eax,  [eax + 16]
    movdqa     xmm3, xmm1
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
    sub        ecx, 16
    ja         wloop

    pshufd     xmm1, xmm0, 0EEh
    paddd      xmm0, xmm1
    pshufd     xmm1, xmm0, 01h
    paddd      xmm0, xmm1
    movd       eax, xmm0
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)
#define HAS_SUMSQUAREERROR_SSE2
static uint32 SumSquareError_SSE2(const uint8* src_a,
                                  const uint8* src_b, int count) {
  uint32 sse;
  asm volatile (
    "pxor      %%xmm0,%%xmm0                   \n"
    "pxor      %%xmm5,%%xmm5                   \n"
    "sub       %0,%1                           \n"

    "1:                                        \n"
    "movdqa    (%0),%%xmm1                     \n"
    "movdqa    (%0,%1,1),%%xmm2                \n"
    "lea       0x10(%0),%0                     \n"
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
    "sub       $0x10,%2                        \n"
    "ja        1b                              \n"

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

static uint32 SumSquareError_C(const uint8* src_a,
                               const uint8* src_b, int count) {
  uint32 sse = 0u;
  for (int x = 0; x < count; ++x) {
    int diff = src_a[0] - src_b[0];
    sse += static_cast<uint32>(diff * diff);
    src_a += 1;
    src_b += 1;
  }
  return sse;
}

uint64 ComputeSumSquareError(const uint8* src_a,
                             const uint8* src_b, int count) {
  uint32 (*SumSquareError)(const uint8* src_a,
                           const uint8* src_b, int count);
#if defined(HAS_SUMSQUAREERROR_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SumSquareError = SumSquareError_NEON;
  } else
#elif defined(HAS_SUMSQUAREERROR_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(src_a, 16) && IS_ALIGNED(src_b, 16)) {
    SumSquareError = SumSquareError_SSE2;
  } else
#endif
  {
    SumSquareError = SumSquareError_C;
  }
  const int kBlockSize = 32768;
  uint64 sse = 0;
  while (count >= kBlockSize) {
    sse += SumSquareError(src_a, src_b, kBlockSize);
    src_a += kBlockSize;
    src_b += kBlockSize;
    count -= kBlockSize;
  }
  int remainder = count & ~15;
  if (remainder) {
    sse += SumSquareError(src_a, src_b, remainder);
    src_a += remainder;
    src_b += remainder;
    count -= remainder;
  }
  if (count) {
    sse += SumSquareError_C(src_a, src_b, count);
  }
  return sse;
}

uint64 ComputeSumSquareErrorPlane(const uint8* src_a, int stride_a,
                                  const uint8* src_b, int stride_b,
                                  int width, int height) {
  uint32 (*SumSquareError)(const uint8* src_a,
                           const uint8* src_b, int count);
#if defined(HAS_SUMSQUAREERROR_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(width, 16)) {
    SumSquareError = SumSquareError_NEON;
  } else
#endif
  {
    SumSquareError = SumSquareError_C;
  }

  uint64 sse = 0;
  for (int h = 0; h < height; ++h) {
    sse += SumSquareError(src_a, src_b, width);
    src_a += stride_a;
    src_b += stride_b;
  }

  return sse;
}

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

double CalcFramePsnr(const uint8* src_a, int stride_a,
                     const uint8* src_b, int stride_b,
                     int width, int height) {
  const uint64 samples = width * height;
  const uint64 sse = ComputeSumSquareErrorPlane(src_a, stride_a,
                                                src_b, stride_b,
                                                width, height);
  return SumSquareErrorToPsnr(sse, samples);
}

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
