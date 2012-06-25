/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "row.h"

#include "libyuv/basic_types.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#ifdef __APPLE__
#define CONST
#else
#define CONST static const
#endif

#ifdef HAS_ARGBTOUVROW_SSSE3
CONST vec8 kARGBToU = {
  112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0
};

CONST vec8 kARGBToV = {
  -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0
};

CONST uvec8 kAddUV128 = {
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u
};
#endif

#ifdef HAS_ARGBTOYROW_SSSE3

// Constant multiplication table for converting ARGB to I400.
CONST vec8 kARGBToY = {
  13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0
};

CONST uvec8 kAddY16 = {
  16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u
};

// Shuffle table for converting BG24 to ARGB.
CONST uvec8 kShuffleMaskBG24ToARGB = {
  0u, 1u, 2u, 12u, 3u, 4u, 5u, 13u, 6u, 7u, 8u, 14u, 9u, 10u, 11u, 15u
};

// Shuffle table for converting RAW to ARGB.
CONST uvec8 kShuffleMaskRAWToARGB = {
  2u, 1u, 0u, 12u, 5u, 4u, 3u, 13u, 8u, 7u, 6u, 14u, 11u, 10u, 9u, 15u
};

// Shuffle table for converting ABGR to ARGB.
CONST uvec8 kShuffleMaskABGRToARGB = {
  2u, 1u, 0u, 3u, 6u, 5u, 4u, 7u, 10u, 9u, 8u, 11u, 14u, 13u, 12u, 15u
};

// Shuffle table for converting BGRA to ARGB.
CONST uvec8 kShuffleMaskBGRAToARGB = {
  3u, 2u, 1u, 0u, 7u, 6u, 5u, 4u, 11u, 10u, 9u, 8u, 15u, 14u, 13u, 12u
};

void I400ToARGBRow_SSE2(const uint8* src_y, uint8* dst_argb, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "pslld      $0x18,%%xmm5                     \n"
"1:                                            \n"
  "movq       (%0),%%xmm0                      \n"
  "lea        0x8(%0),%0                       \n"
  "punpcklbw  %%xmm0,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "punpcklwd  %%xmm0,%%xmm0                    \n"
  "punpckhwd  %%xmm1,%%xmm1                    \n"
  "por        %%xmm5,%%xmm0                    \n"
  "por        %%xmm5,%%xmm1                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "movdqa     %%xmm1,0x10(%1)                  \n"
  "lea        0x20(%1),%1                      \n"
  "sub        $0x8,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_y),     // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm5"
#endif
);
}

void ABGRToARGBRow_SSSE3(const uint8* src_abgr, uint8* dst_argb, int pix) {
  asm volatile (
  "movdqa     %3,%%xmm5                        \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "lea        0x10(%0),%0                      \n"
  "pshufb     %%xmm5,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x4,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_abgr),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskABGRToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif

);
}

void BGRAToARGBRow_SSSE3(const uint8* src_bgra, uint8* dst_argb, int pix) {
  asm volatile (
  "movdqa     %3,%%xmm5                        \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "lea        0x10(%0),%0                      \n"
  "pshufb     %%xmm5,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x4,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_bgra),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskBGRAToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif
);
}

void BG24ToARGBRow_SSSE3(const uint8* src_bg24, uint8* dst_argb, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"  // generate mask 0xff000000
  "pslld      $0x18,%%xmm5                     \n"
  "movdqa     %3,%%xmm4                        \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     0x20(%0),%%xmm3                  \n"
  "lea        0x30(%0),%0                      \n"
  "movdqa     %%xmm3,%%xmm2                    \n"
  "palignr    $0x8,%%xmm1,%%xmm2               \n"
  "pshufb     %%xmm4,%%xmm2                    \n"
  "por        %%xmm5,%%xmm2                    \n"
  "palignr    $0xc,%%xmm0,%%xmm1               \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "movdqa     %%xmm2,0x20(%1)                  \n"
  "por        %%xmm5,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm1                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "por        %%xmm5,%%xmm1                    \n"
  "palignr    $0x4,%%xmm3,%%xmm3               \n"
  "pshufb     %%xmm4,%%xmm3                    \n"
  "movdqa     %%xmm1,0x10(%1)                  \n"
  "por        %%xmm5,%%xmm3                    \n"
  "movdqa     %%xmm3,0x30(%1)                  \n"
  "lea        0x40(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_bg24),  // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskBG24ToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
);
}

void RAWToARGBRow_SSSE3(const uint8* src_raw, uint8* dst_argb, int pix) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"  // generate mask 0xff000000
  "pslld      $0x18,%%xmm5                     \n"
  "movdqa     %3,%%xmm4                        \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     0x20(%0),%%xmm3                  \n"
  "lea        0x30(%0),%0                      \n"
  "movdqa     %%xmm3,%%xmm2                    \n"
  "palignr    $0x8,%%xmm1,%%xmm2               \n"
  "pshufb     %%xmm4,%%xmm2                    \n"
  "por        %%xmm5,%%xmm2                    \n"
  "palignr    $0xc,%%xmm0,%%xmm1               \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "movdqa     %%xmm2,0x20(%1)                  \n"
  "por        %%xmm5,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm1                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "por        %%xmm5,%%xmm1                    \n"
  "palignr    $0x4,%%xmm3,%%xmm3               \n"
  "pshufb     %%xmm4,%%xmm3                    \n"
  "movdqa     %%xmm1,0x10(%1)                  \n"
  "por        %%xmm5,%%xmm3                    \n"
  "movdqa     %%xmm3,0x30(%1)                  \n"
  "lea        0x40(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_raw),   // %0
    "+r"(dst_argb),  // %1
    "+r"(pix)        // %2
  : "m"(kShuffleMaskRAWToARGB)  // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
);
}

void ARGBToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  asm volatile (
  "movdqa     %4,%%xmm5                        \n"
  "movdqa     %3,%%xmm4                        \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     0x20(%0),%%xmm2                  \n"
  "movdqa     0x30(%0),%%xmm3                  \n"
  "pmaddubsw  %%xmm4,%%xmm0                    \n"
  "pmaddubsw  %%xmm4,%%xmm1                    \n"
  "pmaddubsw  %%xmm4,%%xmm2                    \n"
  "pmaddubsw  %%xmm4,%%xmm3                    \n"
  "lea        0x40(%0),%0                      \n"
  "phaddw     %%xmm1,%%xmm0                    \n"
  "phaddw     %%xmm3,%%xmm2                    \n"
  "psrlw      $0x7,%%xmm0                      \n"
  "psrlw      $0x7,%%xmm2                      \n"
  "packuswb   %%xmm2,%%xmm0                    \n"
  "paddb      %%xmm5,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_argb),  // %0
    "+r"(dst_y),     // %1
    "+r"(pix)        // %2
  : "m"(kARGBToY),   // %3
    "m"(kAddY16)     // %4
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif

);
}
#endif

#ifdef HAS_ARGBTOUVROW_SSSE3
void ARGBToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
 asm volatile (
  "movdqa     %0,%%xmm4                        \n"
  "movdqa     %1,%%xmm3                        \n"
  "movdqa     %2,%%xmm5                        \n"
  :
  : "m"(kARGBToU),         // %0
    "m"(kARGBToV),         // %1
    "m"(kAddUV128)         // %2
  :
#if defined(__SSE2__)
    "xmm3", "xmm4", "xmm5"
#endif
 );
 asm volatile (
  "sub        %1,%2                            \n"
"1:                                            \n"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     0x20(%0),%%xmm2                  \n"
  "movdqa     0x30(%0),%%xmm6                  \n"
  "pavgb      (%0,%4,1),%%xmm0                 \n"
  "pavgb      0x10(%0,%4,1),%%xmm1             \n"
  "pavgb      0x20(%0,%4,1),%%xmm2             \n"
  "pavgb      0x30(%0,%4,1),%%xmm6             \n"
  "lea        0x40(%0),%0                      \n"
  "movdqa     %%xmm0,%%xmm7                    \n"
  "shufps     $0x88,%%xmm1,%%xmm0              \n"
  "shufps     $0xdd,%%xmm1,%%xmm7              \n"
  "pavgb      %%xmm7,%%xmm0                    \n"
  "movdqa     %%xmm2,%%xmm7                    \n"
  "shufps     $0x88,%%xmm6,%%xmm2              \n"
  "shufps     $0xdd,%%xmm6,%%xmm7              \n"
  "pavgb      %%xmm7,%%xmm2                    \n"
  "movdqa     %%xmm0,%%xmm1                    \n"
  "movdqa     %%xmm2,%%xmm6                    \n"
  "pmaddubsw  %%xmm4,%%xmm0                    \n"
  "pmaddubsw  %%xmm4,%%xmm2                    \n"
  "pmaddubsw  %%xmm3,%%xmm1                    \n"
  "pmaddubsw  %%xmm3,%%xmm6                    \n"
  "phaddw     %%xmm2,%%xmm0                    \n"
  "phaddw     %%xmm6,%%xmm1                    \n"
  "psraw      $0x8,%%xmm0                      \n"
  "psraw      $0x8,%%xmm1                      \n"
  "packsswb   %%xmm1,%%xmm0                    \n"
  "paddb      %%xmm5,%%xmm0                    \n"
  "movlps     %%xmm0,(%1)                      \n"
  "movhps     %%xmm0,(%1,%2,1)                 \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x10,%3                         \n"
  "ja         1b                               \n"
  : "+r"(src_argb0),       // %0
    "+r"(dst_u),           // %1
    "+r"(dst_v),           // %2
    "+rm"(width)           // %3
  : "r"(static_cast<intptr_t>(src_stride_argb))
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm6", "xmm7"
#endif
);
}
#endif

#ifdef HAS_FASTCONVERTYUVTOARGBROW_SSSE3
#define UB 127 /* min(63,static_cast<int8>(2.018 * 64)) */
#define UG -25 /* static_cast<int8>(-0.391 * 64 - 0.5) */
#define UR 0

#define VB 0
#define VG -52 /* static_cast<int8>(-0.813 * 64 - 0.5) */
#define VR 102 /* static_cast<int8>(1.596 * 64 + 0.5) */

// Bias
#define BB UB * 128 + VB * 128
#define BG UG * 128 + VG * 128
#define BR UR * 128 + VR * 128

#define YG 74 /* static_cast<int8>(1.164 * 64 + 0.5) */

#if defined(__APPLE__) || defined(__x86_64__)
#define OMITFP
#else
#define OMITFP __attribute__((optimize("omit-frame-pointer")))
#endif

struct {
  vec8 kUVToB;
  vec8 kUVToG;
  vec8 kUVToR;
  vec16 kUVBiasB;
  vec16 kUVBiasG;
  vec16 kUVBiasR;
  vec16 kYSub16;
  vec16 kYToRgb;
} CONST SIMD_ALIGNED(kYuvConstants) = {
  { UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB },
  { UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG },
  { UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR },
  { BB, BB, BB, BB, BB, BB, BB, BB },
  { BG, BG, BG, BG, BG, BG, BG, BG },
  { BR, BR, BR, BR, BR, BR, BR, BR },
  { 16, 16, 16, 16, 16, 16, 16, 16 },
  { YG, YG, YG, YG, YG, YG, YG, YG }
};

// Convert 8 pixels
#define YUVTORGB                                                               \
  "movd        (%1),%%xmm0                     \n"                             \
  "movd        (%1,%2,1),%%xmm1                \n"                             \
  "lea         0x4(%1),%1                      \n"                             \
  "punpcklbw   %%xmm1,%%xmm0                   \n"                             \
  "punpcklwd   %%xmm0,%%xmm0                   \n"                             \
  "movdqa      %%xmm0,%%xmm1                   \n"                             \
  "movdqa      %%xmm0,%%xmm2                   \n"                             \
  "pmaddubsw   (%5),%%xmm0                     \n"                             \
  "pmaddubsw   16(%5),%%xmm1                   \n"                             \
  "pmaddubsw   32(%5),%%xmm2                   \n"                             \
  "psubw       48(%5),%%xmm0                   \n"                             \
  "psubw       64(%5),%%xmm1                   \n"                             \
  "psubw       80(%5),%%xmm2                   \n"                             \
  "movq        (%0),%%xmm3                     \n"                             \
  "lea         0x8(%0),%0                      \n"                             \
  "punpcklbw   %%xmm4,%%xmm3                   \n"                             \
  "psubsw      96(%5),%%xmm3                   \n"                             \
  "pmullw      112(%5),%%xmm3                  \n"                             \
  "paddsw      %%xmm3,%%xmm0                   \n"                             \
  "paddsw      %%xmm3,%%xmm1                   \n"                             \
  "paddsw      %%xmm3,%%xmm2                   \n"                             \
  "psraw       $0x6,%%xmm0                     \n"                             \
  "psraw       $0x6,%%xmm1                     \n"                             \
  "psraw       $0x6,%%xmm2                     \n"                             \
  "packuswb    %%xmm0,%%xmm0                   \n"                             \
  "packuswb    %%xmm1,%%xmm1                   \n"                             \
  "packuswb    %%xmm2,%%xmm2                   \n"

void OMITFP FastConvertYUVToARGBRow_SSSE3(const uint8* y_buf,  // rdi
                                          const uint8* u_buf,  // rsi
                                          const uint8* v_buf,  // rdx
                                          uint8* rgb_buf,      // rcx
                                          int width) {         // r8
  asm volatile (
    "sub         %1,%2                         \n"
    "pcmpeqb     %%xmm5,%%xmm5                 \n"
    "pxor        %%xmm4,%%xmm4                 \n"

  "1:                                          \n"
    YUVTORGB
    "punpcklbw   %%xmm1,%%xmm0                 \n"
    "punpcklbw   %%xmm5,%%xmm2                 \n"
    "movdqa      %%xmm0,%%xmm1                 \n"
    "punpcklwd   %%xmm2,%%xmm0                 \n"
    "punpckhwd   %%xmm2,%%xmm1                 \n"
    "movdqa      %%xmm0,(%3)                   \n"
    "movdqa      %%xmm1,0x10(%3)               \n"
    "lea         0x20(%3),%3                   \n"
    "sub         $0x8,%4                       \n"
    "ja          1b                            \n"
  : "+r"(y_buf),    // %0
    "+r"(u_buf),    // %1
    "+r"(v_buf),    // %2
    "+r"(rgb_buf),  // %3
    "+rm"(width)    // %4
  : "r"(&kYuvConstants.kUVToB) // %5
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP FastConvertYUVToBGRARow_SSSE3(const uint8* y_buf,  // rdi
                                          const uint8* u_buf,  // rsi
                                          const uint8* v_buf,  // rdx
                                          uint8* rgb_buf,      // rcx
                                          int width) {         // r8
  asm volatile (
    "sub         %1,%2                         \n"
    "pcmpeqb     %%xmm5,%%xmm5                 \n"
    "pxor        %%xmm4,%%xmm4                 \n"

  "1:                                          \n"
    YUVTORGB
    "pcmpeqb     %%xmm5,%%xmm5                 \n"
    "punpcklbw   %%xmm0,%%xmm1                 \n"
    "punpcklbw   %%xmm2,%%xmm5                 \n"
    "movdqa      %%xmm5,%%xmm0                 \n"
    "punpcklwd   %%xmm1,%%xmm5                 \n"
    "punpckhwd   %%xmm1,%%xmm0                 \n"
    "movdqa      %%xmm5,(%3)                   \n"
    "movdqa      %%xmm0,0x10(%3)               \n"
    "lea         0x20(%3),%3                   \n"
    "sub         $0x8,%4                       \n"
    "ja          1b                            \n"
  : "+r"(y_buf),    // %0
    "+r"(u_buf),    // %1
    "+r"(v_buf),    // %2
    "+r"(rgb_buf),  // %3
    "+rm"(width)    // %4
  : "r"(&kYuvConstants.kUVToB) // %5
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP FastConvertYUVToABGRRow_SSSE3(const uint8* y_buf,  // rdi
                                          const uint8* u_buf,  // rsi
                                          const uint8* v_buf,  // rdx
                                          uint8* rgb_buf,      // rcx
                                          int width) {         // r8
  asm volatile (
    "sub         %1,%2                         \n"
    "pcmpeqb     %%xmm5,%%xmm5                 \n"
    "pxor        %%xmm4,%%xmm4                 \n"

  "1:                                          \n"
    YUVTORGB
    "punpcklbw   %%xmm1,%%xmm2                 \n"
    "punpcklbw   %%xmm5,%%xmm0                 \n"
    "movdqa      %%xmm2,%%xmm1                 \n"
    "punpcklwd   %%xmm0,%%xmm2                 \n"
    "punpckhwd   %%xmm0,%%xmm1                 \n"
    "movdqa      %%xmm2,(%3)                   \n"
    "movdqa      %%xmm1,0x10(%3)               \n"
    "lea         0x20(%3),%3                   \n"
    "sub         $0x8,%4                       \n"
    "ja          1b                            \n"
  : "+r"(y_buf),    // %0
    "+r"(u_buf),    // %1
    "+r"(v_buf),    // %2
    "+r"(rgb_buf),  // %3
    "+rm"(width)    // %4
  : "r"(&kYuvConstants.kUVToB) // %5
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}

void OMITFP FastConvertYUV444ToARGBRow_SSSE3(const uint8* y_buf,  // rdi
                                             const uint8* u_buf,  // rsi
                                             const uint8* v_buf,  // rdx
                                             uint8* rgb_buf,      // rcx
                                             int width) {         // r8
  asm volatile (
    "sub         %1,%2                         \n"
    "pcmpeqb     %%xmm5,%%xmm5                 \n"
    "pxor        %%xmm4,%%xmm4                 \n"

  "1:                                          \n"
    "movd        (%1),%%xmm0                   \n"
    "movd        (%1,%2,1),%%xmm1              \n"
    "lea         0x4(%1),%1                    \n"
    "punpcklbw   %%xmm1,%%xmm0                 \n"
    "movdqa      %%xmm0,%%xmm1                 \n"
    "movdqa      %%xmm0,%%xmm2                 \n"
    "pmaddubsw   (%5),%%xmm0                   \n"
    "pmaddubsw   16(%5),%%xmm1                 \n"
    "pmaddubsw   32(%5),%%xmm2                 \n"
    "psubw       48(%5),%%xmm0                 \n"
    "psubw       64(%5),%%xmm1                 \n"
    "psubw       80(%5),%%xmm2                 \n"
    "movd        (%0),%%xmm3                   \n"
    "lea         0x4(%0),%0                    \n"
    "punpcklbw   %%xmm4,%%xmm3                 \n"
    "psubsw      96(%5),%%xmm3                 \n"
    "pmullw      112(%5),%%xmm3                \n"
    "paddsw      %%xmm3,%%xmm0                 \n"
    "paddsw      %%xmm3,%%xmm1                 \n"
    "paddsw      %%xmm3,%%xmm2                 \n"
    "psraw       $0x6,%%xmm0                   \n"
    "psraw       $0x6,%%xmm1                   \n"
    "psraw       $0x6,%%xmm2                   \n"
    "packuswb    %%xmm0,%%xmm0                 \n"
    "packuswb    %%xmm1,%%xmm1                 \n"
    "packuswb    %%xmm2,%%xmm2                 \n"
    "punpcklbw   %%xmm1,%%xmm0                 \n"
    "punpcklbw   %%xmm5,%%xmm2                 \n"
    "punpcklwd   %%xmm2,%%xmm0                 \n"
    "movdqa      %%xmm0,(%3)                   \n"
    "lea         0x10(%3),%3                   \n"
    "sub         $0x4,%4                       \n"
    "ja          1b                            \n"
  : "+r"(y_buf),    // %0
    "+r"(u_buf),    // %1
    "+r"(v_buf),    // %2
    "+r"(rgb_buf),  // %3
    "+rm"(width)    // %4
  : "r"(&kYuvConstants.kUVToB) // %5
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5"
#endif
  );
}
#endif

#ifdef HAS_FASTCONVERTYTOARGBROW_SSE2

void FastConvertYToARGBRow_SSE2(const uint8* y_buf,  // rdi
                                uint8* rgb_buf,      // rcx
                                int width) {         // r8
  asm volatile (
  "pcmpeqb     %%xmm4,%%xmm4                   \n"
  "pslld       $0x18,%%xmm4                    \n"
  "mov         $0x10001000,%%eax               \n"
  "movd        %%eax,%%xmm3                    \n"
  "pshufd      $0x0,%%xmm3,%%xmm3              \n"
  "mov         $0x012a012a,%%eax               \n"
  "movd        %%eax,%%xmm2                    \n"
  "pshufd      $0x0,%%xmm2,%%xmm2              \n"

  "1:                                          \n"
    // Step 1: Scale Y contribution to 8 G values. G = (y - 16) * 1.164
    "movq        (%0),%%xmm0                   \n"
    "lea         0x8(%0),%0                    \n"
    "punpcklbw   %%xmm0,%%xmm0                 \n"
    "psubusw     %%xmm3,%%xmm0                 \n"
    "pmulhuw     %%xmm2,%%xmm0                 \n"
    "packuswb    %%xmm0,%%xmm0                 \n"

    // Step 2: Weave into ARGB
    "punpcklbw   %%xmm0,%%xmm0                 \n"
    "movdqa      %%xmm0,%%xmm1                 \n"
    "punpcklwd   %%xmm0,%%xmm0                 \n"
    "punpckhwd   %%xmm1,%%xmm1                 \n"
    "por         %%xmm4,%%xmm0                 \n"
    "por         %%xmm4,%%xmm1                 \n"
    "movdqa      %%xmm0,(%1)                   \n"
    "movdqa      %%xmm1,16(%1)                 \n"
    "lea         32(%1),%1                     \n"

    "sub         $0x8,%2                       \n"
    "ja          1b                            \n"
  : "+r"(y_buf),    // %0
    "+r"(rgb_buf),  // %1
    "+rm"(width)    // %2
  :
  : "memory", "cc", "eax"
#if defined(__SSE2__)
    , "xmm0", "xmm1", "xmm2", "xmm3", "xmm4"
#endif
  );
}
#endif

#ifdef HAS_ARGBTOYROW_SSSE3
void ABGRToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  SIMD_ALIGNED(uint8 row[kMaxStride]);
  ABGRToARGBRow_SSSE3(src_argb, row, pix);
  ARGBToYRow_SSSE3(row, dst_y, pix);
}

void BGRAToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
  SIMD_ALIGNED(uint8 row[kMaxStride]);
  BGRAToARGBRow_SSSE3(src_argb, row, pix);
  ARGBToYRow_SSSE3(row, dst_y, pix);
}
#endif

#ifdef HAS_ARGBTOUVROW_SSSE3
void ABGRToUVRow_SSSE3(const uint8* src_argb, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int pix) {
  SIMD_ALIGNED(uint8 row[kMaxStride * 2]);
  ABGRToARGBRow_SSSE3(src_argb, row, pix);
  ABGRToARGBRow_SSSE3(src_argb + src_stride_argb, row + kMaxStride, pix);
  ARGBToUVRow_SSSE3(row, kMaxStride, dst_u, dst_v, pix);
}

void BGRAToUVRow_SSSE3(const uint8* src_argb, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int pix) {
  SIMD_ALIGNED(uint8 row[kMaxStride * 2]);
  BGRAToARGBRow_SSSE3(src_argb, row, pix);
  BGRAToARGBRow_SSSE3(src_argb + src_stride_argb, row + kMaxStride, pix);
  ARGBToUVRow_SSSE3(row, kMaxStride, dst_u, dst_v, pix);
}
#endif

#ifdef HAS_REVERSE_ROW_SSSE3

// Shuffle table for reversing the bytes.
CONST uvec8 kShuffleReverse = {
  15u, 14u, 13u, 12u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u
};

void ReverseRow_SSSE3(const uint8* src, uint8* dst, int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
  "movdqa     %3,%%xmm5                        \n"
  "lea        -0x10(%0,%2,1),%0                \n"
  "1:                                          \n"
    "movdqa     (%0),%%xmm0                    \n"
    "lea        -0x10(%0),%0                   \n"
    "pshufb     %%xmm5,%%xmm0                  \n"
    "movdqa     %%xmm0,(%1)                    \n"
    "lea        0x10(%1),%1                    \n"
    "sub        $0x10,%2                       \n"
    "ja         1b                             \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(temp_width)  // %2
  : "m"(kShuffleReverse) // %3
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm5"
#endif
  );
}
#endif

#ifdef HAS_REVERSE_ROW_SSE2

void ReverseRow_SSE2(const uint8* src, uint8* dst, int width) {
  intptr_t temp_width = static_cast<intptr_t>(width);
  asm volatile (
  "lea        -0x10(%0,%2,1),%0                \n"
  "1:                                          \n"
    "movdqa     (%0),%%xmm0                    \n"
    "lea        -0x10(%0),%0                   \n"
    "movdqa     %%xmm0,%%xmm1                  \n"
    "psllw      $0x8,%%xmm0                    \n"
    "psrlw      $0x8,%%xmm1                    \n"
    "por        %%xmm1,%%xmm0                  \n"
    "pshuflw    $0x1b,%%xmm0,%%xmm0            \n"
    "pshufhw    $0x1b,%%xmm0,%%xmm0            \n"
    "pshufd     $0x4e,%%xmm0,%%xmm0            \n"
    "movdqa     %%xmm0,(%1)                    \n"
    "lea        0x10(%1),%1                    \n"
    "sub        $0x10,%2                       \n"
    "ja         1b                             \n"
  : "+r"(src),  // %0
    "+r"(dst),  // %1
    "+r"(temp_width)  // %2
  :
  : "memory", "cc"
#if defined(__SSE2__)
    , "xmm0", "xmm1"
#endif
  );
}
#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
