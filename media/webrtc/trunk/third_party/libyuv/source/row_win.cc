/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

// This module is for Visual C x86.
#if !defined(YUV_DISABLE_ASM) && defined(_M_IX86)

#ifdef HAS_ARGBTOYROW_SSSE3

// Constants for ARGB.
static const vec8 kARGBToY = {
  13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0
};

static const vec8 kARGBToU = {
  112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0
};

static const vec8 kARGBToV = {
  -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0,
};

// Constants for BGRA.
static const vec8 kBGRAToY = {
  0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13
};

static const vec8 kBGRAToU = {
  0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112
};

static const vec8 kBGRAToV = {
  0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18
};

// Constants for ABGR.
static const vec8 kABGRToY = {
  33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0, 33, 65, 13, 0
};

static const vec8 kABGRToU = {
  -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0, -38, -74, 112, 0
};

static const vec8 kABGRToV = {
  112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0, 112, -94, -18, 0
};

// Constants for RGBA.
static const vec8 kRGBAToY = {
  0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33, 0, 13, 65, 33
};

static const vec8 kRGBAToU = {
  0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38, 0, 112, -74, -38
};

static const vec8 kRGBAToV = {
  0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112, 0, -18, -94, 112
};

static const uvec8 kAddY16 = {
  16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u, 16u
};

static const uvec8 kAddUV128 = {
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u,
  128u, 128u, 128u, 128u, 128u, 128u, 128u, 128u
};

// Shuffle table for converting RGB24 to ARGB.
static const uvec8 kShuffleMaskRGB24ToARGB = {
  0u, 1u, 2u, 12u, 3u, 4u, 5u, 13u, 6u, 7u, 8u, 14u, 9u, 10u, 11u, 15u
};

// Shuffle table for converting RAW to ARGB.
static const uvec8 kShuffleMaskRAWToARGB = {
  2u, 1u, 0u, 12u, 5u, 4u, 3u, 13u, 8u, 7u, 6u, 14u, 11u, 10u, 9u, 15u
};

// Shuffle table for converting BGRA to ARGB.
static const uvec8 kShuffleMaskBGRAToARGB = {
  3u, 2u, 1u, 0u, 7u, 6u, 5u, 4u, 11u, 10u, 9u, 8u, 15u, 14u, 13u, 12u
};

// Shuffle table for converting ABGR to ARGB.
static const uvec8 kShuffleMaskABGRToARGB = {
  2u, 1u, 0u, 3u, 6u, 5u, 4u, 7u, 10u, 9u, 8u, 11u, 14u, 13u, 12u, 15u
};

// Shuffle table for converting RGBA to ARGB.
static const uvec8 kShuffleMaskRGBAToARGB = {
  1u, 2u, 3u, 0u, 5u, 6u, 7u, 4u, 9u, 10u, 11u, 8u, 13u, 14u, 15u, 12u
};

// Shuffle table for converting ARGB to RGBA.
static const uvec8 kShuffleMaskARGBToRGBA = {
  3u, 0u, 1u, 2u, 7u, 4u, 5u, 6u, 11u, 8u, 9u, 10u, 15u, 12u, 13u, 14u
};

// Shuffle table for converting ARGB to RGB24.
static const uvec8 kShuffleMaskARGBToRGB24 = {
  0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u, 128u, 128u, 128u, 128u
};

// Shuffle table for converting ARGB to RAW.
static const uvec8 kShuffleMaskARGBToRAW = {
  2u, 1u, 0u, 6u, 5u, 4u, 10u, 9u, 8u, 14u, 13u, 12u, 128u, 128u, 128u, 128u
};

__declspec(naked) __declspec(align(16))
void I400ToARGBRow_SSE2(const uint8* src_y, uint8* dst_argb, int pix) {
  __asm {
    mov        eax, [esp + 4]        // src_y
    mov        edx, [esp + 8]        // dst_argb
    mov        ecx, [esp + 12]       // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0xff000000
    pslld      xmm5, 24

    align      16
  convertloop:
    movq       xmm0, qword ptr [eax]
    lea        eax,  [eax + 8]
    punpcklbw  xmm0, xmm0
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm0
    punpckhwd  xmm1, xmm1
    por        xmm0, xmm5
    por        xmm1, xmm5
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx, [edx + 32]
    sub        ecx, 8
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void BGRAToARGBRow_SSSE3(const uint8* src_bgra, uint8* dst_argb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_bgra
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm5, kShuffleMaskBGRAToARGB
    sub       edx, eax

    align      16
 convertloop:
    movdqa    xmm0, [eax]
    pshufb    xmm0, xmm5
    sub       ecx, 4
    movdqa    [eax + edx], xmm0
    lea       eax, [eax + 16]
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ABGRToARGBRow_SSSE3(const uint8* src_abgr, uint8* dst_argb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_abgr
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm5, kShuffleMaskABGRToARGB
    sub       edx, eax

    align      16
 convertloop:
    movdqa    xmm0, [eax]
    pshufb    xmm0, xmm5
    sub       ecx, 4
    movdqa    [eax + edx], xmm0
    lea       eax, [eax + 16]
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGBAToARGBRow_SSSE3(const uint8* src_rgba, uint8* dst_argb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_rgba
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm5, kShuffleMaskRGBAToARGB
    sub       edx, eax

    align      16
 convertloop:
    movdqa    xmm0, [eax]
    pshufb    xmm0, xmm5
    sub       ecx, 4
    movdqa    [eax + edx], xmm0
    lea       eax, [eax + 16]
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToRGBARow_SSSE3(const uint8* src_argb, uint8* dst_rgba, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgba
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm5, kShuffleMaskARGBToRGBA
    sub       edx, eax

    align      16
 convertloop:
    movdqa    xmm0, [eax]
    pshufb    xmm0, xmm5
    sub       ecx, 4
    movdqa    [eax + edx], xmm0
    lea       eax, [eax + 16]
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGB24ToARGBRow_SSSE3(const uint8* src_rgb24, uint8* dst_argb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_rgb24
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    pcmpeqb   xmm5, xmm5       // generate mask 0xff000000
    pslld     xmm5, 24
    movdqa    xmm4, kShuffleMaskRGB24ToARGB

    align      16
 convertloop:
    movdqu    xmm0, [eax]
    movdqu    xmm1, [eax + 16]
    movdqu    xmm3, [eax + 32]
    lea       eax, [eax + 48]
    movdqa    xmm2, xmm3
    palignr   xmm2, xmm1, 8    // xmm2 = { xmm3[0:3] xmm1[8:15]}
    pshufb    xmm2, xmm4
    por       xmm2, xmm5
    palignr   xmm1, xmm0, 12   // xmm1 = { xmm3[0:7] xmm0[12:15]}
    pshufb    xmm0, xmm4
    movdqa    [edx + 32], xmm2
    por       xmm0, xmm5
    pshufb    xmm1, xmm4
    movdqa    [edx], xmm0
    por       xmm1, xmm5
    palignr   xmm3, xmm3, 4    // xmm3 = { xmm3[4:15]}
    pshufb    xmm3, xmm4
    movdqa    [edx + 16], xmm1
    por       xmm3, xmm5
    sub       ecx, 16
    movdqa    [edx + 48], xmm3
    lea       edx, [edx + 64]
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RAWToARGBRow_SSSE3(const uint8* src_raw, uint8* dst_argb,
                        int pix) {
__asm {
    mov       eax, [esp + 4]   // src_raw
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    pcmpeqb   xmm5, xmm5       // generate mask 0xff000000
    pslld     xmm5, 24
    movdqa    xmm4, kShuffleMaskRAWToARGB

    align      16
 convertloop:
    movdqu    xmm0, [eax]
    movdqu    xmm1, [eax + 16]
    movdqu    xmm3, [eax + 32]
    lea       eax, [eax + 48]
    movdqa    xmm2, xmm3
    palignr   xmm2, xmm1, 8    // xmm2 = { xmm3[0:3] xmm1[8:15]}
    pshufb    xmm2, xmm4
    por       xmm2, xmm5
    palignr   xmm1, xmm0, 12   // xmm1 = { xmm3[0:7] xmm0[12:15]}
    pshufb    xmm0, xmm4
    movdqa    [edx + 32], xmm2
    por       xmm0, xmm5
    pshufb    xmm1, xmm4
    movdqa    [edx], xmm0
    por       xmm1, xmm5
    palignr   xmm3, xmm3, 4    // xmm3 = { xmm3[4:15]}
    pshufb    xmm3, xmm4
    movdqa    [edx + 16], xmm1
    por       xmm3, xmm5
    sub       ecx, 16
    movdqa    [edx + 48], xmm3
    lea       edx, [edx + 64]
    jg        convertloop
    ret
  }
}

// pmul method to replicate bits.
// Math to replicate bits:
// (v << 8) | (v << 3)
// v * 256 + v * 8
// v * (256 + 8)
// G shift of 5 is incorporated, so shift is 5 + 8 and 5 + 3
// 20 instructions.
__declspec(naked) __declspec(align(16))
void RGB565ToARGBRow_SSE2(const uint8* src_rgb565, uint8* dst_argb,
                          int pix) {
__asm {
    mov       eax, 0x01080108  // generate multiplier to repeat 5 bits
    movd      xmm5, eax
    pshufd    xmm5, xmm5, 0
    mov       eax, 0x20802080  // multiplier shift by 5 and then repeat 6 bits
    movd      xmm6, eax
    pshufd    xmm6, xmm6, 0
    pcmpeqb   xmm3, xmm3       // generate mask 0xf800f800 for Red
    psllw     xmm3, 11
    pcmpeqb   xmm4, xmm4       // generate mask 0x07e007e0 for Green
    psllw     xmm4, 10
    psrlw     xmm4, 5
    pcmpeqb   xmm7, xmm7       // generate mask 0xff00ff00 for Alpha
    psllw     xmm7, 8

    mov       eax, [esp + 4]   // src_rgb565
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    sub       edx, eax
    sub       edx, eax

    align      16
 convertloop:
    movdqu    xmm0, [eax]   // fetch 8 pixels of bgr565
    movdqa    xmm1, xmm0
    movdqa    xmm2, xmm0
    pand      xmm1, xmm3    // R in upper 5 bits
    psllw     xmm2, 11      // B in upper 5 bits
    pmulhuw   xmm1, xmm5    // * (256 + 8)
    pmulhuw   xmm2, xmm5    // * (256 + 8)
    psllw     xmm1, 8
    por       xmm1, xmm2    // RB
    pand      xmm0, xmm4    // G in middle 6 bits
    pmulhuw   xmm0, xmm6    // << 5 * (256 + 4)
    por       xmm0, xmm7    // AG
    movdqa    xmm2, xmm1
    punpcklbw xmm1, xmm0
    punpckhbw xmm2, xmm0
    movdqa    [eax * 2 + edx], xmm1  // store 4 pixels of ARGB
    movdqa    [eax * 2 + edx + 16], xmm2  // store next 4 pixels of ARGB
    lea       eax, [eax + 16]
    sub       ecx, 8
    jg        convertloop
    ret
  }
}

// 24 instructions
__declspec(naked) __declspec(align(16))
void ARGB1555ToARGBRow_SSE2(const uint8* src_argb1555, uint8* dst_argb,
                            int pix) {
__asm {
    mov       eax, 0x01080108  // generate multiplier to repeat 5 bits
    movd      xmm5, eax
    pshufd    xmm5, xmm5, 0
    mov       eax, 0x42004200  // multiplier shift by 6 and then repeat 5 bits
    movd      xmm6, eax
    pshufd    xmm6, xmm6, 0
    pcmpeqb   xmm3, xmm3       // generate mask 0xf800f800 for Red
    psllw     xmm3, 11
    movdqa    xmm4, xmm3       // generate mask 0x03e003e0 for Green
    psrlw     xmm4, 6
    pcmpeqb   xmm7, xmm7       // generate mask 0xff00ff00 for Alpha
    psllw     xmm7, 8

    mov       eax, [esp + 4]   // src_argb1555
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    sub       edx, eax
    sub       edx, eax

    align      16
 convertloop:
    movdqu    xmm0, [eax]   // fetch 8 pixels of 1555
    movdqa    xmm1, xmm0
    movdqa    xmm2, xmm0
    psllw     xmm1, 1       // R in upper 5 bits
    psllw     xmm2, 11      // B in upper 5 bits
    pand      xmm1, xmm3
    pmulhuw   xmm2, xmm5    // * (256 + 8)
    pmulhuw   xmm1, xmm5    // * (256 + 8)
    psllw     xmm1, 8
    por       xmm1, xmm2    // RB
    movdqa    xmm2, xmm0
    pand      xmm0, xmm4    // G in middle 5 bits
    psraw     xmm2, 8       // A
    pmulhuw   xmm0, xmm6    // << 6 * (256 + 8)
    pand      xmm2, xmm7
    por       xmm0, xmm2    // AG
    movdqa    xmm2, xmm1
    punpcklbw xmm1, xmm0
    punpckhbw xmm2, xmm0
    movdqa    [eax * 2 + edx], xmm1  // store 4 pixels of ARGB
    movdqa    [eax * 2 + edx + 16], xmm2  // store next 4 pixels of ARGB
    lea       eax, [eax + 16]
    sub       ecx, 8
    jg        convertloop
    ret
  }
}

// 18 instructions.
__declspec(naked) __declspec(align(16))
void ARGB4444ToARGBRow_SSE2(const uint8* src_argb4444, uint8* dst_argb,
                            int pix) {
__asm {
    mov       eax, 0x0f0f0f0f  // generate mask 0x0f0f0f0f
    movd      xmm4, eax
    pshufd    xmm4, xmm4, 0
    movdqa    xmm5, xmm4       // 0xf0f0f0f0 for high nibbles
    pslld     xmm5, 4
    mov       eax, [esp + 4]   // src_argb4444
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // pix
    sub       edx, eax
    sub       edx, eax

    align      16
 convertloop:
    movdqu    xmm0, [eax]   // fetch 8 pixels of bgra4444
    movdqa    xmm2, xmm0
    pand      xmm0, xmm4    // mask low nibbles
    pand      xmm2, xmm5    // mask high nibbles
    movdqa    xmm1, xmm0
    movdqa    xmm3, xmm2
    psllw     xmm1, 4
    psrlw     xmm3, 4
    por       xmm0, xmm1
    por       xmm2, xmm3
    movdqa    xmm1, xmm0
    punpcklbw xmm0, xmm2
    punpckhbw xmm1, xmm2
    movdqa    [eax * 2 + edx], xmm0  // store 4 pixels of ARGB
    movdqa    [eax * 2 + edx + 16], xmm1  // store next 4 pixels of ARGB
    lea       eax, [eax + 16]
    sub       ecx, 8
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToRGB24Row_SSSE3(const uint8* src_argb, uint8* dst_rgb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgb
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm6, kShuffleMaskARGBToRGB24

    align      16
 convertloop:
    movdqa    xmm0, [eax]   // fetch 16 pixels of argb
    movdqa    xmm1, [eax + 16]
    movdqa    xmm2, [eax + 32]
    movdqa    xmm3, [eax + 48]
    lea       eax, [eax + 64]
    pshufb    xmm0, xmm6    // pack 16 bytes of ARGB to 12 bytes of RGB
    pshufb    xmm1, xmm6
    pshufb    xmm2, xmm6
    pshufb    xmm3, xmm6
    movdqa    xmm4, xmm1   // 4 bytes from 1 for 0
    psrldq    xmm1, 4      // 8 bytes from 1
    pslldq    xmm4, 12     // 4 bytes from 1 for 0
    movdqa    xmm5, xmm2   // 8 bytes from 2 for 1
    por       xmm0, xmm4   // 4 bytes from 1 for 0
    pslldq    xmm5, 8      // 8 bytes from 2 for 1
    movdqa    [edx], xmm0  // store 0
    por       xmm1, xmm5   // 8 bytes from 2 for 1
    psrldq    xmm2, 8      // 4 bytes from 2
    pslldq    xmm3, 4      // 12 bytes from 3 for 2
    por       xmm2, xmm3   // 12 bytes from 3 for 2
    movdqa    [edx + 16], xmm1   // store 1
    movdqa    [edx + 32], xmm2   // store 2
    lea       edx, [edx + 48]
    sub       ecx, 16
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToRAWRow_SSSE3(const uint8* src_argb, uint8* dst_rgb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgb
    mov       ecx, [esp + 12]  // pix
    movdqa    xmm6, kShuffleMaskARGBToRAW

    align      16
 convertloop:
    movdqa    xmm0, [eax]   // fetch 16 pixels of argb
    movdqa    xmm1, [eax + 16]
    movdqa    xmm2, [eax + 32]
    movdqa    xmm3, [eax + 48]
    lea       eax, [eax + 64]
    pshufb    xmm0, xmm6    // pack 16 bytes of ARGB to 12 bytes of RGB
    pshufb    xmm1, xmm6
    pshufb    xmm2, xmm6
    pshufb    xmm3, xmm6
    movdqa    xmm4, xmm1   // 4 bytes from 1 for 0
    psrldq    xmm1, 4      // 8 bytes from 1
    pslldq    xmm4, 12     // 4 bytes from 1 for 0
    movdqa    xmm5, xmm2   // 8 bytes from 2 for 1
    por       xmm0, xmm4   // 4 bytes from 1 for 0
    pslldq    xmm5, 8      // 8 bytes from 2 for 1
    movdqa    [edx], xmm0  // store 0
    por       xmm1, xmm5   // 8 bytes from 2 for 1
    psrldq    xmm2, 8      // 4 bytes from 2
    pslldq    xmm3, 4      // 12 bytes from 3 for 2
    por       xmm2, xmm3   // 12 bytes from 3 for 2
    movdqa    [edx + 16], xmm1   // store 1
    movdqa    [edx + 32], xmm2   // store 2
    lea       edx, [edx + 48]
    sub       ecx, 16
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToRGB565Row_SSE2(const uint8* src_argb, uint8* dst_rgb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgb
    mov       ecx, [esp + 12]  // pix
    pcmpeqb   xmm3, xmm3       // generate mask 0x0000001f
    psrld     xmm3, 27
    pcmpeqb   xmm4, xmm4       // generate mask 0x000007e0
    psrld     xmm4, 26
    pslld     xmm4, 5
    pcmpeqb   xmm5, xmm5       // generate mask 0xfffff800
    pslld     xmm5, 11

    align      16
 convertloop:
    movdqa    xmm0, [eax]   // fetch 4 pixels of argb
    movdqa    xmm1, xmm0    // B
    movdqa    xmm2, xmm0    // G
    pslld     xmm0, 8       // R
    psrld     xmm1, 3       // B
    psrld     xmm2, 5       // G
    psrad     xmm0, 16      // R
    pand      xmm1, xmm3    // B
    pand      xmm2, xmm4    // G
    pand      xmm0, xmm5    // R
    por       xmm1, xmm2    // BG
    por       xmm0, xmm1    // BGR
    packssdw  xmm0, xmm0
    lea       eax, [eax + 16]
    movq      qword ptr [edx], xmm0  // store 4 pixels of ARGB1555
    lea       edx, [edx + 8]
    sub       ecx, 4
    jg        convertloop
    ret
  }
}

// TODO(fbarchard): Improve sign extension/packing.
__declspec(naked) __declspec(align(16))
void ARGBToARGB1555Row_SSE2(const uint8* src_argb, uint8* dst_rgb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgb
    mov       ecx, [esp + 12]  // pix
    pcmpeqb   xmm4, xmm4       // generate mask 0x0000001f
    psrld     xmm4, 27
    movdqa    xmm5, xmm4       // generate mask 0x000003e0
    pslld     xmm5, 5
    movdqa    xmm6, xmm4       // generate mask 0x00007c00
    pslld     xmm6, 10
    pcmpeqb   xmm7, xmm7       // generate mask 0xffff8000
    pslld     xmm7, 15

    align      16
 convertloop:
    movdqa    xmm0, [eax]   // fetch 4 pixels of argb
    movdqa    xmm1, xmm0    // B
    movdqa    xmm2, xmm0    // G
    movdqa    xmm3, xmm0    // R
    psrad     xmm0, 16      // A
    psrld     xmm1, 3       // B
    psrld     xmm2, 6       // G
    psrld     xmm3, 9       // R
    pand      xmm0, xmm7    // A
    pand      xmm1, xmm4    // B
    pand      xmm2, xmm5    // G
    pand      xmm3, xmm6    // R
    por       xmm0, xmm1    // BA
    por       xmm2, xmm3    // GR
    por       xmm0, xmm2    // BGRA
    packssdw  xmm0, xmm0
    lea       eax, [eax + 16]
    movq      qword ptr [edx], xmm0  // store 4 pixels of ARGB1555
    lea       edx, [edx + 8]
    sub       ecx, 4
    jg        convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToARGB4444Row_SSE2(const uint8* src_argb, uint8* dst_rgb, int pix) {
__asm {
    mov       eax, [esp + 4]   // src_argb
    mov       edx, [esp + 8]   // dst_rgb
    mov       ecx, [esp + 12]  // pix
    pcmpeqb   xmm4, xmm4       // generate mask 0xf000f000
    psllw     xmm4, 12
    movdqa    xmm3, xmm4       // generate mask 0x00f000f0
    psrlw     xmm3, 8

    align      16
 convertloop:
    movdqa    xmm0, [eax]   // fetch 4 pixels of argb
    movdqa    xmm1, xmm0
    pand      xmm0, xmm3    // low nibble
    pand      xmm1, xmm4    // high nibble
    psrl      xmm0, 4
    psrl      xmm1, 8
    por       xmm0, xmm1
    packuswb  xmm0, xmm0
    lea       eax, [eax + 16]
    movq      qword ptr [edx], xmm0  // store 4 pixels of ARGB4444
    lea       edx, [edx + 8]
    sub       ecx, 4
    jg        convertloop
    ret
  }
}

// Convert 16 ARGB pixels (64 bytes) to 16 Y values.
__declspec(naked) __declspec(align(16))
void ARGBToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kARGBToY

    align      16
 convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToYRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kARGBToY

    align      16
 convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void BGRAToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kBGRAToY

    align      16
 convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void BGRAToYRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kBGRAToY

    align      16
 convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ABGRToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kABGRToY

    align      16
 convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ABGRToYRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kABGRToY

    align      16
 convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGBAToYRow_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kRGBAToY

    align      16
 convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGBAToYRow_Unaligned_SSSE3(const uint8* src_argb, uint8* dst_y, int pix) {
__asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_y */
    mov        ecx, [esp + 12]  /* pix */
    movdqa     xmm5, kAddY16
    movdqa     xmm4, kRGBAToY

    align      16
 convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    pmaddubsw  xmm2, xmm4
    pmaddubsw  xmm3, xmm4
    lea        eax, [eax + 64]
    phaddw     xmm0, xmm1
    phaddw     xmm2, xmm3
    psrlw      xmm0, 7
    psrlw      xmm2, 7
    packuswb   xmm0, xmm2
    paddb      xmm0, xmm5
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kARGBToU
    movdqa     xmm6, kARGBToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pavgb      xmm0, [eax + esi]
    pavgb      xmm1, [eax + esi + 16]
    pavgb      xmm2, [eax + esi + 32]
    pavgb      xmm3, [eax + esi + 48]
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ARGBToUVRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                 uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kARGBToU
    movdqa     xmm6, kARGBToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    movdqu     xmm4, [eax + esi]
    pavgb      xmm0, xmm4
    movdqu     xmm4, [eax + esi + 16]
    pavgb      xmm1, xmm4
    movdqu     xmm4, [eax + esi + 32]
    pavgb      xmm2, xmm4
    movdqu     xmm4, [eax + esi + 48]
    pavgb      xmm3, xmm4
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void BGRAToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kBGRAToU
    movdqa     xmm6, kBGRAToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pavgb      xmm0, [eax + esi]
    pavgb      xmm1, [eax + esi + 16]
    pavgb      xmm2, [eax + esi + 32]
    pavgb      xmm3, [eax + esi + 48]
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void BGRAToUVRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                 uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kBGRAToU
    movdqa     xmm6, kBGRAToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    movdqu     xmm4, [eax + esi]
    pavgb      xmm0, xmm4
    movdqu     xmm4, [eax + esi + 16]
    pavgb      xmm1, xmm4
    movdqu     xmm4, [eax + esi + 32]
    pavgb      xmm2, xmm4
    movdqu     xmm4, [eax + esi + 48]
    pavgb      xmm3, xmm4
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ABGRToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kABGRToU
    movdqa     xmm6, kABGRToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pavgb      xmm0, [eax + esi]
    pavgb      xmm1, [eax + esi + 16]
    pavgb      xmm2, [eax + esi + 32]
    pavgb      xmm3, [eax + esi + 48]
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void ABGRToUVRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                 uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kABGRToU
    movdqa     xmm6, kABGRToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    movdqu     xmm4, [eax + esi]
    pavgb      xmm0, xmm4
    movdqu     xmm4, [eax + esi + 16]
    pavgb      xmm1, xmm4
    movdqu     xmm4, [eax + esi + 32]
    pavgb      xmm2, xmm4
    movdqu     xmm4, [eax + esi + 48]
    pavgb      xmm3, xmm4
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGBAToUVRow_SSSE3(const uint8* src_argb0, int src_stride_argb,
                       uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kRGBAToU
    movdqa     xmm6, kRGBAToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]
    pavgb      xmm0, [eax + esi]
    pavgb      xmm1, [eax + esi + 16]
    pavgb      xmm2, [eax + esi + 32]
    pavgb      xmm3, [eax + esi + 48]
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void RGBAToUVRow_Unaligned_SSSE3(const uint8* src_argb0, int src_stride_argb,
                                 uint8* dst_u, uint8* dst_v, int width) {
__asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb
    mov        esi, [esp + 8 + 8]   // src_stride_argb
    mov        edx, [esp + 8 + 12]  // dst_u
    mov        edi, [esp + 8 + 16]  // dst_v
    mov        ecx, [esp + 8 + 20]  // pix
    movdqa     xmm7, kRGBAToU
    movdqa     xmm6, kRGBAToV
    movdqa     xmm5, kAddUV128
    sub        edi, edx             // stride from u to v

    align      16
 convertloop:
    /* step 1 - subsample 16x2 argb pixels to 8x1 */
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + 32]
    movdqu     xmm3, [eax + 48]
    movdqu     xmm4, [eax + esi]
    pavgb      xmm0, xmm4
    movdqu     xmm4, [eax + esi + 16]
    pavgb      xmm1, xmm4
    movdqu     xmm4, [eax + esi + 32]
    pavgb      xmm2, xmm4
    movdqu     xmm4, [eax + esi + 48]
    pavgb      xmm3, xmm4
    lea        eax,  [eax + 64]
    movdqa     xmm4, xmm0
    shufps     xmm0, xmm1, 0x88
    shufps     xmm4, xmm1, 0xdd
    pavgb      xmm0, xmm4
    movdqa     xmm4, xmm2
    shufps     xmm2, xmm3, 0x88
    shufps     xmm4, xmm3, 0xdd
    pavgb      xmm2, xmm4

    // step 2 - convert to U and V
    // from here down is very similar to Y code except
    // instead of 16 different pixels, its 8 pixels of U and 8 of V
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    pmaddubsw  xmm0, xmm7  // U
    pmaddubsw  xmm2, xmm7
    pmaddubsw  xmm1, xmm6  // V
    pmaddubsw  xmm3, xmm6
    phaddw     xmm0, xmm2
    phaddw     xmm1, xmm3
    psraw      xmm0, 8
    psraw      xmm1, 8
    packsswb   xmm0, xmm1
    paddb      xmm0, xmm5            // -> unsigned

    // step 3 - store 8 U and 8 V values
    sub        ecx, 16
    movlps     qword ptr [edx], xmm0 // U
    movhps     qword ptr [edx + edi], xmm0 // V
    lea        edx, [edx + 8]
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}
#endif  // HAS_ARGBTOYROW_SSSE3

#ifdef HAS_I422TOARGBROW_SSSE3

#define YG 74 /* static_cast<int8>(1.164 * 64 + 0.5) */

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

static const vec8 kUVToB = {
  UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB
};

static const vec8 kUVToR = {
  UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR
};

static const vec8 kUVToG = {
  UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG
};

static const vec8 kVUToB = {
  VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB, VB, UB,
};

static const vec8 kVUToR = {
  VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR, VR, UR,
};

static const vec8 kVUToG = {
  VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG, VG, UG,
};

static const vec16 kYToRgb = { YG, YG, YG, YG, YG, YG, YG, YG };
static const vec16 kYSub16 = { 16, 16, 16, 16, 16, 16, 16, 16 };
static const vec16 kUVBiasB = { BB, BB, BB, BB, BB, BB, BB, BB };
static const vec16 kUVBiasG = { BG, BG, BG, BG, BG, BG, BG, BG };
static const vec16 kUVBiasR = { BR, BR, BR, BR, BR, BR, BR, BR };

// TODO(fbarchard): NV12/NV21 fetch UV and use directly.
// TODO(fbarchard): Read that does half size on Y and treats 420 as 444.

// Read 8 UV from 411.
#define READYUV444 __asm {                                                     \
    __asm movq       xmm0, qword ptr [esi] /* U */                /* NOLINT */ \
    __asm movq       xmm1, qword ptr [esi + edi] /* V */          /* NOLINT */ \
    __asm lea        esi,  [esi + 8]                                           \
    __asm punpcklbw  xmm0, xmm1           /* UV */                             \
  }

// Read 4 UV from 422, upsample to 8 UV.
#define READYUV422 __asm {                                                     \
    __asm movd       xmm0, [esi]          /* U */                              \
    __asm movd       xmm1, [esi + edi]    /* V */                              \
    __asm lea        esi,  [esi + 4]                                           \
    __asm punpcklbw  xmm0, xmm1           /* UV */                             \
    __asm punpcklwd  xmm0, xmm0           /* UVUV (upsample) */                \
  }

// Read 2 UV from 411, upsample to 8 UV.
#define READYUV411 __asm {                                                     \
    __asm movd       xmm0, [esi]          /* U */                              \
    __asm movd       xmm1, [esi + edi]    /* V */                              \
    __asm lea        esi,  [esi + 2]                                           \
    __asm punpcklbw  xmm0, xmm1           /* UV */                             \
    __asm punpcklwd  xmm0, xmm0           /* UVUV (upsample) */                \
    __asm punpckldq  xmm0, xmm0           /* UVUV (upsample) */                \
  }

// Read 4 UV from NV12, upsample to 8 UV.
#define READNV12 __asm {                                                       \
    __asm movq       xmm0, qword ptr [esi] /* UV */               /* NOLINT */ \
    __asm lea        esi,  [esi + 8]                                           \
    __asm punpcklwd  xmm0, xmm0           /* UVUV (upsample) */                \
  }

// Convert 8 pixels: 8 UV and 8 Y.
#define YUVTORGB __asm {                                                       \
    /* Step 1: Find 4 UV contributions to 8 R,G,B values */                    \
    __asm movdqa     xmm1, xmm0                                                \
    __asm movdqa     xmm2, xmm0                                                \
    __asm pmaddubsw  xmm0, kUVToB        /* scale B UV */                      \
    __asm pmaddubsw  xmm1, kUVToG        /* scale G UV */                      \
    __asm pmaddubsw  xmm2, kUVToR        /* scale R UV */                      \
    __asm psubw      xmm0, kUVBiasB      /* unbias back to signed */           \
    __asm psubw      xmm1, kUVBiasG                                            \
    __asm psubw      xmm2, kUVBiasR                                            \
    /* Step 2: Find Y contribution to 8 R,G,B values */                        \
    __asm movq       xmm3, qword ptr [eax]                        /* NOLINT */ \
    __asm lea        eax, [eax + 8]                                            \
    __asm punpcklbw  xmm3, xmm4                                                \
    __asm psubsw     xmm3, kYSub16                                             \
    __asm pmullw     xmm3, kYToRgb                                             \
    __asm paddsw     xmm0, xmm3           /* B += Y */                         \
    __asm paddsw     xmm1, xmm3           /* G += Y */                         \
    __asm paddsw     xmm2, xmm3           /* R += Y */                         \
    __asm psraw      xmm0, 6                                                   \
    __asm psraw      xmm1, 6                                                   \
    __asm psraw      xmm2, 6                                                   \
    __asm packuswb   xmm0, xmm0           /* B */                              \
    __asm packuswb   xmm1, xmm1           /* G */                              \
    __asm packuswb   xmm2, xmm2           /* R */                              \
  }

// Convert 8 pixels: 8 VU and 8 Y.
#define YVUTORGB __asm {                                                       \
    /* Step 1: Find 4 UV contributions to 8 R,G,B values */                    \
    __asm movdqa     xmm1, xmm0                                                \
    __asm movdqa     xmm2, xmm0                                                \
    __asm pmaddubsw  xmm0, kVUToB        /* scale B UV */                      \
    __asm pmaddubsw  xmm1, kVUToG        /* scale G UV */                      \
    __asm pmaddubsw  xmm2, kVUToR        /* scale R UV */                      \
    __asm psubw      xmm0, kUVBiasB      /* unbias back to signed */           \
    __asm psubw      xmm1, kUVBiasG                                            \
    __asm psubw      xmm2, kUVBiasR                                            \
    /* Step 2: Find Y contribution to 8 R,G,B values */                        \
    __asm movq       xmm3, qword ptr [eax]                        /* NOLINT */ \
    __asm lea        eax, [eax + 8]                                            \
    __asm punpcklbw  xmm3, xmm4                                                \
    __asm psubsw     xmm3, kYSub16                                             \
    __asm pmullw     xmm3, kYToRgb                                             \
    __asm paddsw     xmm0, xmm3           /* B += Y */                         \
    __asm paddsw     xmm1, xmm3           /* G += Y */                         \
    __asm paddsw     xmm2, xmm3           /* R += Y */                         \
    __asm psraw      xmm0, 6                                                   \
    __asm psraw      xmm1, 6                                                   \
    __asm psraw      xmm2, 6                                                   \
    __asm packuswb   xmm0, xmm0           /* B */                              \
    __asm packuswb   xmm1, xmm1           /* G */                              \
    __asm packuswb   xmm2, xmm2           /* R */                              \
  }

// 8 pixels, dest aligned 16.
// 8 UV values, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void I444ToARGBRow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* argb_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV444
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

// 8 pixels, dest aligned 16.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void I422ToARGBRow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* argb_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

// 8 pixels, dest aligned 16.
// 2 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
// Similar to I420 but duplicate UV once more.
__declspec(naked) __declspec(align(16))
void I411ToARGBRow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* argb_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV411
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

// 8 pixels, dest aligned 16.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void NV12ToARGBRow_SSSE3(const uint8* y_buf,
                         const uint8* uv_buf,
                         uint8* argb_buf,
                         int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // Y
    mov        esi, [esp + 4 + 8]   // UV
    mov        edx, [esp + 4 + 12]  // argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READNV12
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        esi
    ret
  }
}

// 8 pixels, dest aligned 16.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void NV21ToARGBRow_SSSE3(const uint8* y_buf,
                         const uint8* uv_buf,
                         uint8* argb_buf,
                         int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // Y
    mov        esi, [esp + 4 + 8]   // VU
    mov        edx, [esp + 4 + 12]  // argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READNV12
    YVUTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        esi
    ret
  }
}

// 8 pixels, unaligned.
// 8 UV values, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void I444ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* argb_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV444
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqu     [edx], xmm0
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

// 8 pixels, unaligned.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void I422ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* argb_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqu     [edx], xmm0
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

// 8 pixels, unaligned.
// 2 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
// Similar to I420 but duplicate UV once more.
__declspec(naked) __declspec(align(16))
void I411ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* argb_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // argb
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV411
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqu     [edx], xmm0
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}


// 8 pixels, dest aligned 16.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void NV12ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* uv_buf,
                                   uint8* argb_buf,
                                   int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // Y
    mov        esi, [esp + 4 + 8]   // UV
    mov        edx, [esp + 4 + 12]  // argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READNV12
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqu     [edx], xmm0
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        esi
    ret
  }
}

// 8 pixels, dest aligned 16.
// 4 UV values upsampled to 8 UV, mixed with 8 Y producing 8 ARGB (32 bytes).
__declspec(naked) __declspec(align(16))
void NV21ToARGBRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* uv_buf,
                                   uint8* argb_buf,
                                   int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // Y
    mov        esi, [esp + 4 + 8]   // VU
    mov        edx, [esp + 4 + 12]  // argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READNV12
    YVUTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm0, xmm1           // BG
    punpcklbw  xmm2, xmm5           // RA
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm2           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm2           // BGRA next 4 pixels
    movdqu     [edx], xmm0
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToBGRARow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* bgra_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // bgra
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into BGRA
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    punpcklbw  xmm1, xmm0           // GB
    punpcklbw  xmm5, xmm2           // AR
    movdqa     xmm0, xmm5
    punpcklwd  xmm5, xmm1           // BGRA first 4 pixels
    punpckhwd  xmm0, xmm1           // BGRA next 4 pixels
    movdqa     [edx], xmm5
    movdqa     [edx + 16], xmm0
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToBGRARow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* bgra_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // bgra
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into BGRA
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    punpcklbw  xmm1, xmm0           // GB
    punpcklbw  xmm5, xmm2           // AR
    movdqa     xmm0, xmm5
    punpcklwd  xmm5, xmm1           // BGRA first 4 pixels
    punpckhwd  xmm0, xmm1           // BGRA next 4 pixels
    movdqu     [edx], xmm5
    movdqu     [edx + 16], xmm0
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToABGRRow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* abgr_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // abgr
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm2, xmm1           // RG
    punpcklbw  xmm0, xmm5           // BA
    movdqa     xmm1, xmm2
    punpcklwd  xmm2, xmm0           // RGBA first 4 pixels
    punpckhwd  xmm1, xmm0           // RGBA next 4 pixels
    movdqa     [edx], xmm2
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToABGRRow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* abgr_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // abgr
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into ARGB
    punpcklbw  xmm2, xmm1           // RG
    punpcklbw  xmm0, xmm5           // BA
    movdqa     xmm1, xmm2
    punpcklwd  xmm2, xmm0           // RGBA first 4 pixels
    punpckhwd  xmm1, xmm0           // RGBA next 4 pixels
    movdqu     [edx], xmm2
    movdqu     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToRGBARow_SSSE3(const uint8* y_buf,
                         const uint8* u_buf,
                         const uint8* v_buf,
                         uint8* rgba_buf,
                         int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // rgba
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into RGBA
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    punpcklbw  xmm1, xmm2           // GR
    punpcklbw  xmm5, xmm0           // AB
    movdqa     xmm0, xmm5
    punpcklwd  xmm5, xmm1           // RGBA first 4 pixels
    punpckhwd  xmm0, xmm1           // RGBA next 4 pixels
    movdqa     [edx], xmm5
    movdqa     [edx + 16], xmm0
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void I422ToRGBARow_Unaligned_SSSE3(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgba_buf,
                                   int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // Y
    mov        esi, [esp + 8 + 8]   // U
    mov        edi, [esp + 8 + 12]  // V
    mov        edx, [esp + 8 + 16]  // rgba
    mov        ecx, [esp + 8 + 20]  // width
    sub        edi, esi
    pxor       xmm4, xmm4

    align      16
 convertloop:
    READYUV422
    YUVTORGB

    // Step 3: Weave into RGBA
    pcmpeqb    xmm5, xmm5           // generate 0xffffffff for alpha
    punpcklbw  xmm1, xmm2           // GR
    punpcklbw  xmm5, xmm0           // AB
    movdqa     xmm0, xmm5
    punpcklwd  xmm5, xmm1           // RGBA first 4 pixels
    punpckhwd  xmm0, xmm1           // RGBA next 4 pixels
    movdqu     [edx], xmm5
    movdqu     [edx + 16], xmm0
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

#endif  // HAS_I422TOARGBROW_SSSE3

#ifdef HAS_YTOARGBROW_SSE2
__declspec(naked) __declspec(align(16))
void YToARGBRow_SSE2(const uint8* y_buf,
                     uint8* rgb_buf,
                     int width) {
  __asm {
    pcmpeqb    xmm4, xmm4           // generate mask 0xff000000
    pslld      xmm4, 24
    mov        eax,0x10001000
    movd       xmm3,eax
    pshufd     xmm3,xmm3,0
    mov        eax,0x012a012a
    movd       xmm2,eax
    pshufd     xmm2,xmm2,0
    mov        eax, [esp + 4]       // Y
    mov        edx, [esp + 8]       // rgb
    mov        ecx, [esp + 12]      // width

    align      16
 convertloop:
    // Step 1: Scale Y contribution to 8 G values. G = (y - 16) * 1.164
    movq       xmm0, qword ptr [eax]
    lea        eax, [eax + 8]
    punpcklbw  xmm0, xmm0           // Y.Y
    psubusw    xmm0, xmm3
    pmulhuw    xmm0, xmm2
    packuswb   xmm0, xmm0           // G

    // Step 2: Weave into ARGB
    punpcklbw  xmm0, xmm0           // GG
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm0           // BGRA first 4 pixels
    punpckhwd  xmm1, xmm1           // BGRA next 4 pixels
    por        xmm0, xmm4
    por        xmm1, xmm4
    movdqa     [edx], xmm0
    movdqa     [edx + 16], xmm1
    lea        edx,  [edx + 32]
    sub        ecx, 8
    jg         convertloop

    ret
  }
}
#endif  // HAS_YTOARGBROW_SSE2

#ifdef HAS_MIRRORROW_SSSE3

// Shuffle table for reversing the bytes.
static const uvec8 kShuffleMirror = {
  15u, 14u, 13u, 12u, 11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u
};

__declspec(naked) __declspec(align(16))
void MirrorRow_SSSE3(const uint8* src, uint8* dst, int width) {
__asm {
    mov       eax, [esp + 4]   // src
    mov       edx, [esp + 8]   // dst
    mov       ecx, [esp + 12]  // width
    movdqa    xmm5, kShuffleMirror
    lea       eax, [eax - 16]

    align      16
 convertloop:
    movdqa    xmm0, [eax + ecx]
    pshufb    xmm0, xmm5
    sub       ecx, 16
    movdqa    [edx], xmm0
    lea       edx, [edx + 16]
    jg        convertloop
    ret
  }
}
#endif  // HAS_MIRRORROW_SSSE3

#ifdef HAS_MIRRORROW_SSE2
// SSE2 version has movdqu so it can be used on unaligned buffers when SSSE3
// version can not.
__declspec(naked) __declspec(align(16))
void MirrorRow_SSE2(const uint8* src, uint8* dst, int width) {
__asm {
    mov       eax, [esp + 4]   // src
    mov       edx, [esp + 8]   // dst
    mov       ecx, [esp + 12]  // width
    lea       eax, [eax - 16]

    align      16
 convertloop:
    movdqu    xmm0, [eax + ecx]
    movdqa    xmm1, xmm0        // swap bytes
    psllw     xmm0, 8
    psrlw     xmm1, 8
    por       xmm0, xmm1
    pshuflw   xmm0, xmm0, 0x1b  // swap words
    pshufhw   xmm0, xmm0, 0x1b
    pshufd    xmm0, xmm0, 0x4e  // swap qwords
    sub       ecx, 16
    movdqu    [edx], xmm0
    lea       edx, [edx + 16]
    jg        convertloop
    ret
  }
}
#endif  // HAS_MIRRORROW_SSE2

#ifdef HAS_MIRRORROW_UV_SSSE3
// Shuffle table for reversing the bytes of UV channels.
static const uvec8 kShuffleMirrorUV = {
  14u, 12u, 10u, 8u, 6u, 4u, 2u, 0u, 15u, 13u, 11u, 9u, 7u, 5u, 3u, 1u
};

__declspec(naked) __declspec(align(16))
void MirrorRowUV_SSSE3(const uint8* src, uint8* dst_u, uint8* dst_v,
                       int width) {
  __asm {
    push      edi
    mov       eax, [esp + 4 + 4]   // src
    mov       edx, [esp + 4 + 8]   // dst_u
    mov       edi, [esp + 4 + 12]  // dst_v
    mov       ecx, [esp + 4 + 16]  // width
    movdqa    xmm1, kShuffleMirrorUV
    lea       eax, [eax + ecx * 2 - 16]
    sub       edi, edx

    align      16
 convertloop:
    movdqa    xmm0, [eax]
    lea       eax, [eax - 16]
    pshufb    xmm0, xmm1
    sub       ecx, 8
    movlpd    qword ptr [edx], xmm0
    movhpd    qword ptr [edx + edi], xmm0
    lea       edx, [edx + 8]
    jg        convertloop

    pop       edi
    ret
  }
}
#endif  // HAS_MIRRORROW_UV_SSSE3

#ifdef HAS_ARGBMIRRORROW_SSSE3

// Shuffle table for reversing the bytes.
static const uvec8 kARGBShuffleMirror = {
  12u, 13u, 14u, 15u, 8u, 9u, 10u, 11u, 4u, 5u, 6u, 7u, 0u, 1u, 2u, 3u
};

__declspec(naked) __declspec(align(16))
void ARGBMirrorRow_SSSE3(const uint8* src, uint8* dst, int width) {
__asm {
    mov       eax, [esp + 4]   // src
    mov       edx, [esp + 8]   // dst
    mov       ecx, [esp + 12]  // width
    movdqa    xmm5, kARGBShuffleMirror
    lea       eax, [eax - 16]

    align      16
 convertloop:
    movdqa    xmm0, [eax + ecx * 4]
    pshufb    xmm0, xmm5
    sub       ecx, 4
    movdqa    [edx], xmm0
    lea       edx, [edx + 16]
    jg        convertloop
    ret
  }
}
#endif  // HAS_ARGBMIRRORROW_SSSE3

#ifdef HAS_SPLITUV_SSE2
__declspec(naked) __declspec(align(16))
void SplitUV_SSE2(const uint8* src_uv, uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_uv
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    movdqa     xmm2, xmm0
    movdqa     xmm3, xmm1
    pand       xmm0, xmm5   // even bytes
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    psrlw      xmm2, 8      // odd bytes
    psrlw      xmm3, 8
    packuswb   xmm2, xmm3
    movdqa     [edx], xmm0
    movdqa     [edx + edi], xmm2
    lea        edx, [edx + 16]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    ret
  }
}
#endif  // HAS_SPLITUV_SSE2

#ifdef HAS_COPYROW_SSE2
// CopyRow copys 'count' bytes using a 16 byte load/store, 32 bytes at time.
__declspec(naked) __declspec(align(16))
void CopyRow_SSE2(const uint8* src, uint8* dst, int count) {
  __asm {
    mov        eax, [esp + 4]   // src
    mov        edx, [esp + 8]   // dst
    mov        ecx, [esp + 12]  // count
    sub        edx, eax

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     [eax + edx], xmm0
    movdqa     [eax + edx + 16], xmm1
    lea        eax, [eax + 32]
    sub        ecx, 32
    jg         convertloop
    ret
  }
}
#endif  // HAS_COPYROW_SSE2

#ifdef HAS_COPYROW_X86
__declspec(naked) __declspec(align(16))
void CopyRow_X86(const uint8* src, uint8* dst, int count) {
  __asm {
    mov        eax, esi
    mov        edx, edi
    mov        esi, [esp + 4]   // src
    mov        edi, [esp + 8]   // dst
    mov        ecx, [esp + 12]  // count
    shr        ecx, 2
    rep movsd
    mov        edi, edx
    mov        esi, eax
    ret
  }
}
#endif  // HAS_COPYROW_X86

#ifdef HAS_YUY2TOYROW_SSE2
__declspec(naked) __declspec(align(16))
void YUY2ToYRow_SSE2(const uint8* src_yuy2,
                     uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_yuy2
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix
    pcmpeqb    xmm5, xmm5        // generate mask 0x00ff00ff
    psrlw      xmm5, 8

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // even bytes are Y
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void YUY2ToUVRow_SSE2(const uint8* src_yuy2, int stride_yuy2,
                      uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void YUY2ToUV422Row_SSE2(const uint8* src_yuy2,
                         uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_yuy2
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void YUY2ToYRow_Unaligned_SSE2(const uint8* src_yuy2,
                               uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_yuy2
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix
    pcmpeqb    xmm5, xmm5        // generate mask 0x00ff00ff
    psrlw      xmm5, 8

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // even bytes are Y
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void YUY2ToUVRow_Unaligned_SSE2(const uint8* src_yuy2, int stride_yuy2,
                                uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + esi]
    movdqu     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void YUY2ToUV422Row_Unaligned_SSE2(const uint8* src_yuy2,
                                   uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_yuy2
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8      // YUYV -> UVUV
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToYRow_SSE2(const uint8* src_uyvy,
                     uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_uyvy
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8    // odd bytes are Y
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    sub        ecx, 16
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToUVRow_SSE2(const uint8* src_uyvy, int stride_uyvy,
                      uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    pand       xmm0, xmm5   // UYVY -> UVUV
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToUV422Row_SSE2(const uint8* src_uyvy,
                         uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_yuy2
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // UYVY -> UVUV
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToYRow_Unaligned_SSE2(const uint8* src_uyvy,
                               uint8* dst_y, int pix) {
  __asm {
    mov        eax, [esp + 4]    // src_uyvy
    mov        edx, [esp + 8]    // dst_y
    mov        ecx, [esp + 12]   // pix

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    psrlw      xmm0, 8    // odd bytes are Y
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    sub        ecx, 16
    movdqu     [edx], xmm0
    lea        edx, [edx + 16]
    jg         convertloop
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToUVRow_Unaligned_SSE2(const uint8* src_uyvy, int stride_uyvy,
                                uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]    // src_yuy2
    mov        esi, [esp + 8 + 8]    // stride_yuy2
    mov        edx, [esp + 8 + 12]   // dst_u
    mov        edi, [esp + 8 + 16]   // dst_v
    mov        ecx, [esp + 8 + 20]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    movdqu     xmm2, [eax + esi]
    movdqu     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    pand       xmm0, xmm5   // UYVY -> UVUV
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    pop        esi
    ret
  }
}

__declspec(naked) __declspec(align(16))
void UYVYToUV422Row_Unaligned_SSE2(const uint8* src_uyvy,
                                   uint8* dst_u, uint8* dst_v, int pix) {
  __asm {
    push       edi
    mov        eax, [esp + 4 + 4]    // src_yuy2
    mov        edx, [esp + 4 + 8]    // dst_u
    mov        edi, [esp + 4 + 12]   // dst_v
    mov        ecx, [esp + 4 + 16]   // pix
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8
    sub        edi, edx

    align      16
  convertloop:
    movdqu     xmm0, [eax]
    movdqu     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5   // UYVY -> UVUV
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     xmm1, xmm0
    pand       xmm0, xmm5  // U
    packuswb   xmm0, xmm0
    psrlw      xmm1, 8     // V
    packuswb   xmm1, xmm1
    movq       qword ptr [edx], xmm0
    movq       qword ptr [edx + edi], xmm1
    lea        edx, [edx + 8]
    sub        ecx, 16
    jg         convertloop

    pop        edi
    ret
  }
}
#endif  // HAS_YUY2TOYROW_SSE2

#ifdef HAS_ARGBBLENDROW_SSE2
// Blend 8 pixels at a time.
__declspec(naked) __declspec(align(16))
void ARGBBlendRow_SSE2(const uint8* src_argb0, const uint8* src_argb1,
                       uint8* dst_argb, int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // src_argb0
    mov        esi, [esp + 4 + 8]   // src_argb1
    mov        edx, [esp + 4 + 12]  // dst_argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm7, xmm7       // generate constant 1
    psrlw      xmm7, 15
    pcmpeqb    xmm6, xmm6       // generate mask 0x00ff00ff
    psrlw      xmm6, 8
    pcmpeqb    xmm5, xmm5       // generate mask 0xff00ff00
    psllw      xmm5, 8
    pcmpeqb    xmm4, xmm4       // generate mask 0xff000000
    pslld      xmm4, 24

    sub        ecx, 1
    je         convertloop1     // only 1 pixel?
    jl         convertloop1b

    // 1 pixel loop until destination pointer is aligned.
  alignloop1:
    test       edx, 15          // aligned?
    je         alignloop1b
    movd       xmm3, [eax]
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        alignloop1

  alignloop1b:
    add        ecx, 1 - 4
    jl         convertloop4b

    // 4 pixel loop.
  convertloop4:
    movdqu     xmm3, [eax]      // src argb
    lea        eax, [eax + 16]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movdqu     xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movdqu     xmm1, [esi]      // _a_g
    lea        esi, [esi + 16]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jge        convertloop4

  convertloop4b:
    add        ecx, 4 - 1
    jl         convertloop1b

    // 1 pixel loop.
  convertloop1:
    movd       xmm3, [eax]      // src argb
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    psrlw      xmm3, 8          // alpha
    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
    pshuflw    xmm3, xmm3,0F5h
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        convertloop1

  convertloop1b:
    pop        esi
    ret
  }
}
#endif  // HAS_ARGBBLENDROW_SSE2

#ifdef HAS_ARGBBLENDROW_SSSE3
// Shuffle table for isolating alpha.
static const uvec8 kShuffleAlpha = {
  3u, 0x80, 3u, 0x80, 7u, 0x80, 7u, 0x80,
  11u, 0x80, 11u, 0x80, 15u, 0x80, 15u, 0x80
};
// Same as SSE2, but replaces:
//    psrlw      xmm3, 8          // alpha
//    pshufhw    xmm3, xmm3,0F5h  // 8 alpha words
//    pshuflw    xmm3, xmm3,0F5h
// with..
//    pshufb     xmm3, kShuffleAlpha // alpha
// Blend 8 pixels at a time.

__declspec(naked) __declspec(align(16))
void ARGBBlendRow_SSSE3(const uint8* src_argb0, const uint8* src_argb1,
                        uint8* dst_argb, int width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]   // src_argb0
    mov        esi, [esp + 4 + 8]   // src_argb1
    mov        edx, [esp + 4 + 12]  // dst_argb
    mov        ecx, [esp + 4 + 16]  // width
    pcmpeqb    xmm7, xmm7       // generate constant 1
    psrlw      xmm7, 15
    pcmpeqb    xmm6, xmm6       // generate mask 0x00ff00ff
    psrlw      xmm6, 8
    pcmpeqb    xmm5, xmm5       // generate mask 0xff00ff00
    psllw      xmm5, 8
    pcmpeqb    xmm4, xmm4       // generate mask 0xff000000
    pslld      xmm4, 24

    sub        ecx, 1
    je         convertloop1     // only 1 pixel?
    jl         convertloop1b

    // 1 pixel loop until destination pointer is aligned.
  alignloop1:
    test       edx, 15          // aligned?
    je         alignloop1b
    movd       xmm3, [eax]
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    pshufb     xmm3, kShuffleAlpha // alpha
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        alignloop1

  alignloop1b:
    add        ecx, 1 - 4
    jl         convertloop4b

    test       eax, 15          // unaligned?
    jne        convertuloop4
    test       esi, 15          // unaligned?
    jne        convertuloop4

    // 4 pixel loop.
  convertloop4:
    movdqa     xmm3, [eax]      // src argb
    lea        eax, [eax + 16]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movdqa     xmm2, [esi]      // _r_b
    pshufb     xmm3, kShuffleAlpha // alpha
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movdqa     xmm1, [esi]      // _a_g
    lea        esi, [esi + 16]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jge        convertloop4
    jmp        convertloop4b

    // 4 pixel unaligned loop.
  convertuloop4:
    movdqu     xmm3, [eax]      // src argb
    lea        eax, [eax + 16]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movdqu     xmm2, [esi]      // _r_b
    pshufb     xmm3, kShuffleAlpha // alpha
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movdqu     xmm1, [esi]      // _a_g
    lea        esi, [esi + 16]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 4
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    jge        convertuloop4

  convertloop4b:
    add        ecx, 4 - 1
    jl         convertloop1b

    // 1 pixel loop.
  convertloop1:
    movd       xmm3, [eax]      // src argb
    lea        eax, [eax + 4]
    movdqa     xmm0, xmm3       // src argb
    pxor       xmm3, xmm4       // ~alpha
    movd       xmm2, [esi]      // _r_b
    pshufb     xmm3, kShuffleAlpha // alpha
    pand       xmm2, xmm6       // _r_b
    paddw      xmm3, xmm7       // 256 - alpha
    pmullw     xmm2, xmm3       // _r_b * alpha
    movd       xmm1, [esi]      // _a_g
    lea        esi, [esi + 4]
    psrlw      xmm1, 8          // _a_g
    por        xmm0, xmm4       // set alpha to 255
    pmullw     xmm1, xmm3       // _a_g * alpha
    psrlw      xmm2, 8          // _r_b convert to 8 bits again
    paddusb    xmm0, xmm2       // + src argb
    pand       xmm1, xmm5       // a_g_ convert to 8 bits again
    paddusb    xmm0, xmm1       // + src argb
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        convertloop1

  convertloop1b:
    pop        esi
    ret
  }
}
#endif  // HAS_ARGBBLENDROW_SSSE3

#ifdef HAS_ARGBATTENUATE_SSE2
// Attenuate 4 pixels at a time.
// Aligned to 16 bytes.
__declspec(naked) __declspec(align(16))
void ARGBAttenuateRow_SSE2(const uint8* src_argb, uint8* dst_argb, int width) {
  __asm {
    mov        eax, [esp + 4]   // src_argb0
    mov        edx, [esp + 8]   // dst_argb
    mov        ecx, [esp + 12]  // width
    sub        edx, eax
    pcmpeqb    xmm4, xmm4       // generate mask 0xff000000
    pslld      xmm4, 24
    pcmpeqb    xmm5, xmm5       // generate mask 0x00ffffff
    psrld      xmm5, 8

    align      16
 convertloop:
    movdqa     xmm0, [eax]      // read 4 pixels
    punpcklbw  xmm0, xmm0       // first 2
    pshufhw    xmm2, xmm0,0FFh  // 8 alpha words
    pshuflw    xmm2, xmm2,0FFh
    pmulhuw    xmm0, xmm2       // rgb * a
    movdqa     xmm1, [eax]      // read 4 pixels
    punpckhbw  xmm1, xmm1       // next 2 pixels
    pshufhw    xmm2, xmm1,0FFh  // 8 alpha words
    pshuflw    xmm2, xmm2,0FFh
    pmulhuw    xmm1, xmm2       // rgb * a
    movdqa     xmm2, [eax]      // alphas
    psrlw      xmm0, 8
    pand       xmm2, xmm4
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    pand       xmm0, xmm5       // keep original alphas
    por        xmm0, xmm2
    sub        ecx, 4
    movdqa     [eax + edx], xmm0
    lea        eax, [eax + 16]
    jg         convertloop

    ret
  }
}
#endif  // HAS_ARGBATTENUATE_SSE2

#ifdef HAS_ARGBATTENUATEROW_SSSE3
// Shuffle table duplicating alpha.
static const uvec8 kShuffleAlpha0 = {
  3u, 3u, 3u, 3u, 3u, 3u, 128u, 128u, 7u, 7u, 7u, 7u, 7u, 7u, 128u, 128u,
};
static const uvec8 kShuffleAlpha1 = {
  11u, 11u, 11u, 11u, 11u, 11u, 128u, 128u,
  15u, 15u, 15u, 15u, 15u, 15u, 128u, 128u,
};
__declspec(naked) __declspec(align(16))
void ARGBAttenuateRow_SSSE3(const uint8* src_argb, uint8* dst_argb, int width) {
  __asm {
    mov        eax, [esp + 4]   // src_argb0
    mov        edx, [esp + 8]   // dst_argb
    mov        ecx, [esp + 12]  // width
    sub        edx, eax
    pcmpeqb    xmm3, xmm3       // generate mask 0xff000000
    pslld      xmm3, 24
    movdqa     xmm4, kShuffleAlpha0
    movdqa     xmm5, kShuffleAlpha1

    align      16
 convertloop:
    movdqa     xmm0, [eax]      // read 4 pixels
    pshufb     xmm0, xmm4       // isolate first 2 alphas
    movdqa     xmm1, [eax]      // read 4 pixels
    punpcklbw  xmm1, xmm1       // first 2 pixel rgbs
    pmulhuw    xmm0, xmm1       // rgb * a
    movdqa     xmm1, [eax]      // read 4 pixels
    pshufb     xmm1, xmm5       // isolate next 2 alphas
    movdqa     xmm2, [eax]      // read 4 pixels
    punpckhbw  xmm2, xmm2       // next 2 pixel rgbs
    pmulhuw    xmm1, xmm2       // rgb * a
    movdqa     xmm2, [eax]      // mask original alpha
    pand       xmm2, xmm3
    psrlw      xmm0, 8
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    por        xmm0, xmm2       // copy original alpha
    sub        ecx, 4
    movdqa     [eax + edx], xmm0
    lea        eax, [eax + 16]
    jg         convertloop

    ret
  }
}
#endif  // HAS_ARGBATTENUATEROW_SSSE3

#ifdef HAS_ARGBUNATTENUATEROW_SSE2
// Unattenuate 4 pixels at a time.
// Aligned to 16 bytes.
__declspec(naked) __declspec(align(16))
void ARGBUnattenuateRow_SSE2(const uint8* src_argb, uint8* dst_argb,
                             int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 8 + 4]   // src_argb0
    mov        edx, [esp + 8 + 8]   // dst_argb
    mov        ecx, [esp + 8 + 12]  // width
    sub        edx, eax
    pcmpeqb    xmm4, xmm4       // generate mask 0xff000000
    pslld      xmm4, 24

    align      16
 convertloop:
    movdqa     xmm0, [eax]      // read 4 pixels
    movzx      esi, byte ptr [eax + 3]  // first alpha
    movzx      edi, byte ptr [eax + 7]  // second alpha
    punpcklbw  xmm0, xmm0       // first 2
    movd       xmm2, dword ptr fixed_invtbl8[esi * 4]
    movd       xmm3, dword ptr fixed_invtbl8[edi * 4]
    pshuflw    xmm2, xmm2,0C0h  // first 4 inv_alpha words
    pshuflw    xmm3, xmm3,0C0h  // next 4 inv_alpha words
    movlhps    xmm2, xmm3
    pmulhuw    xmm0, xmm2       // rgb * a

    movdqa     xmm1, [eax]      // read 4 pixels
    movzx      esi, byte ptr [eax + 11]  // third alpha
    movzx      edi, byte ptr [eax + 15]  // forth alpha
    punpckhbw  xmm1, xmm1       // next 2
    movd       xmm2, dword ptr fixed_invtbl8[esi * 4]
    movd       xmm3, dword ptr fixed_invtbl8[edi * 4]
    pshuflw    xmm2, xmm2,0C0h  // first 4 inv_alpha words
    pshuflw    xmm3, xmm3,0C0h  // next 4 inv_alpha words
    movlhps    xmm2, xmm3
    pmulhuw    xmm1, xmm2       // rgb * a

    movdqa     xmm2, [eax]      // alphas
    pand       xmm2, xmm4
    packuswb   xmm0, xmm1
    por        xmm0, xmm2
    sub        ecx, 4
    movdqa     [eax + edx], xmm0
    lea        eax, [eax + 16]
    jg         convertloop
    pop        edi
    pop        esi
    ret
  }
}
#endif  // HAS_ARGBUNATTENUATEROW_SSE2

#ifdef HAS_ARGBGRAYROW_SSSE3
// Constant for ARGB color to gray scale: 0.11 * B + 0.59 * G + 0.30 * R
static const vec8 kARGBToGray = {
  14, 76, 38, 0, 14, 76, 38, 0, 14, 76, 38, 0, 14, 76, 38, 0
};

// Convert 8 ARGB pixels (64 bytes) to 8 Gray ARGB pixels.
__declspec(naked) __declspec(align(16))
void ARGBGrayRow_SSSE3(const uint8* src_argb, uint8* dst_argb, int width) {
  __asm {
    mov        eax, [esp + 4]   /* src_argb */
    mov        edx, [esp + 8]   /* dst_argb */
    mov        ecx, [esp + 12]  /* width */
    movdqa     xmm4, kARGBToGray
    sub        edx, eax

    align      16
 convertloop:
    movdqa     xmm0, [eax]  // G
    movdqa     xmm1, [eax + 16]
    pmaddubsw  xmm0, xmm4
    pmaddubsw  xmm1, xmm4
    phaddw     xmm0, xmm1
    psrlw      xmm0, 7
    packuswb   xmm0, xmm0   // 8 G bytes
    movdqa     xmm2, [eax]  // A
    movdqa     xmm3, [eax + 16]
    psrld      xmm2, 24
    psrld      xmm3, 24
    packuswb   xmm2, xmm3
    packuswb   xmm2, xmm2   // 8 A bytes
    movdqa     xmm3, xmm0   // Weave into GG, GA, then GGGA
    punpcklbw  xmm0, xmm0   // 8 GG words
    punpcklbw  xmm3, xmm2   // 8 GA words
    movdqa     xmm1, xmm0
    punpcklwd  xmm0, xmm3   // GGGA first 4
    punpckhwd  xmm1, xmm3   // GGGA next 4
    sub        ecx, 8
    movdqa     [eax + edx], xmm0
    movdqa     [eax + edx + 16], xmm1
    lea        eax, [eax + 32]
    jg         convertloop
    ret
  }
}
#endif  // HAS_ARGBGRAYROW_SSSE3

#ifdef HAS_ARGBSEPIAROW_SSSE3
//    b = (r * 35 + g * 68 + b * 17) >> 7
//    g = (r * 45 + g * 88 + b * 22) >> 7
//    r = (r * 50 + g * 98 + b * 24) >> 7
// Constant for ARGB color to sepia tone.
static const vec8 kARGBToSepiaB = {
  17, 68, 35, 0, 17, 68, 35, 0, 17, 68, 35, 0, 17, 68, 35, 0
};

static const vec8 kARGBToSepiaG = {
  22, 88, 45, 0, 22, 88, 45, 0, 22, 88, 45, 0, 22, 88, 45, 0
};

static const vec8 kARGBToSepiaR = {
  24, 98, 50, 0, 24, 98, 50, 0, 24, 98, 50, 0, 24, 98, 50, 0
};

// Convert 8 ARGB pixels (32 bytes) to 8 Sepia ARGB pixels.
__declspec(naked) __declspec(align(16))
void ARGBSepiaRow_SSSE3(uint8* dst_argb, int width) {
  __asm {
    mov        eax, [esp + 4]   /* dst_argb */
    mov        ecx, [esp + 8]   /* width */
    movdqa     xmm2, kARGBToSepiaB
    movdqa     xmm3, kARGBToSepiaG
    movdqa     xmm4, kARGBToSepiaR

    align      16
 convertloop:
    movdqa     xmm0, [eax]  // B
    movdqa     xmm6, [eax + 16]
    pmaddubsw  xmm0, xmm2
    pmaddubsw  xmm6, xmm2
    phaddw     xmm0, xmm6
    psrlw      xmm0, 7
    packuswb   xmm0, xmm0   // 8 B values
    movdqa     xmm5, [eax]  // G
    movdqa     xmm1, [eax + 16]
    pmaddubsw  xmm5, xmm3
    pmaddubsw  xmm1, xmm3
    phaddw     xmm5, xmm1
    psrlw      xmm5, 7
    packuswb   xmm5, xmm5   // 8 G values
    punpcklbw  xmm0, xmm5   // 8 BG values
    movdqa     xmm5, [eax]  // R
    movdqa     xmm1, [eax + 16]
    pmaddubsw  xmm5, xmm4
    pmaddubsw  xmm1, xmm4
    phaddw     xmm5, xmm1
    psrlw      xmm5, 7
    packuswb   xmm5, xmm5   // 8 R values
    movdqa     xmm6, [eax]  // A
    movdqa     xmm1, [eax + 16]
    psrld      xmm6, 24
    psrld      xmm1, 24
    packuswb   xmm6, xmm1
    packuswb   xmm6, xmm6   // 8 A values
    punpcklbw  xmm5, xmm6   // 8 RA values
    movdqa     xmm1, xmm0   // Weave BG, RA together
    punpcklwd  xmm0, xmm5   // BGRA first 4
    punpckhwd  xmm1, xmm5   // BGRA next 4
    sub        ecx, 8
    movdqa     [eax], xmm0
    movdqa     [eax + 16], xmm1
    lea        eax, [eax + 32]
    jg         convertloop
    ret
  }
}
#endif  // HAS_ARGBSEPIAROW_SSSE3

#ifdef HAS_ARGBCOLORMATRIXROW_SSSE3
// Tranform 8 ARGB pixels (32 bytes) with color matrix.
// Same as Sepia except matrix is provided.
// TODO(fbarchard): packuswbs only use half of the reg.  To make RGBA, combine R
// and B into a high and low, then G/A, unpackl/hbw and then unpckl/hwd.
__declspec(naked) __declspec(align(16))
void ARGBColorMatrixRow_SSSE3(uint8* dst_argb, const int8* matrix_argb,
                              int width) {
  __asm {
    mov        eax, [esp + 4]   /* dst_argb */
    mov        edx, [esp + 8]   /* matrix_argb */
    mov        ecx, [esp + 12]  /* width */
    movd       xmm2, [edx]
    movd       xmm3, [edx + 4]
    movd       xmm4, [edx + 8]
    pshufd     xmm2, xmm2, 0
    pshufd     xmm3, xmm3, 0
    pshufd     xmm4, xmm4, 0

    align      16
 convertloop:
    movdqa     xmm0, [eax]  // B
    movdqa     xmm6, [eax + 16]
    pmaddubsw  xmm0, xmm2
    pmaddubsw  xmm6, xmm2
    movdqa     xmm5, [eax]  // G
    movdqa     xmm1, [eax + 16]
    pmaddubsw  xmm5, xmm3
    pmaddubsw  xmm1, xmm3
    phaddsw    xmm0, xmm6   // B
    phaddsw    xmm5, xmm1   // G
    psraw      xmm0, 7      // B
    psraw      xmm5, 7      // G
    packuswb   xmm0, xmm0   // 8 B values
    packuswb   xmm5, xmm5   // 8 G values
    punpcklbw  xmm0, xmm5   // 8 BG values
    movdqa     xmm5, [eax]  // R
    movdqa     xmm1, [eax + 16]
    pmaddubsw  xmm5, xmm4
    pmaddubsw  xmm1, xmm4
    phaddsw    xmm5, xmm1
    psraw      xmm5, 7
    packuswb   xmm5, xmm5   // 8 R values
    movdqa     xmm6, [eax]  // A
    movdqa     xmm1, [eax + 16]
    psrld      xmm6, 24
    psrld      xmm1, 24
    packuswb   xmm6, xmm1
    packuswb   xmm6, xmm6   // 8 A values
    movdqa     xmm1, xmm0   // Weave BG, RA together
    punpcklbw  xmm5, xmm6   // 8 RA values
    punpcklwd  xmm0, xmm5   // BGRA first 4
    punpckhwd  xmm1, xmm5   // BGRA next 4
    sub        ecx, 8
    movdqa     [eax], xmm0
    movdqa     [eax + 16], xmm1
    lea        eax, [eax + 32]
    jg         convertloop
    ret
  }
}
#endif  // HAS_ARGBCOLORMATRIXROW_SSSE3

#ifdef HAS_ARGBCOLORTABLEROW_X86
// Tranform ARGB pixels with color table.
__declspec(naked) __declspec(align(16))
void ARGBColorTableRow_X86(uint8* dst_argb, const uint8* table_argb,
                           int width) {
  __asm {
    push       ebx
    push       esi
    push       edi
    push       ebp
    mov        eax, [esp + 16 + 4]   /* dst_argb */
    mov        edi, [esp + 16 + 8]   /* table_argb */
    mov        ecx, [esp + 16 + 12]  /* width */
    xor        ebx, ebx
    xor        edx, edx

    align      16
 convertloop:
    mov        ebp, dword ptr [eax]  // BGRA
    mov        esi, ebp
    and        ebp, 255
    shr        esi, 8
    and        esi, 255
    mov        bl, [edi + ebp * 4 + 0]  // B
    mov        dl, [edi + esi * 4 + 1]  // G
    mov        ebp, dword ptr [eax]  // BGRA
    mov        esi, ebp
    shr        ebp, 16
    shr        esi, 24
    and        ebp, 255
    mov        [eax], bl
    mov        [eax + 1], dl
    mov        bl, [edi + ebp * 4 + 2]  // R
    mov        dl, [edi + esi * 4 + 3]  // A
    mov        [eax + 2], bl
    mov        [eax + 3], dl
    lea        eax, [eax + 4]
    sub        ecx, 1
    jg         convertloop
    pop        ebp
    pop        edi
    pop        esi
    pop        ebx
    ret
  }
}
#endif  // HAS_ARGBCOLORTABLEROW_X86

#ifdef HAS_ARGBQUANTIZEROW_SSE2
// Quantize 4 ARGB pixels (16 bytes).
// Aligned to 16 bytes.
__declspec(naked) __declspec(align(16))
void ARGBQuantizeRow_SSE2(uint8* dst_argb, int scale, int interval_size,
                          int interval_offset, int width) {
  __asm {
    mov        eax, [esp + 4]    /* dst_argb */
    movd       xmm2, [esp + 8]   /* scale */
    movd       xmm3, [esp + 12]  /* interval_size */
    movd       xmm4, [esp + 16]  /* interval_offset */
    mov        ecx, [esp + 20]   /* width */
    pshuflw    xmm2, xmm2, 040h
    pshufd     xmm2, xmm2, 044h
    pshuflw    xmm3, xmm3, 040h
    pshufd     xmm3, xmm3, 044h
    pshuflw    xmm4, xmm4, 040h
    pshufd     xmm4, xmm4, 044h
    pxor       xmm5, xmm5  // constant 0
    pcmpeqb    xmm6, xmm6  // generate mask 0xff000000
    pslld      xmm6, 24

    align      16
 convertloop:
    movdqa     xmm0, [eax]  // read 4 pixels
    punpcklbw  xmm0, xmm5   // first 2 pixels
    pmulhuw    xmm0, xmm2   // pixel * scale >> 16
    movdqa     xmm1, [eax]  // read 4 pixels
    punpckhbw  xmm1, xmm5   // next 2 pixels
    pmulhuw    xmm1, xmm2
    pmullw     xmm0, xmm3   // * interval_size
    movdqa     xmm7, [eax]  // read 4 pixels
    pmullw     xmm1, xmm3
    pand       xmm7, xmm6   // mask alpha
    paddw      xmm0, xmm4   // + interval_size / 2
    paddw      xmm1, xmm4
    packuswb   xmm0, xmm1
    por        xmm0, xmm7
    sub        ecx, 4
    movdqa     [eax], xmm0
    lea        eax, [eax + 16]
    jg         convertloop
    ret
  }
}
#endif  // HAS_ARGBQUANTIZEROW_SSE2

#ifdef HAS_CUMULATIVESUMTOAVERAGE_SSE2
// Consider float CumulativeSum.
// Consider calling CumulativeSum one row at time as needed.
// Consider circular CumulativeSum buffer of radius * 2 + 1 height.
// Convert cumulative sum for an area to an average for 1 pixel.
// topleft is pointer to top left of CumulativeSum buffer for area.
// botleft is pointer to bottom left of CumulativeSum buffer.
// width is offset from left to right of area in CumulativeSum buffer measured
//   in number of ints.
// area is the number of pixels in the area being averaged.
// dst points to pixel to store result to.
// count is number of averaged pixels to produce.
// Does 4 pixels at a time, requires CumulativeSum pointers to be 16 byte
// aligned.
void CumulativeSumToAverage_SSE2(const int32* topleft, const int32* botleft,
                                 int width, int area, uint8* dst, int count) {
  __asm {
    mov        eax, topleft  // eax topleft
    mov        esi, botleft  // esi botleft
    mov        edx, width
    movd       xmm4, area
    mov        edi, dst
    mov        ecx, count
    cvtdq2ps   xmm4, xmm4
    rcpss      xmm4, xmm4  // 1.0f / area
    pshufd     xmm4, xmm4, 0
    sub        ecx, 4
    jl         l4b

    // 4 pixel loop
    align      4
  l4:
    // top left
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + 32]
    movdqa     xmm3, [eax + 48]

    // - top right
    psubd      xmm0, [eax + edx * 4]
    psubd      xmm1, [eax + edx * 4 + 16]
    psubd      xmm2, [eax + edx * 4 + 32]
    psubd      xmm3, [eax + edx * 4 + 48]
    lea        eax, [eax + 64]

    // - bottom left
    psubd      xmm0, [esi]
    psubd      xmm1, [esi + 16]
    psubd      xmm2, [esi + 32]
    psubd      xmm3, [esi + 48]

    // + bottom right
    paddd      xmm0, [esi + edx * 4]
    paddd      xmm1, [esi + edx * 4 + 16]
    paddd      xmm2, [esi + edx * 4 + 32]
    paddd      xmm3, [esi + edx * 4 + 48]
    lea        esi, [esi + 64]

    cvtdq2ps   xmm0, xmm0   // Average = Sum * 1 / Area
    cvtdq2ps   xmm1, xmm1
    mulps      xmm0, xmm4
    mulps      xmm1, xmm4
    cvtdq2ps   xmm2, xmm2
    cvtdq2ps   xmm3, xmm3
    mulps      xmm2, xmm4
    mulps      xmm3, xmm4
    cvtps2dq   xmm0, xmm0
    cvtps2dq   xmm1, xmm1
    cvtps2dq   xmm2, xmm2
    cvtps2dq   xmm3, xmm3
    packssdw   xmm0, xmm1
    packssdw   xmm2, xmm3
    packuswb   xmm0, xmm2
    movdqu     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 4
    jge        l4

  l4b:
    add        ecx, 4 - 1
    jl         l1b

    // 1 pixel loop
    align      4
  l1:
    movdqa     xmm0, [eax]
    psubd      xmm0, [eax + edx * 4]
    lea        eax, [eax + 16]
    psubd      xmm0, [esi]
    paddd      xmm0, [esi + edx * 4]
    lea        esi, [esi + 16]
    cvtdq2ps   xmm0, xmm0
    mulps      xmm0, xmm4
    cvtps2dq   xmm0, xmm0
    packssdw   xmm0, xmm0
    packuswb   xmm0, xmm0
    movd       dword ptr [edi], xmm0
    lea        edi, [edi + 4]
    sub        ecx, 1
    jge        l1
  l1b:
  }
}
#endif  // HAS_CUMULATIVESUMTOAVERAGE_SSE2

#ifdef HAS_COMPUTECUMULATIVESUMROW_SSE2
// Creates a table of cumulative sums where each value is a sum of all values
// above and to the left of the value.
void ComputeCumulativeSumRow_SSE2(const uint8* row, int32* cumsum,
                                  const int32* previous_cumsum, int width) {
  __asm {
    mov        eax, row
    mov        edx, cumsum
    mov        esi, previous_cumsum
    mov        ecx, width
    sub        esi, edx
    pxor       xmm0, xmm0
    pxor       xmm1, xmm1

    sub        ecx, 4
    jl         l4b
    test       edx, 15
    jne        l4b

    // 4 pixel loop
    align      4
  l4:
    movdqu     xmm2, [eax]  // 4 argb pixels 16 bytes.
    lea        eax, [eax + 16]
    movdqa     xmm4, xmm2

    punpcklbw  xmm2, xmm1
    movdqa     xmm3, xmm2
    punpcklwd  xmm2, xmm1
    punpckhwd  xmm3, xmm1

    punpckhbw  xmm4, xmm1
    movdqa     xmm5, xmm4
    punpcklwd  xmm4, xmm1
    punpckhwd  xmm5, xmm1

    paddd      xmm0, xmm2
    movdqa     xmm2, [edx + esi]  // previous row above.
    paddd      xmm2, xmm0

    paddd      xmm0, xmm3
    movdqa     xmm3, [edx + esi + 16]
    paddd      xmm3, xmm0

    paddd      xmm0, xmm4
    movdqa     xmm4, [edx + esi + 32]
    paddd      xmm4, xmm0

    paddd      xmm0, xmm5
    movdqa     xmm5, [edx + esi + 48]
    paddd      xmm5, xmm0

    movdqa     [edx], xmm2
    movdqa     [edx + 16], xmm3
    movdqa     [edx + 32], xmm4
    movdqa     [edx + 48], xmm5

    lea        edx, [edx + 64]
    sub        ecx, 4
    jge        l4

  l4b:
    add        ecx, 4 - 1
    jl         l1b

    // 1 pixel loop
    align      4
  l1:
    movd       xmm2, dword ptr [eax]  // 1 argb pixel 4 bytes.
    lea        eax, [eax + 4]
    punpcklbw  xmm2, xmm1
    punpcklwd  xmm2, xmm1
    paddd      xmm0, xmm2
    movdqu     xmm2, [edx + esi]
    paddd      xmm2, xmm0
    movdqu     [edx], xmm2
    lea        edx, [edx + 16]
    sub        ecx, 1
    jge        l1

 l1b:
  }
}
#endif  // HAS_COMPUTECUMULATIVESUMROW_SSE2

#ifdef HAS_ARGBSHADE_SSE2
// Shade 4 pixels at a time by specified value.
// Aligned to 16 bytes.
__declspec(naked) __declspec(align(16))
void ARGBShadeRow_SSE2(const uint8* src_argb, uint8* dst_argb, int width,
                       uint32 value) {
  __asm {
    mov        eax, [esp + 4]   // src_argb
    mov        edx, [esp + 8]   // dst_argb
    mov        ecx, [esp + 12]  // width
    movd       xmm2, [esp + 16]  // value
    sub        edx, eax
    punpcklbw  xmm2, xmm2
    punpcklqdq xmm2, xmm2

    align      16
 convertloop:
    movdqa     xmm0, [eax]      // read 4 pixels
    movdqa     xmm1, xmm0
    punpcklbw  xmm0, xmm0       // first 2
    punpckhbw  xmm1, xmm1       // next 2
    pmulhuw    xmm0, xmm2       // argb * value
    pmulhuw    xmm1, xmm2       // argb * value
    psrlw      xmm0, 8
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    sub        ecx, 4
    movdqa     [eax + edx], xmm0
    lea        eax, [eax + 16]
    jg         convertloop

    ret
  }
}
#endif  // HAS_ARGBSHADE_SSE2

#ifdef HAS_ARGBAFFINEROW_SSE2
// Copy ARGB pixels from source image with slope to a row of destination.
__declspec(naked) __declspec(align(16))
LIBYUV_API
void ARGBAffineRow_SSE2(const uint8* src_argb, int src_argb_stride,
                        uint8* dst_argb, const float* uv_dudv, int width) {
  __asm {
    push       esi
    push       edi
    mov        eax, [esp + 12]   // src_argb
    mov        esi, [esp + 16]  // stride
    mov        edx, [esp + 20]  // dst_argb
    mov        ecx, [esp + 24]  // pointer to uv_dudv
    movq       xmm2, qword ptr [ecx]  // uv
    movq       xmm7, qword ptr [ecx + 8]  // dudv
    mov        ecx, [esp + 28]  // width
    shl        esi, 16          // 4, stride
    add        esi, 4
    movd       xmm5, esi
    sub        ecx, 4
    jl         l4b

    // setup for 4 pixel loop
    pshufd     xmm7, xmm7, 0x44  // dup dudv
    pshufd     xmm5, xmm5, 0  // dup 4, stride
    movdqa     xmm0, xmm2    // x0, y0, x1, y1
    addps      xmm0, xmm7
    movlhps    xmm2, xmm0
    movdqa     xmm4, xmm7
    addps      xmm4, xmm4    // dudv *= 2
    movdqa     xmm3, xmm2    // x2, y2, x3, y3
    addps      xmm3, xmm4
    addps      xmm4, xmm4    // dudv *= 4

    // 4 pixel loop
    align      4
  l4:
    cvttps2dq  xmm0, xmm2    // x, y float to int first 2
    cvttps2dq  xmm1, xmm3    // x, y float to int next 2
    packssdw   xmm0, xmm1    // x, y as 8 shorts
    pmaddwd    xmm0, xmm5    // offsets = x * 4 + y * stride.
    movd       esi, xmm0
    pshufd     xmm0, xmm0, 0x39  // shift right
    movd       edi, xmm0
    pshufd     xmm0, xmm0, 0x39  // shift right
    movd       xmm1, [eax + esi]  // read pixel 0
    movd       xmm6, [eax + edi]  // read pixel 1
    punpckldq  xmm1, xmm6     // combine pixel 0 and 1
    addps      xmm2, xmm4    // x, y += dx, dy first 2
    movq       qword ptr [edx], xmm1
    movd       esi, xmm0
    pshufd     xmm0, xmm0, 0x39  // shift right
    movd       edi, xmm0
    movd       xmm6, [eax + esi]  // read pixel 2
    movd       xmm0, [eax + edi]  // read pixel 3
    punpckldq  xmm6, xmm0     // combine pixel 2 and 3
    addps      xmm3, xmm4    // x, y += dx, dy next 2
    sub        ecx, 4
    movq       qword ptr 8[edx], xmm6
    lea        edx, [edx + 16]
    jge        l4

  l4b:
    add        ecx, 4 - 1
    jl         l1b

    // 1 pixel loop
    align      4
  l1:
    cvttps2dq  xmm0, xmm2    // x, y float to int
    packssdw   xmm0, xmm0    // x, y as shorts
    pmaddwd    xmm0, xmm5    // offset = x * 4 + y * stride
    addps      xmm2, xmm7    // x, y += dx, dy
    movd       esi, xmm0
    movd       xmm0, [eax + esi]  // copy a pixel
    sub        ecx, 1
    movd       [edx], xmm0
    lea        edx, [edx + 4]
    jge        l1
  l1b:
    pop        edi
    pop        esi
    ret
  }
}
#endif  // HAS_ARGBAFFINEROW_SSE2

// Bilinear row filtering combines 4x2 -> 4x1. SSSE3 version.
__declspec(naked) __declspec(align(16))
void ARGBInterpolateRow_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                              ptrdiff_t src_stride, int dst_width,
                              int source_y_fraction) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]   // dst_ptr
    mov        esi, [esp + 8 + 8]   // src_ptr
    mov        edx, [esp + 8 + 12]  // src_stride
    mov        ecx, [esp + 8 + 16]  // dst_width
    mov        eax, [esp + 8 + 20]  // source_y_fraction (0..255)
    sub        edi, esi
    shr        eax, 1
    cmp        eax, 0
    je         xloop1
    cmp        eax, 64
    je         xloop2
    movd       xmm0, eax  // high fraction 0..127
    neg        eax
    add        eax, 128
    movd       xmm5, eax  // low fraction 128..1
    punpcklbw  xmm5, xmm0
    punpcklwd  xmm5, xmm5
    pshufd     xmm5, xmm5, 0

    align      16
  xloop:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    movdqa     xmm1, xmm0
    punpcklbw  xmm0, xmm2
    punpckhbw  xmm1, xmm2
    pmaddubsw  xmm0, xmm5
    pmaddubsw  xmm1, xmm5
    psrlw      xmm0, 7
    psrlw      xmm1, 7
    packuswb   xmm0, xmm1
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop

    pop        edi
    pop        esi
    ret

    align      16
  xloop1:
    movdqa     xmm0, [esi]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop1

    pop        edi
    pop        esi
    ret

    align      16
  xloop2:
    movdqa     xmm0, [esi]
    pavgb      xmm0, [esi + edx]
    sub        ecx, 4
    movdqa     [esi + edi], xmm0
    lea        esi, [esi + 16]
    jg         xloop2

    pop        edi
    pop        esi
    ret
  }
}

#endif  // _M_IX86

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
