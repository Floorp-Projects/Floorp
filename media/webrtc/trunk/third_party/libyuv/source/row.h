/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LIBYUV_SOURCE_ROW_H_
#define LIBYUV_SOURCE_ROW_H_

#include "libyuv/basic_types.h"

#define kMaxStride (2048 * 4)
#define IS_ALIGNED(p, a) (!((uintptr_t)(p) & ((a) - 1)))

#if defined(COVERAGE_ENABLED) || defined(TARGET_IPHONE_SIMULATOR)
#define YUV_DISABLE_ASM
#endif

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_FASTCONVERTYUVTOARGBROW_NEON
void FastConvertYUVToARGBRow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);
#define HAS_FASTCONVERTYUVTOBGRAROW_NEON
void FastConvertYUVToBGRARow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);
#define HAS_FASTCONVERTYUVTOABGRROW_NEON
void FastConvertYUVToABGRRow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);
#endif

// The following are available on all x86 platforms
#if (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)) && \
    !defined(YUV_DISABLE_ASM)
#define HAS_ABGRTOARGBROW_SSSE3
#define HAS_BGRATOARGBROW_SSSE3
#define HAS_BG24TOARGBROW_SSSE3
#define HAS_RAWTOARGBROW_SSSE3
#define HAS_RGB24TOYROW_SSSE3
#define HAS_RAWTOYROW_SSSE3
#define HAS_RGB24TOUVROW_SSSE3
#define HAS_RAWTOUVROW_SSSE3
#define HAS_ARGBTOYROW_SSSE3
#define HAS_BGRATOYROW_SSSE3
#define HAS_ABGRTOYROW_SSSE3
#define HAS_ARGBTOUVROW_SSSE3
#define HAS_BGRATOUVROW_SSSE3
#define HAS_ABGRTOUVROW_SSSE3
#define HAS_I400TOARGBROW_SSE2
#define HAS_FASTCONVERTYTOARGBROW_SSE2
#define HAS_FASTCONVERTYUVTOARGBROW_SSSE3
#define HAS_FASTCONVERTYUVTOBGRAROW_SSSE3
#define HAS_FASTCONVERTYUVTOABGRROW_SSSE3
#define HAS_FASTCONVERTYUV444TOARGBROW_SSSE3
#define HAS_REVERSE_ROW_SSSE3
#define HAS_REVERSE_ROW_SSE2
#endif

// The following are available on Neon platforms
#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_REVERSE_ROW_NEON
#endif

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#ifdef HAS_ARGBTOYROW_SSSE3
void ARGBToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix);
void BGRAToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix);
void ABGRToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix);
void ARGBToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width);
void BGRAToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width);
void ABGRToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width);
#endif
#if defined(HAS_BG24TOARGBROW_SSSE3) && defined(HAS_ARGBTOYROW_SSSE3)
#define HASRGB24TOYROW_SSSE3
#endif
#ifdef HASRGB24TOYROW_SSSE3
void RGB24ToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix);
void RAWToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix);
void RGB24ToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                        uint8* dst_u, uint8* dst_v, int width);
void RAWToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                      uint8* dst_u, uint8* dst_v, int width);
#endif
#ifdef HAS_REVERSE_ROW_SSSE3
void ReverseRow_SSSE3(const uint8* src, uint8* dst, int width);
#endif
#ifdef HAS_REVERSE_ROW_SSE2
void ReverseRow_SSE2(const uint8* src, uint8* dst, int width);
#endif
#ifdef HAS_REVERSE_ROW_NEON
void ReverseRow_NEON(const uint8* src, uint8* dst, int width);
#endif
void ReverseRow_C(const uint8* src, uint8* dst, int width);

void ARGBToYRow_C(const uint8* src_argb, uint8* dst_y, int pix);
void BGRAToYRow_C(const uint8* src_argb, uint8* dst_y, int pix);
void ABGRToYRow_C(const uint8* src_argb, uint8* dst_y, int pix);
void RGB24ToYRow_C(const uint8* src_argb, uint8* dst_y, int pix);
void RAWToYRow_C(const uint8* src_argb, uint8* dst_y, int pix);
void ARGBToUVRow_C(const uint8* src_argb0, int src_stride_argb,
                   uint8* dst_u, uint8* dst_v, int width);
void BGRAToUVRow_C(const uint8* src_argb0, int src_stride_argb,
                   uint8* dst_u, uint8* dst_v, int width);
void ABGRToUVRow_C(const uint8* src_argb0, int src_stride_argb,
                   uint8* dst_u, uint8* dst_v, int width);
void RGB24ToUVRow_C(const uint8* src_argb0, int src_stride_argb,
                    uint8* dst_u, uint8* dst_v, int width);
void RAWToUVRow_C(const uint8* src_argb0, int src_stride_argb,
                  uint8* dst_u, uint8* dst_v, int width);

#ifdef HAS_BG24TOARGBROW_SSSE3
void ABGRToARGBRow_SSSE3(const uint8* src_abgr, uint8* dst_argb, int pix);
void BGRAToARGBRow_SSSE3(const uint8* src_bgra, uint8* dst_argb, int pix);
void BG24ToARGBRow_SSSE3(const uint8* src_bg24, uint8* dst_argb, int pix);
void RAWToARGBRow_SSSE3(const uint8* src_bg24, uint8* dst_argb, int pix);
#endif
void ABGRToARGBRow_C(const uint8* src_abgr, uint8* dst_argb, int pix);
void BGRAToARGBRow_C(const uint8* src_bgra, uint8* dst_argb, int pix);
void BG24ToARGBRow_C(const uint8* src_bg24, uint8* dst_argb, int pix);
void RAWToARGBRow_C(const uint8* src_bg24, uint8* dst_argb, int pix);

#ifdef HAS_I400TOARGBROW_SSE2
void I400ToARGBRow_SSE2(const uint8* src_y, uint8* dst_argb, int pix);
#endif
void I400ToARGBRow_C(const uint8* src_y, uint8* dst_argb, int pix);

#if defined(_MSC_VER)
#define SIMD_ALIGNED(var) __declspec(align(16)) var
typedef __declspec(align(16)) signed char vec8[16];
typedef __declspec(align(16)) unsigned char uvec8[16];
typedef __declspec(align(16)) signed short vec16[8];
#else // __GNUC__
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
typedef signed char __attribute__((vector_size(16))) vec8;
typedef unsigned char __attribute__((vector_size(16))) uvec8;
typedef signed short __attribute__((vector_size(16))) vec16;
#endif

extern "C" SIMD_ALIGNED(const int16 kCoefficientsRgbY[768][4]);
extern "C" SIMD_ALIGNED(const int16 kCoefficientsBgraY[768][4]);
extern "C" SIMD_ALIGNED(const int16 kCoefficientsAbgrY[768][4]);

void FastConvertYUVToARGBRow_C(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width);

void FastConvertYUVToBGRARow_C(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width);

void FastConvertYUVToABGRRow_C(const uint8* y_buf,
                               const uint8* u_buf,
                               const uint8* v_buf,
                               uint8* rgb_buf,
                               int width);

void FastConvertYUV444ToARGBRow_C(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);

void FastConvertYToARGBRow_C(const uint8* y_buf,
                             uint8* rgb_buf,
                             int width);

#ifdef HAS_FASTCONVERTYUVTOARGBROW_SSE2
void FastConvertYUVToARGBRow_SSE2(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);

void FastConvertYUVToARGBRow4_SSE2(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);

void FastConvertYUVToBGRARow_SSE2(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);

void FastConvertYUVToABGRRow_SSE2(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width);

void FastConvertYUV444ToARGBRow_SSE2(const uint8* y_buf,
                                     const uint8* u_buf,
                                     const uint8* v_buf,
                                     uint8* rgb_buf,
                                     int width);

void FastConvertYToARGBRow_SSE2(const uint8* y_buf,
                                uint8* rgb_buf,
                                int width);
#endif

#ifdef HAS_FASTCONVERTYUVTOARGBROW_SSSE3
void FastConvertYUVToARGBRow_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);

void FastConvertYUVToBGRARow_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);

void FastConvertYUVToABGRRow_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int width);

void FastConvertYUV444ToARGBRow_SSSE3(const uint8* y_buf,
                                      const uint8* u_buf,
                                      const uint8* v_buf,
                                      uint8* rgb_buf,
                                      int width);

#endif

#ifdef HAS_FASTCONVERTYTOARGBROW_SSE2
void FastConvertYToARGBRow_SSE2(const uint8* y_buf,
                                uint8* rgb_buf,
                                int width);

#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // LIBYUV_SOURCE_ROW_H_
