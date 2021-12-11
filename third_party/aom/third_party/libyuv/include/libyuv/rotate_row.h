/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef INCLUDE_LIBYUV_ROTATE_ROW_H_  // NOLINT
#define INCLUDE_LIBYUV_ROTATE_ROW_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(__pnacl__) || defined(__CLR_VER) || \
    (defined(__i386__) && !defined(__SSE2__))
#define LIBYUV_DISABLE_X86
#endif

// Visual C 2012 required for AVX2.
#if defined(_M_IX86) && !defined(__clang__) && \
    defined(_MSC_VER) && _MSC_VER >= 1700
#define VISUALC_HAS_AVX2 1
#endif  // VisualStudio >= 2012

// TODO(fbarchard): switch to standard form of inline; fails on clangcl.
#if !defined(LIBYUV_DISABLE_X86) && \
    (defined(_M_IX86) || defined(__x86_64__) || defined(__i386__))
#if defined(__APPLE__) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".private_extern _" #name "                \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#elif defined(__MINGW32__) || defined(__CYGWIN__) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#else
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
#name ":                                       \n"
#endif
#endif

// The following are available for Visual C:
#if !defined(LIBYUV_DISABLE_X86) && defined(_M_IX86) && \
    defined(_MSC_VER) && !defined(__clang__)
#define HAS_TRANSPOSEWX8_SSSE3
#define HAS_TRANSPOSEUVWX8_SSE2
#endif

// The following are available for GCC but not NaCL:
#if !defined(LIBYUV_DISABLE_X86) && \
    (defined(__i386__) || (defined(__x86_64__) && !defined(__native_client__)))
#define HAS_TRANSPOSEWX8_SSSE3
#endif

// The following are available for 32 bit GCC:
#if !defined(LIBYUV_DISABLE_X86) && defined(__i386__)  && !defined(__clang__)
#define HAS_TRANSPOSEUVWX8_SSE2
#endif

// The following are available for 64 bit GCC but not NaCL:
#if !defined(LIBYUV_DISABLE_X86) && !defined(__native_client__) && \
    defined(__x86_64__)
#define HAS_TRANSPOSEWX8_FAST_SSSE3
#define HAS_TRANSPOSEUVWX8_SSE2
#endif

#if !defined(LIBYUV_DISABLE_NEON) && !defined(__native_client__) && \
    (defined(__ARM_NEON__) || defined(LIBYUV_NEON) || defined(__aarch64__))
#define HAS_TRANSPOSEWX8_NEON
#define HAS_TRANSPOSEUVWX8_NEON
#endif

#if !defined(LIBYUV_DISABLE_MIPS) && !defined(__native_client__) && \
    defined(__mips__) && \
    defined(__mips_dsp) && (__mips_dsp_rev >= 2)
#define HAS_TRANSPOSEWX8_MIPS_DSPR2
#define HAS_TRANSPOSEUVWx8_MIPS_DSPR2
#endif  // defined(__mips__)

void TransposeWxH_C(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride, int width, int height);

void TransposeWx8_C(const uint8* src, int src_stride,
                    uint8* dst, int dst_stride, int width);
void TransposeWx8_NEON(const uint8* src, int src_stride,
                       uint8* dst, int dst_stride, int width);
void TransposeWx8_SSSE3(const uint8* src, int src_stride,
                        uint8* dst, int dst_stride, int width);
void TransposeWx8_Fast_SSSE3(const uint8* src, int src_stride,
                             uint8* dst, int dst_stride, int width);
void TransposeWx8_MIPS_DSPR2(const uint8* src, int src_stride,
                             uint8* dst, int dst_stride, int width);

void TransposeWx8_Any_NEON(const uint8* src, int src_stride,
                           uint8* dst, int dst_stride, int width);
void TransposeWx8_Any_SSSE3(const uint8* src, int src_stride,
                            uint8* dst, int dst_stride, int width);
void TransposeWx8_Fast_Any_SSSE3(const uint8* src, int src_stride,
                                 uint8* dst, int dst_stride, int width);
void TransposeWx8_Any_MIPS_DSPR2(const uint8* src, int src_stride,
                                 uint8* dst, int dst_stride, int width);

void TransposeUVWxH_C(const uint8* src, int src_stride,
                      uint8* dst_a, int dst_stride_a,
                      uint8* dst_b, int dst_stride_b,
                      int width, int height);

void TransposeUVWx8_C(const uint8* src, int src_stride,
                      uint8* dst_a, int dst_stride_a,
                      uint8* dst_b, int dst_stride_b, int width);
void TransposeUVWx8_SSE2(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b, int width);
void TransposeUVWx8_NEON(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b, int width);
void TransposeUVWx8_MIPS_DSPR2(const uint8* src, int src_stride,
                               uint8* dst_a, int dst_stride_a,
                               uint8* dst_b, int dst_stride_b, int width);

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif  // INCLUDE_LIBYUV_ROTATE_ROW_H_  NOLINT
